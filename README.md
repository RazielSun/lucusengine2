# LUCUS ENGINE 2
An educational project, cross-platform (Linux-Vulkan, Win-DX12, macOS-Metal in progress). Not for production.

## Project structure
- src/ — main code of engine
- shaders/ — GPU shaders (SLANG language)
- cmake/ - build
- scripts/ - bash/bat utilities for building, compiling, running
- content/scripts/ - AngelScript code for engine (main and tests)
- third-party/ - third-party dependencies for different platforms

## Backend status

| Feature | Vulkan (Linux) | DX12 (Win) | Metal (macOS) |
|---|---|---|---|
| Forward pass | yes | yes | in progress |
| Shadow map pass | yes | yes | no |
| Deferred / GBuffer | yes | yes | no (rejected explicitly) |
| Bindless textures | yes | yes | no |

Metal shaders are emitted as MSL by `slangc` and compiled at runtime with
`newLibraryWithSource:`, so macOS needs no Xcode — Command Line Tools are enough.
See `PLAN.md` S1.

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

### Linux
- Cmake >= 3.28
- Ninja
- Vulkan (loader + headers, e.g. `libvulkan-dev` / Vulkan SDK)
- Clang (default) or GCC 13+ — both build the project as-is.

### macOS
- Xcode Command Line Tools (`xcode-select --install`) — Apple clang, Metal/Cocoa/QuartzCore frameworks. Full Xcode is not needed.
- Cmake >= 3.28 (e.g. `brew install cmake` or `pip3 install cmake`)
- Ninja (e.g. `brew install ninja` or `pip3 install ninja`)
- SLANGC (see below) — the macOS release goes to `scripts/tools/slang`

Verified on macOS 12.7.6, Intel Iris 6100, CMake 4.4.4, slangc 2026.21.

## Instalation

- git clone
- cd lucusengine2
- git submodule update --init


## Shaders Compilations
Get SLANGC from https://github.com/shader-slang/slang/releases
e.x. Ubuntu Linux, put slang-2026.5-linux-x86_64-glibc-2.27.tar.gz to scripts/tools/slang

On macOS take the `macos-x86_64` or `macos-aarch64` archive and unpack it into
`scripts/tools/slang`, so that `scripts/tools/slang/bin/slangc` exists. If macOS
blocks the binary as downloaded from the internet:

```bash
xattr -dr com.apple.quarantine scripts/tools/slang
```

macOS shaders are not compiled ahead of time: `compile_metal.sh` emits `.metal`
source into `bin/shaders/`, the app compiles it at runtime.

Get DXC for WIN SLANG compilation
https://github.com/microsoft/DirectXShaderCompiler/releases
to scripts/tools/dxc

The shader scripts look for `slangc` in this order: `$SLANGC`, then
`scripts/tools/slang/bin/slangc`, then `PATH`. Any one of the three works:

```bash
SLANGC=/path/to/slangc ./scripts/compile_vulkan.sh
```

## Build

| Platform | Command |
|----------|---------|
| Linux | `./scripts/build_linux.sh` |
| Windows | `scripts\build_win.bat` |
| macOS | `./scripts/build_mac.sh` |

Output: `bin/app` (or `bin/app.exe`), `bin/shaders/`.
Note: `build_linux.sh` and `build_mac.sh` also compile shaders, link `bin/content`, and then launch `./bin/app`.

Build options are environment variables, e.g. `BUILD_TYPE=Release ./scripts/build_mac.sh`
(`BUILD_TYPE`, `GENERATOR`, `BUILD_DIR`, `CC`, `CXX`).

### Run on macOS

The default `content/scripts/main.as` uses deferred rendering, which Metal does
not support yet, so plain `./bin/app` — and the launch at the end of
`build_mac.sh` — exits with `Metal backend does not support deferred rendering yet`.
The build itself is fine. Run a forward-pass script instead, from the repo root:

```bash
./bin/app --script tests/cube_gpu_test.as --ticks 5 --no-validation
```

Metal has no validation through the engine yet (`VALIDATION_PLAN.md` V7), so pass
`--no-validation`. The Metal debug layer is enabled from the environment instead:

```bash
MTL_DEBUG_LAYER=1 MTL_DEBUG_LAYER_ERROR_MODE=assert ./bin/app --script tests/cube_gpu_test.as --ticks 5 --no-validation
```

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

On macOS the runner works too, but only with `VALIDATION=off`, and it stops at
the first failure. Deferred and bindless tests are expected to fail on Metal
(`PLAN.md` S1), so run the supported subset through a list:

