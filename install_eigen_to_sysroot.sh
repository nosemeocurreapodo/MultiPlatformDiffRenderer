#!/usr/bin/env bash
# Install Eigen headers into a ZynqMP sysroot ($COMMON_IMAGE_ZYNQMP).
#
# What it does:
#   - Clones Eigen if not already present.
#   - Configures with CMake (header-only; no cross-compile needed).
#   - Installs headers into: $COMMON_IMAGE_ZYNQMP/usr/include/eigen3
#
# Usage:
#   export COMMON_IMAGE_ZYNQMP=/path/to/sysroot
#   ./install_eigen_to_sysroot.sh
#
# Optional environment variables:
#   EIGEN_REPO_URL=https://gitlab.com/libeigen/eigen.git
#   EIGEN_DIR=eigen
#   EIGEN_TAG=3.4.0              # set to a tag/branch/commit; empty = default branch
#   BUILD_DIR=build/eigen-host
#   JOBS=8
#   TOOLCHAIN_FILE=</path/to/toolchain.cmake>  # not required (Eigen is header-only)
#
set -euo pipefail

EIGEN_REPO_URL="${EIGEN_REPO_URL:-https://gitlab.com/libeigen/eigen.git}"
EIGEN_DIR="${EIGEN_DIR:-eigen}"
EIGEN_TAG="${EIGEN_TAG:-}"
BUILD_DIR="${BUILD_DIR:-build/eigen-host}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 4)}"

if [[ -z "${COMMON_IMAGE_ZYNQMP:-}" ]]; then
  echo "ERROR: COMMON_IMAGE_ZYNQMP is not set. Export it to your target sysroot path." >&2
  exit 1
fi
if [[ ! -d "$COMMON_IMAGE_ZYNQMP" ]]; then
  echo "ERROR: Sysroot directory does not exist: $COMMON_IMAGE_ZYNQMP" >&2
  exit 1
fi

INSTALL_PREFIX="$COMMON_IMAGE_ZYNQMP/usr"
INCLUDE_DIR="$INSTALL_PREFIX/include/eigen3"

# Ensure install prefix exists and is writable
mkdir -p "$INSTALL_PREFIX"
if ! (touch "$INSTALL_PREFIX/.writetest" 2>/dev/null); then
  echo "ERROR: No write permission in $INSTALL_PREFIX. Try running with appropriate permissions." >&2
  exit 1
else
  rm -f "$INSTALL_PREFIX/.writetest"
fi

# Clone or update Eigen
if [[ -d "$EIGEN_DIR/.git" ]]; then
  echo "[✓] Eigen already cloned at: $EIGEN_DIR"
  echo "[*] Fetching latest refs..."
  git -C "$EIGEN_DIR" fetch --all --tags --prune
else
  echo "[*] Cloning Eigen from $EIGEN_REPO_URL into $EIGEN_DIR ..."
  git clone "$EIGEN_REPO_URL" "$EIGEN_DIR"
fi

# Checkout requested tag/branch if provided
if [[ -n "$EIGEN_TAG" ]]; then
  echo "[*] Checking out $EIGEN_TAG ..."
  git -C "$EIGEN_DIR" checkout "$EIGEN_TAG"
fi

# Configure & install (header-only; build step is trivial)
mkdir -p "$BUILD_DIR"

CMAKE_ARGS=(
  -S "$EIGEN_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
  -DBUILD_TESTING=OFF
)

# Pass an optional toolchain if the user insists, though not required.
if [[ -n "${TOOLCHAIN_FILE:-}" ]]; then
  CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE")
fi

echo "[*] Configuring CMake ..."
cmake "${CMAKE_ARGS[@]}"

echo "[*] Installing Eigen headers into $INCLUDE_DIR ..."
cmake --build "$BUILD_DIR" --target install -- -j"$JOBS"

# Sanity check
if [[ -d "$INCLUDE_DIR/Eigen" || -d "$INCLUDE_DIR/unsupported" ]]; then
  echo "[✓] Eigen installed successfully."
  echo "    Headers located at: $INCLUDE_DIR"
else
  echo "WARNING: Install finished, but expected headers not found in $INCLUDE_DIR." >&2
fi

