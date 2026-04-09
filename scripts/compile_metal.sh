#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo "Compiling shaders..."

SRC_DIR="shaders"
OUT_DIR="bin/shaders"
COMMON_DIR="$SRC_DIR/common"
SLANGC="$(dirname "$0")/tools/slang/bin/slangc"

mkdir -p "$OUT_DIR"

needs_rebuild() {
    local src_file="$1"
    local out_file="$2"

    if [ ! -f "$out_file" ] || [ "$src_file" -nt "$out_file" ]; then
        return 0
    fi

    if [ -d "$COMMON_DIR" ] && [ -n "$(find "$COMMON_DIR" -type f -newer "$out_file" -print -quit)" ]; then
        return 0
    fi

    return 1
}

find "$SRC_DIR" -type f -name "*.slang" ! -path "*/common/*" | while read -r file; do

    rel="${file#$SRC_DIR/}"
    base="${rel%.slang}"

    out_mtl="$OUT_DIR/$base.metal"
    out_air="$OUT_DIR/$base.air"
    out_lib="$OUT_DIR/$base.metallib"

    mkdir -p "$(dirname "$out_mtl")"

    if needs_rebuild "$file" "$out_mtl"; then
        echo "Compiling $file to $out_lib"
        $SLANGC "$file" -target metal -entry vsMain -entry psMain -D TARGET_METAL=1 -o "$out_mtl" -I "$SRC_DIR"
        xcrun -sdk macosx metal -c "$out_mtl" -o "$out_air"
        xcrun -sdk macosx metallib "$out_air" -o "$out_lib"
    fi

done
