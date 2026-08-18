# LUCUS ENGINE 2
An educational project, cross-platform (Linux-Vulkan, Mac-Metal, Win-DX12). Not for production.

## Project structure
- src/ — main code of engine
- shaders/ — GPU shaders (SLANG language)
- cmake/ - build
- scripts/ - bash/bat utilities for building, compiling, running
- content/scripts/ - AngelScript code for engine (main and tests)
- third-party/ - third-party dependencies for different platforms

## Backend status

| Feature | Vulkan (Linux) | DX12 (Win) | Metal (Mac) |
|---|---|---|---|
| Forward pass | yes | yes | yes |
| Shadow map pass | yes | yes | no (disabled) |
| Deferred / GBuffer | yes | yes | no |
| Bindless textures | yes | yes | no |

Metal was intentionally left behind at the `shadow_map` pass stage, maybe in the future.

## TODO LIST
### Done
- [x] Simple shadow pass (Vulkan, DX12)
- [x] Simple deferred pipeline (GBuffer + deferred lighting pass) — Vulkan, DX12
- [x] Bindless textures (Vulkan, DX12)
- [x] Separate render targets and framebuffers per pass
- [x] GBuffer handles moved to window contexts

### Current
- [ ] Fix dx12 render triangle upside down
- [ ] New sampler for shadow map (today one global `REPEAT` / `compareEnable=VK_FALSE` sampler is reused everywhere)
- [ ] Refactor work with handlers for renderer
- [ ] Fix Window context aspect ratio (frame uniform buffer is keyed by camera, not by camera+window, so with 2 windows of different size the last context wins)
- [ ] Add support for get render object from scene by name
- [ ] Add flag in render object to disable render on main pass
- [ ] Swapchain recreation on window resize (no resize callback, `vkAcquireNextImageKHR` / `vkQueuePresentKHR` results are ignored)

### Features
- [ ] Simple PBR (GBuffer B channel already reserved for Metalness/Roughness/Specular/ShadingModel, lighting is still Lambert)
- [ ] Early Z
- [ ] TAA like Inside

## Dependencies

### WIN
- Visual Studio + Toolchain + Cmake enabled
- Cmake
- Ninja
- LLVM > 20.0

### Mac
* I decided do not support mac on shadow_map pass stage, maybe in the future
```
brew install cmake
brew install ninja
```

### Linux
- Cmake >= 3.28
- Ninja
- Vulkan (loader + headers, e.g. `libvulkan-dev` / Vulkan SDK)
- Clang (default). GCC 14+ rejects `material.hpp` with `-Werror=changes-meaning`;
  build with `-DCMAKE_CXX_FLAGS=-Wno-changes-meaning` if you use GCC.

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

`slangc` must be on `PATH` when `scripts/compile_vulkan.sh` runs.

## Build

| Platform | Command |
|----------|---------|
| Linux | `./scripts/build_linux.sh` |
| macOS | `./scripts/build_mac.sh` |
| Windows | `scripts\build_win.bat` |

Output: `bin/app` (or `bin/app.exe`), `bin/shaders/`.
Note: `build_linux.sh` also compiles shaders, links `bin/content`, and then launches `./bin/app`.

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

Tests are smoke tests only: a test passes if the app exits 0 after N ticks.
Nothing checks the rendered image. Logs are written to `scripts/logs`.
