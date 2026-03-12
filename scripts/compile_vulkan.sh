#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo "Compiling shaders..."

SRC_DIR="shaders/vulkan"
OUT_DIR="bin/shaders"

mkdir -p "$OUT_DIR"

find "$SRC_DIR" -type f \( -name "*.vert" -o -name "*.frag" -o -name "*.comp" \) | while read -r file; do

    rel="${file#$SRC_DIR/}"
    out="$OUT_DIR/$rel.spv"

    mkdir -p "$(dirname "$out")"

    if [ ! -f "$out" ] || [ "$file" -nt "$out" ]; then
        echo "Compiling $file"
        glslc "$file" -o "$out"
    fi

done