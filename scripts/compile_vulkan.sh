#!/bin/bash

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

echo "Compiling shaders..."

mkdir -p bin/shaders/vulkan
glslc shaders/vulkan/shader.vert -o bin/shaders/vert.spv
glslc shaders/vulkan/shader.frag -o bin/shaders/frag.spv