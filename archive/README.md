# Archive

Code that is kept for reference but is not part of the build.

## `metal/` — macOS Metal backend

Archived 2026-08-20. Decision recorded in `PLAN.md` **S1**.

The backend was parked at the forward-pass/shadow-map stage and then drifted out
of the RHI interface: `m_dynamic_rhi` declares `createPSO(material*) override`
where the base class has `createPSO(material*, render_pass_name)`, and it never
implemented `createRenderTarget` or `getWindowContextRenderTarget`. The class was
therefore abstract and the macOS target had not compiled for some time. Shadow
mapping, deferred rendering and bindless textures were never implemented.

Nothing was deleted. `archive/scripts/` holds the macOS build and Metal shader
compilation scripts that went with it.

Configuring the project on macOS now fails immediately with a message pointing
here, instead of failing deep inside the compile.

`validation_backend::METAL` stays in `validation_sink` — it costs nothing and is
one less thing to reintroduce.

### To revive

1. Restore `metal/` to `src/lucus-graphics/metal/` and the scripts to `scripts/`.
2. Undo the `APPLE` guard in `cmake/liblucus/lucus-graphics/CMakeLists.txt` and
   restore the Metal framework lookup and source glob from git history.
3. Implement the current `dynamic_rhi` interface in full — 13 pure virtuals —
   and reject unsupported render modes explicitly rather than silently.
4. Restore `if(APPLE OR WIN32)` in `cmake/liblucus/CMakeLists.txt` (C++20).
5. Do `VALIDATION_PLAN.md` **V7**: Metal validation through `validation_sink`.
6. Add macOS to CI (`PLAN.md` **S9**) so it cannot drift out again.
