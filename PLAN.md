# PLAN

Working backlog for LUCUS ENGINE 2, split into three tracks:

- **Bugs** — the engine does the wrong thing today. Fix before building on top.
- **Support** — platform/backend parity, build & test infrastructure, hardening. Nothing new for the user, but it unblocks the rest.
- **Features** — new rendering capability. The actual roadmap.

Each item carries the file it lives in and a "done when" so it can be closed
without re-deriving the analysis.

Priorities: **P0** crash / corruption · **P1** visibly wrong output or a latent
hazard · **P2** cleanup, no user-visible symptom today.

---

## 1. Bugs

| id | prio | title | where |
|----|------|-------|-------|
| B1 | P0 | ~~Framebuffer view array sized by constant, not by real swapchain image count~~ **fixed** | `vulkan/vk_dynamic_rhi.cpp` |
| B2 | P0 | `g_swapchainImageCount` treated as fact everywhere else too | `vulkan/vk_window_context.*` |
| B3 | P1 | ~~Camera frame uniform buffer is keyed by camera, not by camera+window~~ **fixed** | `renderer.cpp` |
| B4 | P1 | Texture-upload tick skips the whole frame and still advances the frame ring | `renderer.cpp` |
| B5 | P1 | Shadow pass re-recorded per window context into one shared render target | `renderer.cpp` |
| B6 | P1 | Acquire / present return codes discarded | `vulkan/` |
| B7 | P1 | DX12 renders the triangle upside down | `shaders/`, `dx12/` |
| B8 | P2 | ~~Sampler state cannot be described through the RHI~~ **fixed** | `dynamic_rhi.hpp`, backend sampler types |
| B9 | P2 | Semaphore ring and fence ring have different lengths | `vulkan/vk_dynamic_rhi.cpp` |
| B10 | P2 | Framebuffer cache is never invalidated and its key has no context | `vulkan/` |
| B11 | P1 | Every draw reads the last material uniform written in the frame | `renderer.cpp` |
| B12 | P1 | PSO identity is mutable and does not include the render pass | `material.*`, `renderer.cpp`, RHI backends |
| B13 | P1 | Truncated hashes are used as unique GPU resource handles | `mesh.cpp`, `material.cpp`, RHI backends |
| B14 | P1 | Intrusive-reference ownership is inconsistent and leaks objects | `lucus-core/`, `scene.cpp`, `gltf_utils.cpp`, `renderer.cpp` |

### B1 — Framebuffer view array sized by constant · P0 · **FIXED** (`71b85e5`)

[vk_dynamic_rhi.cpp:1332](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L1332)

`viewsPerFrame` was resized to `g_swapchainImageCount` (3), then filled with
`count` entries, where `count` comes from `rt.desc.count` — the *actual*
swapchain image count reported by the driver. Mesa hands out 4, so the fill loop
wrote past the end of the vector. `vk_framebuffer::init` then read the same
out-of-range slots. Hard segfault on the second tick.

```diff
-    fb_desc.viewsPerFrame.resize(g_swapchainImageCount);
+    fb_desc.viewsPerFrame.resize(count);
```

Reproduced on Mesa (4 images), invisible on any driver that happens to return 3.

**Closed.** Verified: 14/14 tests pass with the fix. Kept here because B2 is the
same assumption left unfixed elsewhere.

### B2 — `g_swapchainImageCount` treated as fact · P0

