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

## Tests

```bash
./bin/app --script tests/cube_gpu_test.as --ticks 5
./bin/app --script tests/cube_load_test.as --ticks 5
./bin/app --script tests/cube_test.as --ticks 5
./bin/app --script tests/scene_load_test.as --ticks 5
./bin/app --script tests/triangle_gpu_test.as --ticks 5
./bin/app --script tests/triangle_test.as --ticks 5
./bin/app --script tests/two_windows_test.as --ticks 5
```

```bat
./bin/app.exe --script tests/cube_gpu_test.as --ticks 5
./bin/app.exe --script tests/cube_load_test.as --ticks 5
./bin/app.exe --script tests/cube_test.as --ticks 5
./bin/app.exe --script tests/scene_load_test.as --ticks 5
./bin/app.exe --script tests/triangle_gpu_test.as --ticks 5
./bin/app.exe --script tests/triangle_test.as --ticks 5
./bin/app.exe --script tests/two_windows_test.as --ticks 5
```

Or run the scripted test runners:

```bash
./scripts/run_tests.sh
```

```bat
scripts\run_tests.bat
```

Logs are written to `scripts/logs`.
