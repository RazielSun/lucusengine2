#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo "Compiling shaders..."

SRC_DIR="shaders/metal"
OUT_DIR="bin/shaders"

mkdir -p "$OUT_DIR"

find "$SRC_DIR" -type f \( -name "*.metal" \) | while read -r file; do

    rel="${file#$SRC_DIR/}"
    out_air="$OUT_DIR/$rel.air"
    out_lib="$OUT_DIR/$rel.metallib"

    mkdir -p "$(dirname "$out_air")"

    if [ ! -f "$out_air" ] || [ "$file" -nt "$out_air" ]; then
        echo "Compiling $file"
        xcrun -sdk macosx metal -c "$file" -o "$out_air"
        xcrun -sdk macosx metallib "$out_air" -o "$out_lib"
    fi

done