[render_types.hpp:12](src/lucus-graphics/render_types.hpp#L12) ·
[vk_window_context.hpp:38](src/lucus-graphics/vulkan/vk_window_context.hpp#L38) ·
[vk_window_context.cpp:273](src/lucus-graphics/vulkan/vk_window_context.cpp#L273)

B1 was one symptom; the assumption is still load-bearing elsewhere.
`minImageCount` passed to `vkCreateSwapchainKHR` is a **request** — the driver
may return more. `imageSync` is a fixed `std::array<vk_image_sync, 3>`, and both
`vkGetSwapchainImagesKHR` calls throw their `VkResult` away.

**Fix:** rename the constant to something like `g_desiredSwapchainImageCount`,
store the real count in `vk_swapchain::imageCount`, and size every per-image
array from that. Make `imageSync` a `std::vector`.

**Done when:** nothing indexes a per-image array by a compile-time constant, and
the engine survives a driver returning 2 or 5 images.

### B3 — Camera uniform buffer keyed by camera, not camera+window · P1 · **FIXED**

[renderer.cpp:745](src/lucus-graphics/renderer.cpp#L745) ·
[renderer.cpp:311-323](src/lucus-graphics/renderer.cpp#L311-L323) ·
[renderer.cpp:495-507](src/lucus-graphics/renderer.cpp#L495-L507)

This is the root cause behind the old *"Fix Window context aspect ratio"* TODO.
The projection itself is computed correctly, from the right per-context
`width`/`height` — the defect is *where it is written*. The buffer is addressed
by `cmr->getHandle()`, so every window context in a frame shares one slot.

It is a data race, not merely a last-writer-wins overwrite. `execute()` waits on
the frame fence at its **start**
([vk_dynamic_rhi.cpp:382](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L382)),
but the uniform write happens during pass recording, before that call:

```
ctx1: record passes, write UB[frame%2] = 1280x720 -> execute(ctx1): wait fence, submit S1
ctx2: record passes, write UB[frame%2] =  800x600 -> execute(ctx2): wait fence (for S1) ...
                     ^ overwrites while S1 is still in flight and reading it
```

So window 1 is drawn with window 2's aspect ratio. Widening the fence wait does
not help — the write must not target a slot another submit still owns.

**Live reproducer already in the suite:**
[two_windows_test.as](content/scripts/tests/two_windows_test.as) creates
1280x720 and 800x600. It passes only because nothing inspects the image (S6).

**Fixed.** The view buffer moved off the camera into the renderer, one per
window context (`renderer::_viewBuffers`, indexed by context index) and surfaced
through `pass_context::view`. `camera::_handle` / `getHandle` / `setHandle` were
removed — the camera never owned that buffer, and nothing else referenced them
(not script-bound). The `const_cast<camera*>` at the lazy-create site went with
them.

The camera identity never mattered: `updateFrameUniformBuffer` rewrites the
whole struct every frame, so the buffer only ever needed to be per *context*.

**Verified** by probing the target handle on every write and running
`two_windows_test` (1280x720 + 800x600):

```
BEFORE  frame=1 ub=10 1280x720
        frame=1 ub=10  800x600   <- same buffer, second window overwrote the first
AFTER   frame=1 ub=10 1280x720
        frame=1 ub=12  800x600   <- one buffer per context
```

**Still shared, benignly:** the light's `viewProj` buffer (shadow pass) and the
`LightUniforms` buffer are still one-per-scene and rewritten by every context in
a frame. Same race, but every context writes *identical* bytes — the shadow
extent is a fixed constant and the light does not depend on the window — so
there is nothing to observe. **B5** removes the duplicate writes entirely by
hoisting the shadow pass out of the per-context loop.

**Same defect class as B11** — one shared uniform buffer written repeatedly
before a single submit. B3 was per window, B11 is per draw. B11 still needs the
frame-local ring; B3 did not, because one buffer per context suffices when the
contents are fully rewritten each frame.

### B4 — Texture-upload tick eats a frame · P1

[renderer.cpp:51-68](src/lucus-graphics/renderer.cpp#L51-L68)

When textures are pending, `tick` uploads them, then does `g_frameIndex++;
return;` — skipping all rendering for that tick *and* advancing the frame ring
without a matching submit. After that, `g_frameIndex` no longer tracks in-flight
submissions, which is what the fence and uniform-buffer slot indices assume.

**Fix:** upload and render in the same tick, or advance the frame index only
where a submit actually happens.

**Done when:** frame index increments once per `execute()`, never on the upload
path.

### B5 — Shadow pass duplicated across window contexts · P1

[renderer.cpp:71-79](src/lucus-graphics/renderer.cpp#L71-L79) ·
[renderer.cpp:901](src/lucus-graphics/renderer.cpp#L901)

`shadowPass` runs once per window context, but `shadow_rt_handle` is a single
renderer-wide target whose slot is picked by `currentFrame`, not by context. Two
windows in one frame record two shadow passes into the same slot. Today they are
serialized by the shared fence so nothing corrupts — it is pure duplicated work,
and it becomes a real write hazard the moment contexts stop being serialized.

**Fix:** render the shadow map once per frame before the per-context loop.

**Done when:** the shadow pass is recorded once regardless of window count.

### B6 — Acquire / present results discarded · P1

[vk_window_context.cpp:149](src/lucus-graphics/vulkan/vk_window_context.cpp#L149) ·
[vk_dynamic_rhi.cpp:684](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L684)

`vkAcquireNextImageKHR` and `vkQueuePresentKHR` return values are ignored, so
`VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` pass silently. Every other
Vulkan call in the codebase checks its result and throws — these two are the
exception. Prerequisite for S3 (window resize).

**Done when:** both are checked and out-of-date is routed to swapchain
recreation.

### B7 — DX12 triangle upside down · P1

[shaders/common/defines.slang:12](shaders/common/defines.slang#L12) ·
`src/lucus-graphics/dx12/`

Carried over from the previous TODO list. Shaders flip Y under
`#if TARGET_METAL || TARGET_DX12`, applied per-shader rather than in one place —
[triangle.slang:24](shaders/triangle.slang#L24),
[simple.slang:9](shaders/simple.slang#L9),
[deferred_lighting.slang:23](shaders/deferred_lighting.slang#L23). Worth fixing
by picking a single convention (negative-height viewport, or one shared helper)
instead of per-shader `y *= -1`.

**Not verified on Linux** — needs a Windows machine to confirm and close.

### B8 — Sampler state cannot be described through the RHI · P2 · **FIXED**

Every sampler handle resolved to identical backend-hardcoded state: `REPEAT` on
all axes, linear filtering, `compareEnable = VK_FALSE`. The renderer already
created four distinct handles and bound them to distinct shader slots, so the
intent was there; only the description was missing.

Added a backend-neutral `sampler_desc` (filter, mip filter, address mode, border
colour) to `dynamic_rhi::createSampler`, with translation tables in all three
backends. The four samplers are now:

| sampler | address mode | border |
|---|---|---|
| default | `REPEAT` | opaque black |
| bindless | `REPEAT` | opaque black |
| shadow map | `CLAMP_TO_BORDER` | opaque **white** |
| g-buffer | `CLAMP_TO_EDGE` | opaque black |

White border on the shadow map means "far / lit" for anything sampled outside
the map. `CLAMP_TO_EDGE` on the g-buffer stops the fullscreen lighting pass
wrapping at screen edges.

Also fixed on the way: the Vulkan border colour was `VK_BORDER_COLOR_INT_OPAQUE_BLACK`
on float-format images, which is invalid usage. Now `FLOAT_OPAQUE_BLACK`.

**Correction to the original entry.** It claimed "geometry outside the light
frustum samples wrapped depth". That was wrong — `computeShadow` in
`shaders/common/light.slang` already masks the result with an explicit
`insideShadowMap` UV bounds check. The real defect was narrower: bilinear
filtering at the shadow-map edge blended in texels from the opposite edge, and
the g-buffer sampler wrapped at screen borders.

**Verified** by probing `VkSamplerCreateInfo` at creation: four samplers, the
address mode and border colour above, all distinct. Suite 14/14.

**Still open:** hardware comparison sampling (`compareEnable = VK_TRUE`,
`COMPARE_OP_LESS_OR_EQUAL`) for free PCF. It needs `SamplerComparisonState` /
`SampleCmp` in the shaders and a separate comparison-sampler binding, so it did
not belong in this change. Tracked under **F3** (PBR), which is where soft
shadows start to matter.

### B9 — Mismatched sync ring lengths · P2

[render_types.hpp:11-12](src/lucus-graphics/render_types.hpp#L11-L12) ·
[vk_dynamic_rhi.cpp:380](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L380) ·
[vk_dynamic_rhi.cpp:656](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L656)

Fences ring on `g_framesInFlight` (2), semaphores on `g_swapchainImageCount`
(3). It is currently safe only because there is a single in-order queue. The
conventional layout is `imageAvailable` per frame-in-flight and `renderFinished`
per swapchain **image** — presenting image *i* must wait on a semaphore owned by
image *i*, not by a frame slot.

**Done when:** semaphore ownership matches what each one actually guards.

### B10 — Framebuffer cache never invalidated · P2

[vk_dynamic_rhi.hpp:88](src/lucus-graphics/vulkan/vk_dynamic_rhi.hpp#L88) ·
[vk_render_types.hpp:94](src/lucus-graphics/vulkan/vk_render_types.hpp#L94)

`_framebuffers` grows forever and has no drop path when render targets are
destroyed or recreated. `vk_framebuffer_key` also carries no window-context
field — the shadow framebuffer is shared across windows, which is correct today
purely by accident (same pass, same size, same target handle). Both block S3.

**Done when:** the cache is invalidated on render-target recreation and the key
identifies its owner unambiguously.

### B11 — One material uniform buffer for every draw · P1

[renderer.hpp:56](src/lucus-graphics/renderer.hpp#L56) ·
[renderer.cpp:372](src/lucus-graphics/renderer.cpp#L372) ·
[renderer.cpp:536](src/lucus-graphics/renderer.cpp#L536) ·
[renderer.cpp:865](src/lucus-graphics/renderer.cpp#L865)

`updateMaterialUniformBuffer` writes each object's material into the same
`g_defaultMaterialBufferHandle`. Command recording then binds that same buffer
for every draw, and `execute()` runs only after all objects have been recorded.
Consequently every draw in the pass reads the last material value written for
that frame slot. Separate texture descriptors hide part of the problem, but
base colour, material parameters and bindless texture flags/indices are wrong.

**Fix:** allocate material constants per material or per draw. A frame-local
uniform ring with dynamic offsets scales better than one GPU buffer per object.
The command protocol must carry the selected offset or allocation handle.

Same underlying defect as **B3** (one shared buffer, many writers before a single
submit); the same ring solves both. Do them together.

**Done when:** a scene with two objects using different base colour and
bindless-material flags renders both correctly in forward and deferred modes.

### B12 — PSO identity is mutable and incomplete · P1

[material.cpp:20](src/lucus-graphics/material.cpp#L20) ·
[material.cpp:45](src/lucus-graphics/material.cpp#L45) ·
[renderer.cpp:144](src/lucus-graphics/renderer.cpp#L144) ·
[vk_dynamic_rhi.cpp:841](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L841)

`material::getHash()` depends on the renderer singleton's current mode, while a
material stores only one `_pso_handle`. Switching an already-prepared scene from
forward to deferred leaves that handle valid, so the renderer can bind a
forward pipeline in the GBuffer pass. The backend cache key also omits the
explicit `render_pass_name`, attachment formats and most fixed-function state.

**Fix:** define an immutable `pipeline_state_desc` and use it as the cache key.
It should include shader variant, render pass/attachment formats, vertex layout,
depth/raster/blend state and binding layout. Materials should request a PSO for
the current pass rather than own one global transient handle.

**Done when:** switching forward/deferred mode at runtime creates or selects the
correct pass-compatible PSO, and distinct PSO descriptors cannot alias.

### B13 — Resource hash used as unique handle · P1

[mesh.cpp:15](src/lucus-graphics/mesh.cpp#L15) ·
[material.cpp:45](src/lucus-graphics/material.cpp#L45) ·
[vk_dynamic_rhi.cpp:964](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L964)

Mesh, texture and pipeline caches truncate `std::hash` to `u32` and then use the
result directly as the public handle and map key. A collision silently aliases
two unrelated GPU resources. The mesh key is especially weak: path plus draw
count does not identify vertex/index contents, and multiple glTF primitives can
share both while containing different geometry. A truncated hash can also be
zero, which the handle API reserves as invalid.

**Fix:** separate ownership identity from cache lookup. Use allocated,
generation-checked handles for resource tables; use a full descriptor plus
equality comparison for optional deduplication.

**Done when:** hash collisions cannot substitute one resource for another, and
destroyed/recreated resources cannot be reached through stale handles.

### B14 — Intrusive ownership leaks · P1

[object.hpp:24](src/lucus-core/object.hpp#L24) ·
[intrusive_ptr.hpp:13](src/lucus-core/intrusive_ptr.hpp#L13) ·
[scene.cpp:10](src/lucus-graphics/scene.cpp#L10) ·
[gltf_utils.cpp:827](src/lucus-graphics/gltf_utils.cpp#L827)

`object` starts with reference count 1 and constructing/resetting an
`intrusive_ptr` from a raw pointer increments it again. Call sites must therefore
release the factory's initial reference after transfer. Some importer paths do
this, but `scene::newObject`, imported cameras, mesh caches and renderer default
resources do not. Their objects retain at least one unreachable reference.

The same raw-pointer API serves both AngelScript ownership and internal C++
ownership, making correct transfer easy to miss.

**Fix:** choose one explicit convention: factories return `intrusive_ptr<T>`,
objects start with count zero, or an `adopt_ref` constructor transfers the
initial reference without incrementing it. Keep AngelScript adapters at the
binding boundary instead of spreading its ownership convention through engine
code.

**Done when:** scene load/unload and renderer init/cleanup return all object
counts to baseline under ASan/LSan, including failed and partial glTF loads.

---

## 2. Support

| id | prio | title |
|----|------|-------|
| S1 | P1 | Metal backend does not match the current RHI interface |
| S2 | P1 | `assert()` used as production error handling |
| S3 | P1 | No swapchain recreation / window resize |
| S4 | P2 | ~~GCC cannot compile the project~~ **fixed** |
| S5 | P2 | ~~`compile_vulkan.sh` does not resolve `slangc` from `scripts/tools`~~ **fixed** |
| S6 | P2 | Tests only check the exit code |
| S7 | P2 | Refactor renderer handle plumbing — *partly done* |
| S8 | P1 | RHI has no explicit backend capability contract |
| S9 | P1 | No cross-platform build matrix guards backend interface parity |

### S1 — Metal backend does not match the current RHI interface · P1

`src/lucus-graphics/metal/`

Metal was deliberately parked at the `shadow_map` stage, but the current source
has since drifted past simple feature disparity. `dynamic_rhi` now requires
`createPSO(material*, render_pass_name)`, `getWindowContextRenderTarget` and
`createRenderTarget`; `m_dynamic_rhi` still declares `createPSO(material*)` and
does not implement the two render-target methods. Its `override` is incompatible
and the class remains abstract, so the current macOS target cannot compile.

Intended parity after restoring the build:

| | Vulkan | DX12 | Metal |
|---|---|---|---|
| Forward pass | yes | yes | yes |
| Shadow map | yes | yes | **no** |
| Deferred / GBuffer | yes | yes | **no** |
| Bindless textures | yes | yes | **no** |

**Decision needed:** revive Metal, or remove it from the supported build matrix
and label it as archived. If retained as forward-only, it must still implement
the current interface, reject unsupported modes explicitly and compile in CI.
Every new feature currently costs 2×, not 3× — that should be a conscious
choice, not a drift.

**Done when:** macOS compiles from a clean checkout and a forward-pass smoke test
runs, or Metal is deliberately removed from supported platforms and build docs.

### S2 — `assert()` as error handling · P1

42 asserts in [vk_dynamic_rhi.cpp](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp)
alone, several guarding real invariants rather than programmer mistakes:
`assert(found != _renderPasses.end())`, `assert(pair.second)`,
`assert(count == rt.desc.count)`. Under `NDEBUG` all of them vanish and the same
conditions become silent memory corruption.

That last one sits directly on the B1 path — it would have caught the crash, and
in a release build it would not have.

**Fix:** promote invariant checks to `throw`, matching the style already used
for every `VkResult`. Keep `assert` for genuinely impossible states.

**Done when:** a Release build fails loudly instead of silently.

### S3 — No swapchain recreation · P1

No GLFW framebuffer-size callback exists anywhere in
[window.cpp](src/lucus-graphics/window.cpp) /
[window_manager.cpp](src/lucus-graphics/window_manager.cpp), and there is no
recreation path. Resizing a window produces stretched or undefined output.

Depends on **B6** (detect out-of-date), **B10** (drop stale framebuffers) and
**B2** (image count may change with the new swapchain).

**Done when:** resizing a window re-creates the swapchain and dependent targets,
and `two_windows_test` survives a resize.

### S4 — GCC cannot compile the project · P2 · **FIXED**

[material.hpp:14](src/lucus-graphics/material.hpp#L14)

```cpp
intrusive_ptr<texture> texture;   // member named after its own type
```

`texture_slot::texture` shadowed the `lucus::texture` class it was declared
from. GCC 13+ makes this a hard error (`-Werror=changes-meaning`, added in GCC
13); clang accepts it silently. Building with GCC required
`-Wno-changes-meaning`.

Renamed the member to `tex` — declaration plus four call sites
([material.cpp:36](src/lucus-graphics/material.cpp#L36),
[renderer.cpp:380](src/lucus-graphics/renderer.cpp#L380),
[544](src/lucus-graphics/renderer.cpp#L544),
[817](src/lucus-graphics/renderer.cpp#L817)). The Metal `.texture` accesses are
`MTLRenderPassDescriptor` fields and were left alone.

**Closed.** Verified: clean GCC 13.3 configure + build with no workaround flag,
14/14 tests pass on the resulting binary.

### S5 — `slangc` lookup inconsistent across platforms · P2 · **FIXED**

`compile_dx12.bat` prepended `scripts\tools\slang\bin` to `PATH`,
`compile_metal.sh` hardcoded `scripts/tools/slang/bin/slangc` with no fallback,
and `compile_vulkan.sh` called bare `slangc` with its tools-dir block commented
out. So the layout the README documents did not work on Linux, and a
system-wide `slangc` did not work on macOS.

Both shell scripts now share one resolver — `$SLANGC`, then
`scripts/tools/slang/bin/slangc`, then `PATH`, then a diagnostic and exit 1.
The chosen binary is echoed so build logs record which compiler ran.

**Closed.** Verified on all three paths (tools dir only, `PATH` only, `$SLANGC`
override with the other two absent): 26/26 shaders each time, and the
not-found case exits 1 with instructions.

### S6 — Tests only check the exit code · P2

[run_tests.sh:52](scripts/run_tests.sh#L52)

A test passes if the app exits 0 after N ticks. Nothing inspects the rendered
image. B1 was caught only because it crashed; anything that merely renders
*wrong* passes silently — which is exactly the failure mode of B3, B5, B7, B8
and B11.

**Fix:** dump the swapchain image on the last tick and compare against a golden
image with a tolerance.

**Done when:** a deliberately broken shader fails the suite.

### S7 — Refactor renderer handle plumbing · P2

Carried over from the previous TODO list. Partially done — GBuffer handles moved
into window contexts (commit `2f8247e`). What remains: `shadow_rt_handle` is
still renderer-global (see B5), and the pass functions each re-fetch context size
and re-derive the same handles
([renderer.cpp:281](src/lucus-graphics/renderer.cpp#L281),
[448](src/lucus-graphics/renderer.cpp#L448),
[628](src/lucus-graphics/renderer.cpp#L628)).

**Q1 landed the pass-context half.** `pass_context` is now built once per window
context per frame in `tick` and handed to every pass; the three duplicated
preambles and the lazy uniform-buffer creation are gone, replaced by
`makePassContext`, `requireGBufferTargets`, `ensureViewBuffer`,
`ensureShadowViewBuffer` and `ensureLightBuffer`. `shadowPass` lost its unused
`window_context_handle` parameter.

Still open: `shadow_rt_handle` remains renderer-global (**B5**), and
`pass_context` does not yet carry the frame slot, the acquired swapchain image
or a frame-local uniform allocator — that arrives with **B3**/**B11**.

The duplication that was removed, for the record:

| block | forwardPass | gbufferPass | lightingPass |
|---|---|---|---|
| context size + target fetch + validation | [280-290](src/lucus-graphics/renderer.cpp#L280-L290) | [446-459](src/lucus-graphics/renderer.cpp#L446-L459) | [627-642](src/lucus-graphics/renderer.cpp#L627-L642) |
| lazy-create view uniform buffer | [311-323](src/lucus-graphics/renderer.cpp#L311-L323) | [495-507](src/lucus-graphics/renderer.cpp#L495-L507) | [644-650](src/lucus-graphics/renderer.cpp#L644-L650) |
| lazy-create light uniform buffer | [325-338](src/lucus-graphics/renderer.cpp#L325-L338) | — | [672-684](src/lucus-graphics/renderer.cpp#L672-L684) |

**Fix:** a per-frame render-context struct built once per context per frame and
passed to every pass. It should own the context extent, actual swapchain image,
frame slot, pass targets and frame-local uniform allocator. This also provides a
natural home for the B3 and B11 fixes.

```cpp
struct pass_context {
    window_context_handle ctx;
    u32 width, height;
    render_target_handle color, depth;
};
```

**Note on error messages:** the three per-pass validation throws collapsed into
one `makePassContext::` message. `gbufferPass` now also requires a valid colour
target, which `lightingPass` already demanded in the same frame, so this is not
a new failure mode.

### S8 — No explicit backend capability contract · P1

[dynamic_rhi.hpp](src/lucus-graphics/dynamic_rhi.hpp)

Optional RHI operations return empty handles or default no-ops. The renderer can
select deferred mode on a backend that does not support GBuffer targets and only
fail later while recording a pass. Feature support currently exists in README
tables and backend knowledge inside the renderer, not in executable code.

**Fix:** expose a capability struct from `dynamic_rhi` covering render modes,
bindless resources, comparison samplers and relevant limits. Validate requested
mode once, before resource or command creation. Unsupported paths should fail
with a backend-and-feature-specific error.

**Done when:** selecting deferred or bindless mode on an unsupported backend is
rejected deterministically before the first frame.

### S9 — No cross-platform build matrix · P1

Platform sources are selected by CMake, so a Linux build cannot even parse the
DX12 or Metal implementations. Interface drift therefore remains invisible
until someone builds on that OS; the current Metal `createPSO` mismatch is an
example.

**Fix:** CI compile jobs for Linux/Vulkan, Windows/DX12 and macOS/Metal. Runtime
smoke tests may remain platform-specific, but every supported backend must at
least compile after an RHI interface change.

**Done when:** all supported platform targets are compiled for every change to
`dynamic_rhi`, command structures, shared render types or shader bindings.

---

## 3. Features

| id | title | depends on |
|----|-------|-----------|
| F1 | Get render object from scene by name | — |
| F2 | Flag on render object to skip the main pass | — |
| F3 | Simple PBR | B11 |
| F4 | Early Z | S7 |
| F5 | TAA like Inside | B2, B3, B11, S3, S7, F3 |

### F1 — Get render object from scene by name

`scene` currently exposes only `objects()`. Scripts have no way to grab one
object. Needs a name on `render_object`, a lookup on `scene`, and an
AngelScript binding in
[bind_graphics.cpp](src/lucus-graphics/bind_graphics.cpp).

### F2 — Flag to skip the main pass

`render_object` already has `bCastShadow`
([render_object.hpp:50](src/lucus-graphics/render_object.hpp#L50)). Add the
mirror flag — shadow caster that is invisible in the main pass. Same shape as
the existing flag, plus a script binding.

### F3 — Simple PBR

GBuffer B is already reserved and documented as
Metalness + Roughness + Specular + ShadingModel
([structs.slang:54](shaders/common/structs.slang#L54),
[render_types.hpp:55](src/lucus-graphics/render_types.hpp#L55)), and the
material layout already carries bindless indices for an RME map
([render_types.hpp:174](src/lucus-graphics/render_types.hpp#L174)). The lighting
pass still evaluates plain Lambert
([deferred_lighting.slang](shaders/deferred_lighting.slang)).

So the plumbing exists; what is missing is the BRDF. Cook-Torrance in the
deferred lighting pass, writing real material values into GBuffer B.

Wants **B11** first (per-draw material constants). **B8** is done; the remaining
shadow-quality work — a comparison sampler for hardware PCF — belongs here,
since PBR is where hard shadow edges start to look wrong.

### F4 — Early Z

Depth prepass before the GBuffer pass. No prepass exists today
(`render_pass_name` has only SHADOW / FORWARD / GBUFFER / DEFERRED_LIGHTING,
[render_types.hpp](src/lucus-graphics/render_types.hpp)). Cheap to add once
**S7** gives passes a shared context, and it pairs naturally with F2.

### F5 — TAA like Inside

The big one. Needs history buffers, per-frame jitter, motion vectors (a new
GBuffer channel or a separate target), and reprojection. Depends on **B2** and
**S3** — TAA is meaningless while resize is undefined and the image count is
assumed — and reads much better on top of **F3**.

---

## Quick wins

Small, self-contained items worth doing before the large refactors. Each is
verifiable with the existing suite and touches one area.

| | item | scope | effort | risk |
|---|---|---|---|---|
| Q1 | ~~S7 — extract `pass_context` + uniform lazy-create helpers~~ **done** | `renderer.cpp` | — | closed |
| Q2 | ~~B3 — move the view buffer off the camera, slot per context~~ **done** | `renderer.cpp` | — | closed |
| Q3 | ~~B8 — add `sampler_desc` to `createSampler`~~ **done** | RHI + backends | — | closed |
| Q4 | ~~S4 — rename `material::texture` to `tex`~~ **done** | `material.*` | — | closed |
| Q5 | ~~S5 — resolve `slangc` from `scripts/tools/slang/bin`~~ **done** | `compile_vulkan.sh` | — | closed |

**All quick wins are done.** Next up is the execution order below.

**Caveat:** Q2 fixed a race the suite still cannot observe. It was verified with
temporary instrumentation (see B3), not by an assertion that would catch a
regression. Until **S6** exists, nothing stops it coming back.

---

## Recommended execution order

1. **B2** — remove the remaining fixed swapchain-image-count assumptions.
2. **B11** — make material data correct per draw. B3, the per-window half of
   the same defect, is already fixed; this is the remaining per-draw half and
   still wants a frame-local uniform ring with dynamic offsets.
3. **B12 + B13** — establish stable PSO/resource identity before adding more
   pipeline variants and transient targets.
4. **B14** — make ownership explicit before scene lookup and scene-lifecycle
   features add more retained references.
5. **B6 + B10 + S3** — implement resize as one lifecycle change.
6. **S1 + S8 + S9** — decide Metal status, expose capabilities and enforce
   backend interface parity in CI.
7. **B5 + S7** — introduce the per-frame/per-window render context, hoist the
   shadow pass out of the per-context loop and remove duplicated pass setup.
   If the quick wins above were taken, Q1 already landed the context struct and
   Q2 the B3 half.
8. Continue with **F3**, **F4**, then **F5**.

---

## Appendix — verified state

Everything below was run on Linux/Vulkan (Mesa, 4 swapchain images) with the B1
fix applied:

- clean configure + build: 98/98 targets
- shaders: 26/26 compiled to SPIR-V with slang 2026.5
- tests: 14/14 pass

Verified against the source while writing this plan: every file:line reference
in B1-B14 and S1-S9, including the additions B11-B14, S8 and S9.

Not verified: DX12 and Metal paths (no Windows/macOS machine in reach). B7 in
particular is carried over on trust and still needs confirmation, and the S1
Metal interface mismatch is read from the headers rather than from a failed
macOS build.

Not reproducible with the current suite (exit-code only, see S6): B3, B5, B7,
B8, B11, B14. These are read from the code, not observed. B3 has a live
reproducer in `two_windows_test` that the suite cannot currently judge.
