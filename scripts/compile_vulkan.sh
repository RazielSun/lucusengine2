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
COMMON_DIR="$SRC_DIR/common"

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

    out_vs="$OUT_DIR/$base.vert.spv"
    out_ps="$OUT_DIR/$base.frag.spv"

    mkdir -p "$(dirname "$out_vs")"

    if needs_rebuild "$file" "$out_vs"; then
        echo "Compiling $file to $out_vs"
        slangc "$file" -entry vsMain -profile vs_6_0 -target spirv -D TARGET_VULKAN=1 -o "$out_vs" -I "$SRC_DIR"
    fi

    if needs_rebuild "$file" "$out_ps"; then
        echo "Compiling $file to $out_ps"
        slangc "$file" -entry psMain -profile ps_6_0 -target spirv -D TARGET_VULKAN=1 -o "$out_ps" -I "$SRC_DIR"
    fi

done
