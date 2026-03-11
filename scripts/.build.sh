#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

# --- defaults ---
BUILD_TYPE="${BUILD_TYPE:-Debug}"         # Debug / Release
GENERATOR="${GENERATOR:-Ninja}"           # Ninja / "Unix Makefiles"
BUILD_DIR="${BUILD_DIR:-build}"             # output folder
INSTALL_DIR="${INSTALL_DIR:-_install}"    # optional install prefix

# compilers (optional)
CXX="${CXX:-clang++}"
CC="${CC:-clang}"

# extra cmake args passed from CLI
EXTRA_ARGS=("$@")

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE_DIR="$ROOT/cmake"

echo "== Lucus Build =="
echo "ROOT:        $ROOT"
echo "CMAKE_DIR:   $CMAKE_DIR"
echo "BUILD_DIR:   $BUILD_DIR"
echo "TYPE:        $BUILD_TYPE"
echo "GENERATOR:   $GENERATOR"
echo "CC/CXX:      $CC / $CXX"
echo

cmake -S "$CMAKE_DIR" -B "$ROOT/$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_INSTALL_PREFIX="$ROOT/$INSTALL_DIR" \
  "${EXTRA_ARGS[@]}"

cmake --build "$ROOT/$BUILD_DIR" -- -j"$(nproc)"