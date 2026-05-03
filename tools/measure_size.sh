#!/bin/bash
#
# LinCLI Flash/RAM size measurement — delta method
# Compares "with LinCLI" vs "without LinCLI" in the same STM32 HAL project.
#
# Usage:
#   ./tools/measure_size.sh              # default: no LTO
#   ./tools/measure_size.sh -O0          # override optimization
#   ./tools/measure_size.sh --lto        # enable LTO
#   ./tools/measure_size.sh --no-user    # disable user system
#   ./tools/measure_size.sh --no-env     # disable env system
#   ./tools/measure_size.sh --no-var     # disable var system
#   ./tools/measure_size.sh --no-advanced-completion  # disable advanced tab completion
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXAMPLE_DIR="${PROJECT_ROOT}/example_project/stm32g431_gcc_example_project"
BUILD_DIR="${EXAMPLE_DIR}/build"
CONFIG_H="${PROJECT_ROOT}/include/cli_config.h"

# --- defaults ---
OPT=""
LTO="--no-lto"
NO_USER=""
NO_ENV=""
NO_VAR=""
NO_ADV_CMP=""

# --- parse args ---
for arg in "$@"; do
	case "$arg" in
		-O0|-Os|-O1|-O2|-O3|-Oz)
			OPT="$arg"
			;;
		--lto)
			LTO=""
			;;
		--no-user)
			NO_USER=1
			;;
		--no-env)
			NO_ENV=1
			;;
		--no-var)
			NO_VAR=1
			;;
		--no-advanced-completion)
			NO_ADV_CMP=1
			;;
		-h|--help)
			cat <<'EOF'
LinCLI Flash/RAM size measurement — delta method

DESCRIPTION:
  Builds the stm32g431_gcc_example_project twice (with / without LinCLI)
  and prints the size delta. This gives the true cost of LinCLI alone.

USAGE:
  ./tools/measure_size.sh [OPTIONS]

OPTIMIZATION:
  -O0 | -Os | -O1 | -O2 | -O3 | -Oz   Override optimization level (default: Makefile default)

LTO:
  --lto                                Enable LTO (default: disabled)

FEATURE TRIMMING (all default to ENABLED, use --no-* to disable):

  --no-user                            Disable user management system
                                       Effect: removes login/su/permission checks
                                       Stub: current_user = root (always permitted)

  --no-env                             Disable environment variable expansion ($VAR)
                                       Effect: removes env command & built-in vars
                                       Stub: cli_env_replace() copies input verbatim

  --no-var                             Disable variable export system (CLI_VAR macros)
                                       Effect: removes var command & type registries
                                       Macros expand to nothing

  --no-advanced-completion             Disable advanced Tab completion
                                       Effect: keeps command-name prefix match + list
                                       Removes: option completion, value completion,
                                       candidate highlight cycling, completer tables

COMBINATION EXAMPLES:
  # Minimal build: disable all optional modules
  ./tools/measure_size.sh --no-user --no-env --no-var --no-advanced-completion

  # Check size with smallest optimization, no LTO
  ./tools/measure_size.sh -Os

  # Disable only env + advanced completion
  ./tools/measure_size.sh --no-env --no-advanced-completion
EOF
			exit 0
			;;
	esac
done

CC="arm-none-eabi-gcc"
SZ="arm-none-eabi-size"

if ! command -v "$CC" &>/dev/null; then
	echo "ERROR: ${CC} not found in PATH"
	exit 1
fi

if [ ! -d "$EXAMPLE_DIR" ]; then
	echo "ERROR: Example project not found: $EXAMPLE_DIR"
	exit 1
fi

# --- backup & override cli_config.h ---
if [ -n "$NO_USER" ] || [ -n "$NO_ENV" ] || [ -n "$NO_VAR" ] || [ -n "$NO_ADV_CMP" ]; then
	cp "$CONFIG_H" "${CONFIG_H}.orig"
	if [ -n "$NO_USER" ]; then
		sed -i -E 's/#define CLI_ENABLE_USER\s+1/#define CLI_ENABLE_USER 0/' "$CONFIG_H"
	fi
	if [ -n "$NO_ENV" ]; then
		sed -i -E 's/#define CLI_ENABLE_ENV\s+1/#define CLI_ENABLE_ENV 0/' "$CONFIG_H"
	fi
	if [ -n "$NO_VAR" ]; then
		sed -i -E 's/#define CLI_ENABLE_VAR\s+1/#define CLI_ENABLE_VAR 0/' "$CONFIG_H"
	fi
	if [ -n "$NO_ADV_CMP" ]; then
		sed -i -E 's/#define CLI_ENABLE_ADVANCED_COMPLETION\s+1/#define CLI_ENABLE_ADVANCED_COMPLETION 0/' "$CONFIG_H"
	fi
fi

cd "$EXAMPLE_DIR"

# ============================================
# 1. Build WITH LinCLI (full)
# ============================================
echo "========================================"
echo "  Building WITH LinCLI..."
echo "========================================"

make clean >/dev/null 2>&1 || true