```bash
cat > scripts/tests.list <<'EOF'
tests/cube_gpu_test.as
tests/cube_load_test.as
tests/cube_test.as
tests/scene_light_test.as
tests/scene_load_test.as
tests/triangle_gpu_test.as
tests/triangle_test.as
tests/two_windows_test.as
EOF
VALIDATION=off MTL_DEBUG_LAYER=1 ./scripts/run_tests.sh
```

Tests are smoke tests only: a test passes if the app exits 0 after N ticks.
Nothing checks the rendered image. Logs are written to `scripts/logs`.

## Screenshots

```bash
./bin/app --script tests/triangle_gpu_test.as --ticks 5 --screenshot=/tmp/frame.png --validation=on --validation-required
```

`--screenshot=<path>` captures the main window on tick N of `--ticks N`.
A positive tick count and a non-empty path are required. Relative output paths
are resolved against the process working directory; the parent directory must
already exist. Closing the window before tick N, an unsupported capture format
or surface, a GPU readback failure, or a PNG write failure makes the run fail.

Vulkan, DX12 and Metal implement the same synchronous capture contract:
the backend records a copy of the current backbuffer before presentation and
waits for that copy before returning CPU pixels from `dynamic_rhi::execute`.
`renderer::tick` routes an optional `image_capture*` to the main window; the
engine writes the returned image through the shared PNG writer. Without a
capture request, no readback buffer or extra capture wait is created.

Pixels are tightly packed RGBA8, with the top row first and alpha set to 255.
Backend row padding is removed and BGRA channels are reordered. Color bytes
are preserved without tone mapping or gamma conversion. Capture currently
supports RGBA8/BGRA8 UNORM and sRGB backbuffers, not HDR formats. Vulkan requires
surface support for `TRANSFER_SRC`; Metal enables drawable readback when
requested. Metal remains limited to its existing forward rendering support;
use a forward script and `--no-validation` there.

Capture waits and PNG encoding are diagnostic work and should be excluded
from performance measurements. PNG output alone does not establish image
correctness; compare its content against the scene's expected result.

`./bin/app --capture-selftest` checks pixel packing, channel order, invalid
inputs and PNG round trips without a window or GPU. Both test runners execute
it before the scene smoke tests.

## Static material scenes

The shared scene in `content/scripts/scenes/materials.as` builds a fixed
1280x720 view of twelve cubes. Camera, light, transforms and draw order are
constant; the scene does not read elapsed time. The fixture uses the existing
cube geometry and UVs without its blue vertex colors, so material colors can
be checked directly.

Choose a render mode and texture binding setup through these entry scripts:

| Texture setup | Forward script | Deferred script |
|---|---|---|
| Bindfull | `tests/materials_forward_test.as` | `tests/materials_deferred_test.as` |
| Bindless | `tests/materials_bindless_forward_test.as` | `tests/materials_bindless_deferred_test.as` |
| Mixed | `tests/materials_mixed_forward_test.as` | `tests/materials_mixed_deferred_test.as` |

In mixed mode UV checker B is bindless; UV checker A and the photograph are
bindfull. Each texture is loaded once and reused by its materials.

```bash
./bin/app --script tests/materials_forward_test.as --ticks 5 --screenshot=/tmp/materials-forward.png --validation=gpu --validation-required
```

Expected layout, left to right and top to bottom:

- Top row: red, green, blue and yellow materials using `simple_unlit`.
- Middle row: procedural checker frequencies 4 and 16, UV checker A and UV
  checker B. The texture samples use `simple_texture_unlit`.
- Bottom row: a cool-tinted `simple_lit` material, a photograph with
  `simple_texture_lit`, an untextured `simple_texture_unlit` material using the
  white fallback, and UV checker A with `simple_texture_lit`.

The current deferred lighting pass applies Lambert lighting to all GBuffer
materials, including the shaders named `unlit`. Forward and deferred images
therefore need separate baselines; the presets do not imply pixel parity.

Material constants are set through `SetBaseColor(r, g, b, a = 1)` and
`SetMaterialParams(x, y, z, w)`. The procedural checker uses parameter x as its
frequency. Other preview materials initialize x to 8 and do not use it.

**Known B11 reproducer:** the renderer still writes every material into one
shared uniform buffer. The last draw supplies white base color, checker
frequency 8 and UV checker A's texture flags/index. Consequently the top row
loses its distinct colors, both procedural checkers use frequency 8, and
bindless materials can sample checker A instead of their own texture. These
images are diagnostic baselines, not correct golden images. Per-material
buffers or a frame-local uniform allocator remain the next separate change.

The six entry scripts are discovered by the existing smoke-test runner. A
clean validation run confirms legal GPU commands, not the expected material
appearance. Vulkan is the runtime verification platform here; DX12 and Metal
need native checks. Metal does not support the deferred or bindless presets.
