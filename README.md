# LUCUS ENGINE 2
An educational project, cross-platform (Linux-Vulkan, Mac-Metal, Win-DX12). Not for production.

## Project structure
- src/ — main code of engine
- shaders/ — GPU shaders (SLANG language)
- cmake/ - build
- scripts/ - bash/bat utilities for building, compiling, running
- content/scripts/ - AngelScript code for engine (main and tests)
- third-party/ - third-party dependencies for different platforms

## TODO LIST
### Current
- [ ] Add support simple shadow for dx12
- [ ] New sampler for shadow map
- [ ] Refactor work with handlers for renderer
- [ ] Fix Window context aspect ratio
- [ ] Add support for get render object from scene by name
- [ ] Add flag in render object to disable render on main pass

### Features
- [ ] Simple Defered pipeline (GBuffer)
- [ ] Simple PBR
- [ ] Early Z
- [ ] Bindless textures
- [ ] TAA like Inside

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
```

```bat
./bin/app.exe --script tests/cube_gpu_test.as --ticks 5
```

Or run the scripted test runners:

```bash
./scripts/run_tests.sh
```

By default, `scripts/run_tests.sh` will discover all `*.as` files under
`bin/content/scripts/tests` and run them as `tests/...`.
If you still want an explicit list, set `TEST_LIST_FILE`:

```bash
TEST_LIST_FILE=./scripts/tests.list ./scripts/run_tests.sh
```

To run tests from a different folder, set `TEST_DIR`:

```bash
TEST_DIR=./bin/content/scripts/tests ./scripts/run_tests.sh
```

```bat
scripts\run_tests.bat
```

Logs are written to `scripts/logs`.
