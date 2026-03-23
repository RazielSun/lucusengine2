# LUCUS ENGINE 2
An educational project.

## Dependencies

### WIN
- Visual Studio + Toolchain + Cmake enabled
- Cmake
- Ninja
- LLVM > 20.0

### Mac
```
brew install cmake
brew install ninja
```

### Linux
- Cmake
- Ninja
- Vulkan

## Instalation

- git clone
- cd lucusengine2
- git submodule update --init


## Shaders Compilations
Get SLANGC from https://github.com/shader-slang/slang/releases
e.x. Ubuntu Linux, put slang-2026.5-linux-x86_64-glibc-2.27.tar.gz to scripts/tools/slang

Get DXC for WIN SLANG compilation
https://github.com/microsoft/DirectXShaderCompiler/releases
to scripts/tools/dxc