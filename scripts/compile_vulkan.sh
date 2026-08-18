#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo "Compiling shaders..."

# Resolve slangc: explicit $SLANGC, then scripts/tools/slang/bin, then PATH.
if [ -n "${SLANGC:-}" ]; then
    :
elif [ -x "$(dirname "$0")/tools/slang/bin/slangc" ]; then
    SLANGC="$(dirname "$0")/tools/slang/bin/slangc"
elif command -v slangc >/dev/null 2>&1; then
    SLANGC="$(command -v slangc)"
else
    echo "slangc not found." >&2
    echo "Put a slang release in scripts/tools/slang (so that" >&2
    echo "scripts/tools/slang/bin/slangc exists), add slangc to PATH," >&2
    echo "or set SLANGC=/path/to/slangc. See README." >&2
    exit 1
fi
echo "Using slangc: $SLANGC"

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
        "$SLANGC" "$file" -entry vsMain -profile vs_6_0 -target spirv -D TARGET_VULKAN=1 -o "$out_vs" -I "$SRC_DIR"
    fi

    if needs_rebuild "$file" "$out_ps"; then
        echo "Compiling $file to $out_ps"
        "$SLANGC" "$file" -entry psMain -profile ps_6_0 -target spirv -D TARGET_VULKAN=1 -o "$out_ps" -I "$SRC_DIR"
    fi

done
