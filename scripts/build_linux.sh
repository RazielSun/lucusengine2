#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo
. "$(dirname "$0")/.build.sh" -j"$(nproc)"
echo "Build done."

echo
. "$(dirname "$0")/compile_vulkan.sh"

echo
echo "Create Content symlink..."
ln -sfn "$ROOT/content" "$ROOT/bin/content"

echo
echo "Running app..."
./bin/app