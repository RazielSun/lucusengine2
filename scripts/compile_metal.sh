#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo "Compiling shaders..."

SRC_DIR="shaders"
OUT_DIR="bin/shaders"

mkdir -p "$OUT_DIR"

find "$SRC_DIR" -type f \( -name "*.slang" \) | while read -r file; do

    rel="${file#$SRC_DIR/}"
    base="${rel%.slang}"

    out_vs="$OUT_DIR/$base.vert.metal"
    out_vs_air="$OUT_DIR/$base.vert.air"
    out_vs_lib="$OUT_DIR/$base.vert.metallib"

    out_ps="$OUT_DIR/$base.frag.metal"
    out_ps_air="$OUT_DIR/$base.frag.air"
    out_ps_lib="$OUT_DIR/$base.frag.metallib"

    mkdir -p "$(dirname "$out_vs")"

    if [ ! -f "$out_vs" ] || [ "$file" -nt "$out_vs" ]; then
        echo "Compiling $file to $out_vs"
        slangc "$file" -entry vsMain -profile metal -target metal -o  "$out_vs"
        xcrun -sdk macosx metal -c "$out_vs" -o "$out_vs_air"
        xcrun -sdk macosx metallib "$out_vs_air" -o "$out_vs_lib"
    fi

    if [ ! -f "$out_ps" ] || [ "$file" -nt "$out_ps" ]; then
        echo "Compiling $file to $out_ps"
        slangc "$file" -entry psMain -profile metal -target metal -o "$out_ps"
        xcrun -sdk macosx metal -c "$out_ps" -o "$out_ps_air"
        xcrun -sdk macosx metallib "$out_ps_air" -o "$out_ps_lib"
    fi

done