#!/bin/bash
#
# LinCLI Flash/RAM size measurement — unified script
# Usage: ./tools/measure_size.sh <defconfig_name>
# Example: ./tools/measure_size.sh mini_defconfig
#          ./tools/measure_size.sh max_defconfig
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXAMPLE_DIR="${PROJECT_ROOT}/examples/stm32_g431"
BASELINE_MAIN="${PROJECT_ROOT}/tools/main_baseline.c"
SZ="arm-none-eabi-size"

DEFCONFIG="${1:-}"

# --- argument check ---
if [ -z "$DEFCONFIG" ]; then
	echo "Usage: $0 <defconfig_name>"
	echo ""
	echo "Available defconfigs:"
	for f in "${PROJECT_ROOT}/configs"/*_defconfig; do
		[ -f "$f" ] && echo "  $(basename "$f")"
	done
	exit 1
fi

if [ ! -f "${PROJECT_ROOT}/configs/${DEFCONFIG}" ]; then
	echo "ERROR: configs/${DEFCONFIG} not found"
	exit 1
fi

if ! command -v arm-none-eabi-gcc &>/dev/null; then
	echo "ERROR: arm-none-eabi-gcc not found in PATH"
	exit 1
fi

# --- load defconfig, build PC target, sync headers ---
cd "$PROJECT_ROOT"
make "$DEFCONFIG" >/dev/null 2>&1
make -j$(nproc) >/dev/null 2>&1 || make >/dev/null 2>&1
make sync-kconfig >/dev/null 2>&1

CONFIG_LABEL="${DEFCONFIG%_defconfig}"

cd "$EXAMPLE_DIR"

# --- 1. Build WITH LinCLI ---
make clean >/dev/null 2>&1 || true
make -j$(nproc) >/dev/null 2>&1 || make

FULL_ELF="${EXAMPLE_DIR}/build/stm32g431_gcc_example_project.elf"
if [ ! -f "$FULL_ELF" ]; then
	echo "ERROR: Full build failed"
	exit 1
fi

FULL_TEXT=$($SZ "$FULL_ELF" | tail -1 | awk '{print $1}')
FULL_DATA=$($SZ "$FULL_ELF" | tail -1 | awk '{print $2}')
FULL_BSS=$($SZ "$FULL_ELF" | tail -1 | awk '{print $3}')

# --- 2. Build WITHOUT LinCLI (baseline) ---
cp Makefile Makefile.baseline

# Remove LinCLI / init / lib / tests / third_party sources
sed -i '/\.\.\/\.\.\/src\/lib\//d' Makefile.baseline
sed -i '/\.\.\/\.\.\/src\/cli\//d' Makefile.baseline
sed -i '/\.\.\/\.\.\/src\/init\//d' Makefile.baseline
sed -i '/\.\.\/\.\.\/src\/third_party\//d' Makefile.baseline
sed -i '/\.\.\/\.\.\/tests\/commands\//d' Makefile.baseline

# Remove LinCLI include paths
sed -i '/-I\.\.\/\.\.\/include/d' Makefile.baseline
sed -i '/-I\.\.\/\.\.\/src\/third_party/d' Makefile.baseline

# Use separate build dir to avoid collision with full build
sed -i 's/^BUILD_DIR = build$/BUILD_DIR = build_baseline/' Makefile.baseline

# Use baseline main.c (no LinCLI)
cp "$BASELINE_MAIN" Core/Src/main_baseline.c
sed -i 's|Core/Src/main\.c|Core/Src/main_baseline.c|g' Makefile.baseline

make -f Makefile.baseline clean >/dev/null 2>&1 || true
make -f Makefile.baseline -j$(nproc) >/dev/null 2>&1 || make -f Makefile.baseline

BASE_ELF="${EXAMPLE_DIR}/build_baseline/stm32g431_gcc_example_project.elf"
if [ ! -f "$BASE_ELF" ]; then
	echo "ERROR: Baseline build failed"
	exit 1
fi

BASE_TEXT=$($SZ "$BASE_ELF" | tail -1 | awk '{print $1}')
BASE_DATA=$($SZ "$BASE_ELF" | tail -1 | awk '{print $2}')
BASE_BSS=$($SZ "$BASE_ELF" | tail -1 | awk '{print $3}')

# --- 3. Report delta ---
echo ""
echo "========================================"
echo "  LinCLI Size Report (${CONFIG_LABEL})"
echo "========================================"
echo ""
printf "  %-30s %10s %10s %10s\n" "" "text" "data" "bss"
printf "  %-30s %10s %10s %10s\n" "------------------------------" "----------" "----------" "----------"
printf "  %-30s %10d %10d %10d\n" "WITH LinCLI (${CONFIG_LABEL})" "$FULL_TEXT" "$FULL_DATA" "$FULL_BSS"
printf "  %-30s %10d %10d %10d\n" "WITHOUT LinCLI (baseline)" "$BASE_TEXT" "$BASE_DATA" "$BASE_BSS"
printf "  %-30s %10s %10s %10s\n" "------------------------------" "----------" "----------" "----------"
printf "  %-30s %10d %10d %10d\n" "DELTA (LinCLI only)" "$((FULL_TEXT - BASE_TEXT))" "$((FULL_DATA - BASE_DATA))" "$((FULL_BSS - BASE_BSS))"
echo ""

FLASH_DELTA=$((FULL_TEXT + FULL_DATA - BASE_TEXT - BASE_DATA))
RAM_DELTA=$((FULL_DATA + FULL_BSS - BASE_DATA - BASE_BSS))

echo "  Flash delta (text+data) : ${FLASH_DELTA} B  ($((FLASH_DELTA/1024)).$(((FLASH_DELTA%1024)*100/1024)) KB)"
echo "  RAM   delta (data+bss)  : ${RAM_DELTA} B  ($((RAM_DELTA/1024)).$(((RAM_DELTA%1024)*100/1024)) KB)"
echo ""

# --- 4. Cleanup ---
rm -f Makefile.baseline Core/Src/main_baseline.c
make clean >/dev/null 2>&1 || true
make -f Makefile.baseline clean >/dev/null 2>&1 || true
rm -f Makefile.baseline

echo "Done."