# Override OPT / LTO if requested
if [ -n "$OPT" ] || [ -n "$LTO" ]; then
	# Save original Makefile
	cp Makefile Makefile.orig
	if [ -n "$OPT" ]; then
		sed -i "s/^OPT = .*/OPT = ${OPT}/" Makefile
	fi
	if [ -n "$LTO" ]; then
		sed -i 's/ -flto//g' Makefile
	fi
fi

make -j$(nproc) >/dev/null 2>&1 || make

FULL_ELF="${BUILD_DIR}/stm32g431_gcc_example_project.elf"
if [ ! -f "$FULL_ELF" ]; then
	echo "ERROR: Full build failed"
	exit 1
fi

FULL_TEXT=$($SZ "$FULL_ELF" | tail -1 | awk '{print $1}')
FULL_DATA=$($SZ "$FULL_ELF" | tail -1 | awk '{print $2}')
FULL_BSS=$($SZ "$FULL_ELF" | tail -1 | awk '{print $3}')

# ============================================
# 2. Build WITHOUT LinCLI (baseline)
# ============================================
echo ""
echo "========================================"
echo "  Building WITHOUT LinCLI (baseline)..."
echo "========================================"

# Create baseline Makefile: strip all LinCLI sources & includes
cp Makefile Makefile.baseline

# Remove LinCLI / init / lib / tests sources
sed -i '/\.\.\/\.\.\/lib\//d' Makefile.baseline
sed -i '/\.\.\/\.\.\/cli\/Src\//d' Makefile.baseline
sed -i '/\.\.\/\.\.\/init\//d' Makefile.baseline
sed -i '/\.\.\/\.\.\/tests\//d' Makefile.baseline

# Remove LinCLI include paths
sed -i '/-I\.\.\/\.\.\/cli\/Inc/d' Makefile.baseline
sed -i '/-I\.\.\/\.\.\/include/d' Makefile.baseline

# Use baseline main.c (no LinCLI)
cp "${PROJECT_ROOT}/tools/main_baseline.c" Core/Src/main_baseline.c
sed -i 's|Core/Src/main.c|Core/Src/main_baseline.c|' Makefile.baseline
sed -i 's|Core/Src/main.c|Core/Src/main_baseline.c|' Makefile.baseline

make -f Makefile.baseline clean >/dev/null 2>&1 || true
make -f Makefile.baseline -j$(nproc) >/dev/null 2>&1 || make -f Makefile.baseline

BASE_ELF="${BUILD_DIR}/stm32g431_gcc_example_project.elf"
if [ ! -f "$BASE_ELF" ]; then
	echo "ERROR: Baseline build failed"
	exit 1
fi

BASE_TEXT=$($SZ "$BASE_ELF" | tail -1 | awk '{print $1}')
BASE_DATA=$($SZ "$BASE_ELF" | tail -1 | awk '{print $2}')
BASE_BSS=$($SZ "$BASE_ELF" | tail -1 | awk '{print $3}')

# ============================================
# 3. Report delta
# ============================================
echo ""
echo "========================================"
echo "  LinCLI Size Report (delta method)"
echo "========================================"
echo ""
printf "  %-26s %10s %10s %10s\n" "" "text" "data" "bss"
printf "  %-26s %10s %10s %10s\n" "-------------------------" "----------" "----------" "----------"
printf "  %-26s %10d %10d %10d\n" "WITH LinCLI (full)" "$FULL_TEXT" "$FULL_DATA" "$FULL_BSS"
printf "  %-26s %10d %10d %10d\n" "WITHOUT LinCLI (baseline)" "$BASE_TEXT" "$BASE_DATA" "$BASE_BSS"
printf "  %-26s %10s %10s %10s\n" "-------------------------" "----------" "----------" "----------"
printf "  %-26s %10d %10d %10d\n" "DELTA (LinCLI only)" "$((FULL_TEXT - BASE_TEXT))" "$((FULL_DATA - BASE_DATA))" "$((FULL_BSS - BASE_BSS))"
echo ""

FLASH_DELTA=$((FULL_TEXT + FULL_DATA - BASE_TEXT - BASE_DATA))
RAM_DELTA=$((FULL_DATA + FULL_BSS - BASE_DATA - BASE_BSS))

echo "  Flash delta (text+data) : ${FLASH_DELTA} B  ($((FLASH_DELTA/1024)).$(((FLASH_DELTA%1024)*100/1024)) KB)"
echo "  RAM   delta (data+bss)  : ${RAM_DELTA} B  ($((RAM_DELTA/1024)).$(((RAM_DELTA%1024)*100/1024)) KB)"
echo ""

# ============================================
# 4. Cleanup
# ============================================
rm -f Makefile.baseline Core/Src/main_baseline.c
make clean >/dev/null 2>&1 || true
make -f Makefile.baseline clean >/dev/null 2>&1 || true
rm -f Makefile.baseline

# Restore original Makefile if it was modified
if [ -f Makefile.orig ]; then
	mv Makefile.orig Makefile
fi

# Restore original cli_config.h if it was modified
if [ -f "${CONFIG_H}.orig" ]; then
	mv "${CONFIG_H}.orig" "$CONFIG_H"
fi

echo "Done."
