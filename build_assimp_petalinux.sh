#!/usr/bin/env bash
# Build & install assimp for ZynqMP (aarch64) using Vitis 2024.2 and a specified sysroot.
# Requirements:
#   - Vitis at /tools/Xilinx/Vitis/2024.2 (override with VITIS_DIR)
#   - Sysroot path in $COMMON_IMAGE_ZYNQMP
#   - CMake toolchain file at ./cmake/aarch64-linux-gnu.cmake (override with TOOLCHAIN_FILE)
#
# Usage:
#   export COMMON_IMAGE_ZYNQMP=/path/to/your/sysroot
#   ./build_assimp_aarch64.sh
#
# Optional env vars:
#   VITIS_DIR=/tools/Xilinx/Vitis/2024.2
#   TOOLCHAIN_FILE=/abs/or/rel/path/to/cmake/aarch64-linux-gnu.cmake
#   ASSIMP_REPO_URL=https://github.com/assimp/assimp.git
#   ASSIMP_DIR=assimp
#   ASSIMP_TAG=v5.4.3    # or leave empty for default branch
#   BUILD_DIR=build/assimp-aarch64
#   INSTALL_PREFIX=$(pwd)/install/assimp-aarch64
#   JOBS=8               # parallel build jobs (defaults to CPU count)

set -euo pipefail

# --- Config ---
VITIS_DIR="${VITIS_DIR:-/tools/Xilinx/Vitis/2024.2}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-$(pwd)/cmake/aarch64-linux-gnu.cmake}"
ASSIMP_REPO_URL="${ASSIMP_REPO_URL:-https://github.com/assimp/assimp.git}"
ASSIMP_DIR="${ASSIMP_DIR:-assimp}"
ASSIMP_TAG="${ASSIMP_TAG:-}"
BUILD_DIR="${BUILD_DIR:-build/assimp-aarch64}"
INSTALL_PREFIX="${INSTALL_PREFIX:-$(pwd)/install/assimp-aarch64}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)}"

# --- Checks ---
if [[ -z "${COMMON_IMAGE_ZYNQMP:-}" ]]; then
  echo "ERROR: COMMON_IMAGE_ZYNQMP is not set. Export it to your target sysroot path." >&2
  exit 1
fi
if [[ ! -d "$COMMON_IMAGE_ZYNQMP" ]]; then
  echo "ERROR: Sysroot directory does not exist: $COMMON_IMAGE_ZYNQMP" >&2
  exit 1
fi
if [[ ! -f "$TOOLCHAIN_FILE" ]]; then
  echo "ERROR: CMake toolchain file not found: $TOOLCHAIN_FILE" >&2
  exit 1
fi

# Source Vitis settings if available (helps put cross tools on PATH)
if [[ -f "$VITIS_DIR/settings64.sh" ]]; then
  # shellcheck disable=SC1090
  source "$VITIS_DIR/settings64.sh"
else
  echo "WARNING: $VITIS_DIR/settings64.sh not found. Ensure your aarch64 cross-compiler is on PATH."
fi

# --- Clone assimp if needed ---
if [[ -d "$ASSIMP_DIR/.git" ]]; then
  echo "[✓] assimp already cloned at: $ASSIMP_DIR"
else
  echo "[*] Cloning assimp from $ASSIMP_REPO_URL into $ASSIMP_DIR ..."
  git clone "$ASSIMP_REPO_URL" "$ASSIMP_DIR"
  if [[ -n "$ASSIMP_TAG" ]]; then
    git -C "$ASSIMP_DIR" checkout "$ASSIMP_TAG"
  fi
fi

# --- Configure & build ---
mkdir -p "$BUILD_DIR" "$INSTALL_PREFIX"

echo "[*] Configuring CMake ..."
cmake -S "$ASSIMP_DIR" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_SYSROOT="$COMMON_IMAGE_ZYNQMP" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DASSIMP_BUILD_TESTS=OFF \
  -DASSIMP_BUILD_ASSIMP_TOOLS=OFF

echo "[*] Building & installing (jobs: $JOBS) ..."
cmake --build "$BUILD_DIR" --target install -- -j"$JOBS"

echo
echo "[✓] assimp installed to: $INSTALL_PREFIX"
echo "    CMake integration hint:"
echo "      cmake -S . -B build -DCMAKE_PREFIX_PATH=\"$INSTALL_PREFIX\" -DCMAKE_TOOLCHAIN_FILE=\"$TOOLCHAIN_FILE\" -DCMAKE_SYSROOT=\"$COMMON_IMAGE_ZYNQMP\""
