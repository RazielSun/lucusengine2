#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo "Compiling shaders..."

# find tools/slang -name slangc | while read -r slangc; do
#     export PATH="$(dirname "$slangc"):$PATH"
# done

SRC_DIR="shaders"
OUT_DIR="bin/shaders"

mkdir -p "$OUT_DIR"

find "$SRC_DIR" -type f \( -name "*.slang" \) | while read -r file; do

    rel="${file#$SRC_DIR/}"
    base="${rel%.slang}"

    out_vs="$OUT_DIR/$base.vert.spv"
    out_ps="$OUT_DIR/$base.frag.spv"

    mkdir -p "$(dirname "$out_vs")"

    if [ ! -f "$out_vs" ] || [ "$file" -nt "$out_vs" ]; then
        echo "Compiling $file to $out_vs"
        slangc "$file" -entry vsMain -profile vs_6_0 -target spirv -o  "$out_vs"
    fi

    if [ ! -f "$out_ps" ] || [ "$file" -nt "$out_ps" ]; then
        echo "Compiling $file to $out_ps"
        slangc "$file" -entry psMain -profile ps_6_0 -target spirv -o "$out_ps"
    fi

done