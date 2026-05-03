#!/bin/bash
#
# LinCLI Flash/RAM size measurement script
# Measures LinCLI core footprint WITHOUT HAL, tests, or example code.
#
# Usage:
#   ./tools/measure_size.sh              # default: -Os -flto
#   ./tools/measure_size.sh -O0          # no optimization
#   ./tools/measure_size.sh -O2 --no-lto # O2 without LTO
#   ./tools/measure_size.sh -d           # detailed per-module breakdown
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build_size_measure"

# --- defaults ---
OPT="-Os"
LTO="-flto"
MCU="-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard"
DETAIL=false

# --- parse args ---
for arg in "$@"; do
	case "$arg" in
		-O0|-Os|-O1|-O2|-O3|-Oz)
			OPT="$arg"
			;;
		--no-lto)
			LTO=""
			;;
		-d|--detail)
			DETAIL=true
			;;
		-h|--help)
			echo "Usage: $0 [-O0|-Os|-O1|-O2|-O3] [--no-lto] [-d|--detail]"
			exit 0
			;;
	esac
done

# --- source files (LinCLI core only) ---
CORE_SRCS=(
	"${PROJECT_ROOT}/cli/Src/cli_candidate.c"
	"${PROJECT_ROOT}/cli/Src/cli_cmd_line.c"
	"${PROJECT_ROOT}/cli/Src/cli_completion.c"
	"${PROJECT_ROOT}/cli/Src/cli_edit.c"
	"${PROJECT_ROOT}/cli/Src/cli_env.c"
	"${PROJECT_ROOT}/cli/Src/cli_history.c"
	"${PROJECT_ROOT}/cli/Src/cli_io.c"
	"${PROJECT_ROOT}/cli/Src/cli_logo.c"
	"${PROJECT_ROOT}/cli/Src/cli_parse.c"
	"${PROJECT_ROOT}/cli/Src/cli_user.c"
	"${PROJECT_ROOT}/cli/Src/cli_var.c"
	"${PROJECT_ROOT}/cli/Src/cmd_dispose.c"
	"${PROJECT_ROOT}/init/critical_default.c"
	"${PROJECT_ROOT}/init/init_d.c"
	"${PROJECT_ROOT}/init/scheduler.c"
	"${PROJECT_ROOT}/init/section_markers.c"
	"${PROJECT_ROOT}/lib/cli_atoi.c"
	"${PROJECT_ROOT}/lib/cli_errno.c"
	"${PROJECT_ROOT}/lib/cli_float.c"
	"${PROJECT_ROOT}/lib/cli_mpool.c"
	"${PROJECT_ROOT}/lib/cli_vsnprintf.c"
	"${PROJECT_ROOT}/lib/rbtree.c"
	"${PROJECT_ROOT}/lib/stateM.c"
	"${PROJECT_ROOT}/lib/tVector.c"
	"${PROJECT_ROOT}/tools/stub_main.c"
)

INCLUDES=(
	"-I${PROJECT_ROOT}/cli/Inc"
	"-I${PROJECT_ROOT}/include"
)

# Force test switches off regardless of cli_config.h state
OVERRIDE="-include ${PROJECT_ROOT}/tools/cli_config_override.h"

CFLAGS="${MCU} ${INCLUDES[@]} ${OPT} -Wall -fdata-sections -ffunction-sections ${OVERRIDE}"
[ -n "$LTO" ] && CFLAGS="${CFLAGS} ${LTO}"

LDFLAGS="${MCU} -Wl,--gc-sections -specs=nano.specs"
[ -n "$LTO" ] && LDFLAGS="${LDFLAGS} ${LTO}"

CC="arm-none-eabi-gcc"
SIZE="arm-none-eabi-size"
NM="arm-none-eabi-nm"

# --- check toolchain ---
if ! command -v "$CC" &>/dev/null; then
	echo "ERROR: ${CC} not found in PATH"
	exit 1
fi

# --- prepare build dir ---
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# --- compile (single-step for correct LTO on bare-metal ARM) ---
echo "========================================"
echo "  LinCLI Size Measurement"
echo "========================================"
echo "  Compiler: $(${CC} --version | head -1)"
echo "  CFLAGS:   ${CFLAGS}"
echo "  LDFLAGS:  ${LDFLAGS}"
echo ""

ELF="${BUILD_DIR}/lincli_size.elf"
"${CC}" "${CORE_SRCS[@]}" ${CFLAGS} ${LDFLAGS} -o "${ELF}"

# --- size summary ---
echo "----------------------------------------"
echo "  arm-none-eabi-size (Flash = text+data)"
echo "----------------------------------------"
${SIZE} "${ELF}"
echo ""

# --- precise section breakdown ---
echo "----------------------------------------"
echo "  Section Breakdown (size -A)"
echo "----------------------------------------"
${SIZE} -A "${ELF}" | awk '
/section/ {next}
/\.text/   {t=$2}
/\.rodata/ {r=$2}
/\.data/   {d=$2}
/\.bss/    {b=$2}
END {
    flash = t + r + d
    ram   = d + b
    printf "  .text    %6d B\n", t
    printf "  .rodata  %6d B\n", r
    printf "  .data    %6d B\n", d
    printf "  .bss     %6d B\n", b
    printf "  -------------------------\n"
    printf "  Flash    %6d B  (%d.%02d KB)\n", flash, flash/1024, (flash%1024)*100/1024
    printf "  RAM      %6d B  (%d.%02d KB)\n", ram,   ram/1024,   (ram%1024)*100/1024
}'
echo ""

# --- per-module breakdown (compile each file individually, no LTO) ---
if [ "$DETAIL" = true ]; then
	echo "----------------------------------------"
	echo "  Per-Module Breakdown (obj size, no LTO)"
	echo "----------------------------------------"
	
	TMP_DIR="${BUILD_DIR}/detail"
	mkdir -p "$TMP_DIR"
	
	# Compile without LTO for per-file accuracy
	CFLAGS_NO_LTO="${MCU} ${INCLUDES[@]} ${OPT} -Wall -fdata-sections -ffunction-sections ${OVERRIDE}"
	
	TOTAL_OBJ=0
	for src in "${CORE_SRCS[@]}"; do
		base=$(basename "$src" .c)
		obj="${TMP_DIR}/${base}.o"
		"${CC}" -c ${CFLAGS_NO_LTO} "$src" -o "$obj" 2>/dev/null || true
		
		# size -A gives section sizes per object
		sz=$(${SIZE} -A "$obj" 2>/dev/null | awk '
			/\.text|\.rodata|\.data|\.bss/ {sum+=$2}
			END {print sum+0}
		')
		TOTAL_OBJ=$((TOTAL_OBJ + sz))
		printf "  %-28s %6d B\n" "${base}.c" "$sz"
	done
	echo "  ---------------------------  -------"
	printf "  %-28s %6d B\n" "Total (no LTO)" "$TOTAL_OBJ"
	echo ""
	
	# --- top symbols with LTO ---
	echo "----------------------------------------"
	echo "  Top 20 Symbols (LTO linked ELF)"
	echo "----------------------------------------"
	${NM} --print-size --size-sort --radix=d "${ELF}" 2>/dev/null | \
		grep -v ' [U] ' | tail -20 | \
		awk '{size=$2; name=$4; printf "  %8d B  %s\n", size, name}' || \
		echo "  (nm output unavailable)"
	echo ""
fi

# --- cleanup ---
rm -rf "${BUILD_DIR}"

echo "Done."
