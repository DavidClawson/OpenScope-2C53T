#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_DIR="${ROOT}/ghidra_project"
PROJECT_NAME="FNIRSI_2C53T"
PROGRAM_NAME="APP_2C53T_V1.2.0_251015.bin"
SCRIPT_PATH="${ROOT}/reverse_engineering/ghidra_scripts"
OUT_DIR="${ROOT}/reverse_engineering/analysis_v120"
GHIDRA_HEADLESS="${GHIDRA_HEADLESS:-/opt/homebrew/Cellar/ghidra/12.0.4/libexec/support/analyzeHeadless}"
GHIDRA_CACHE_DIR="${HOME}/Library/ghidra/ghidra_12.0.4_PUBLIC/osgi/compiled-bundles"

usage() {
  cat <<'EOF'
Usage:
  scripts/run_high_flash_ghidra_passes.sh [memory|pass1|pass1-force|pass2|pass2-force|all]

Outputs:
  reverse_engineering/analysis_v120/ghidra_memory_blocks_2026_04_08.txt
  reverse_engineering/analysis_v120/high_flash_pass1_*.c
  reverse_engineering/analysis_v120/high_flash_pass2_*.c

Environment overrides:
  GHIDRA_HEADLESS=/path/to/analyzeHeadless
EOF
}

clean_cache() {
  if [[ -d "${GHIDRA_CACHE_DIR}" ]]; then
    rm -rf "${GHIDRA_CACHE_DIR}"
  fi
}

run_headless() {
  local post_script="$1"
  shift
  "${GHIDRA_HEADLESS}" \
    "${PROJECT_DIR}" "${PROJECT_NAME}" \
    -process "${PROGRAM_NAME}" \
    -noanalysis \
    -scriptPath "${SCRIPT_PATH}" \
    -postScript "${post_script}" "$@"
}

run_memory() {
  clean_cache
  run_headless ListMemoryBlocks.java \
    "${OUT_DIR}/ghidra_memory_blocks_2026_04_08.txt"
}

run_pass1() {
  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass1_scope_tx_08004200_08004920.c" \
    "08004200-08004920"

  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass1_scope_table_0800C000_0800C120.c" \
    "0800C000-0800C120"

  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass1_scope_table_08014140_08014190.c" \
    "08014140-08014190"

  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass1_scope_decode_0802A490_0802A4E0.c" \
    "0802A490-0802A4E0"

  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass1_scope_decode_08033720_08033750.c" \
    "08033720-08033750"
}

run_pass1_force() {
  clean_cache
  run_headless ForceDecompile.java \
    "${OUT_DIR}/high_flash_pass1_force_scope_gap_functions.c" \
    "080041f8,080047cc,0800bff4,0800f5a8,0802a430,08033310"
}

run_pass2() {
  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass2_scope_resource_0802CFC0_0802D020.c" \
    "0802CFC0-0802D020"

  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass2_scope_resource_0802E800_0802E8B0.c" \
    "0802E800-0802E8B0"

  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass2_scope_resource_080157A0_08015830.c" \
    "080157A0-08015830"

  clean_cache
  run_headless DecompileRange.java \
    "${OUT_DIR}/high_flash_pass2_scope_resource_0800B100_0800B150.c" \
    "0800B100-0800B150"
}

run_pass2_force() {
  clean_cache
  run_headless ForceDecompile.java \
    "${OUT_DIR}/high_flash_pass2_force_resource_gap_functions.c" \
    "0800ae84,0800f5a8,0802cfbc,0802e7bc"
}

main() {
  local mode="${1:-all}"

  if [[ ! -x "${GHIDRA_HEADLESS}" ]]; then
    echo "ERROR: analyzeHeadless not found at ${GHIDRA_HEADLESS}" >&2
    exit 1
  fi

  mkdir -p "${OUT_DIR}"

  case "${mode}" in
    memory) run_memory ;;
    pass1) run_pass1 ;;
    pass1-force) run_pass1_force ;;
    pass2) run_pass2 ;;
    pass2-force) run_pass2_force ;;
    all)
      run_memory
      run_pass1
      run_pass1_force
      run_pass2
      run_pass2_force
      ;;
    -h|--help|help)
      usage
      ;;
    *)
      echo "ERROR: unknown mode: ${mode}" >&2
      usage >&2
      exit 1
      ;;
  esac
}

main "$@"
