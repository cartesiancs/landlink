#!/usr/bin/env bash
#
# Publish built firmware binaries into public/firmware/ for the web flasher.
#
# Expects PlatformIO builds to already exist under firmware/.pio/build/<env>/.
# Build them first with:
#   pio run -d firmware -e ttgo-t-beam-sx1262
#   pio run -d firmware -e xiao-esp32s3-wio-sx1262
#
# Usage:
#   script/publish.sh                                      # both targets, version auto-detected
#   script/publish.sh --version 0.1.0
#   script/publish.sh --target ttgo-t-beam-sx1262
#   script/publish.sh --target xiao-esp32s3-wio-sx1262
#
# Tested on macOS (bash 3.2 compatible).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PIO_BUILD_DIR="${REPO_ROOT}/firmware/.pio/build"
PUBLIC_DIR="${REPO_ROOT}/public/firmware"
PIO_INI="${REPO_ROOT}/firmware/platformio.ini"

ALL_TARGETS=("ttgo-t-beam-sx1262" "xiao-esp32s3-wio-sx1262")

usage() {
  cat <<EOF
Publish firmware binaries to public/firmware/ for the web flasher.

Usage:
  $(basename "$0") [--version VERSION] [--target TARGET]

Options:
  --version VERSION  Override version (default: read LL_FW_VERSION from platformio.ini).
  --target  TARGET   Publish only one target. Repeatable. One of:
                       ${ALL_TARGETS[*]}
                     Default: all targets.
  -h, --help         Show this message.
EOF
}

VERSION=""
TARGETS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version)
      [[ $# -ge 2 ]] || { echo "--version requires a value" >&2; exit 2; }
      VERSION="$2"; shift 2 ;;
    --target)
      [[ $# -ge 2 ]] || { echo "--target requires a value" >&2; exit 2; }
      TARGETS+=("$2"); shift 2 ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2 ;;
  esac
done

if [[ -z "${VERSION}" ]]; then
  VERSION="$(sed -n 's/.*-DLL_FW_VERSION=\\"\([^"]*\)\\".*/\1/p' "${PIO_INI}" | head -n1)"
  if [[ -z "${VERSION}" ]]; then
    echo "Could not auto-detect LL_FW_VERSION from ${PIO_INI}." >&2
    echo "Pass it explicitly: $(basename "$0") --version <version>" >&2
    exit 1
  fi
fi

if [[ ${#TARGETS[@]} -eq 0 ]]; then
  TARGETS=("${ALL_TARGETS[@]}")
fi

# Map target -> public/firmware/ subdir. Must stay in sync with downloadUrl
# values in public/firmware/releases.json.
dest_subdir_for_target() {
  case "$1" in
    "ttgo-t-beam-sx1262")      echo "${VERSION}" ;;
    "xiao-esp32s3-wio-sx1262") echo "${VERSION}-xiao" ;;
    *) echo "Unknown target: $1" >&2; return 1 ;;
  esac
}

# macOS stat flag for file size in bytes.
file_size_bytes() {
  stat -f%z "$1"
}

echo "Publishing firmware ${VERSION}"
echo

for target in "${TARGETS[@]}"; do
  src="${PIO_BUILD_DIR}/${target}"
  if [[ ! -d "${src}" ]]; then
    echo "Build directory missing: ${src}" >&2
    echo "Build it first: pio run -d firmware -e ${target}" >&2
    exit 1
  fi

  for bin in firmware.bin bootloader.bin partitions.bin; do
    if [[ ! -f "${src}/${bin}" ]]; then
      echo "Missing artifact: ${src}/${bin}" >&2
      echo "Re-build the target: pio run -d firmware -e ${target}" >&2
      exit 1
    fi
  done

  subdir="$(dest_subdir_for_target "${target}")"
  dest="${PUBLIC_DIR}/${subdir}"
  mkdir -p "${dest}"

  cp "${src}/firmware.bin"   "${dest}/firmware.bin"
  cp "${src}/bootloader.bin" "${dest}/bootloader.bin"
  cp "${src}/partitions.bin" "${dest}/partitions.bin"

  fw_size="$(file_size_bytes "${dest}/firmware.bin")"
  bl_size="$(file_size_bytes "${dest}/bootloader.bin")"
  pt_size="$(file_size_bytes "${dest}/partitions.bin")"

  printf "%s\n" "${target}"
  printf "  -> public/firmware/%s/\n" "${subdir}"
  printf "     firmware.bin   %10s bytes\n" "${fw_size}"
  printf "     bootloader.bin %10s bytes\n" "${bl_size}"
  printf "     partitions.bin %10s bytes\n" "${pt_size}"
  echo
done

cat <<'EOF'
Done. If any sizes changed, update the matching entry in
public/firmware/releases.json so the web flasher reports correct totals.
EOF
