# VALIDATION PLAN

Design and implementation steps for RHI validation as a first-class, always-on
mode, and for making validation errors fail the test suite.

Related: `PLAN.md` → **S6** (tests only check the exit code). This plan does not
replace S6 (golden-image comparison); it is the cheaper half of the same
problem — a whole class of defects reports itself through the graphics API
already, and today the engine throws that report away.

---

## Current state

| backend | what exists | what is missing |
|---------|-------------|-----------------|
| Vulkan | `VK_LAYER_KHRONOS_validation`, gated on `#ifndef NDEBUG` ([vk_dynamic_rhi.cpp:150](src/lucus-graphics/vulkan/vk_dynamic_rhi.cpp#L150)) | no `VK_EXT_debug_utils`, no messenger callback, no GPU-AV, no sync validation, no object names |
| DX12 | debug layer + GBV, `ID3D12InfoQueue` filter, gated on `#ifndef NDEBUG` ([dx_dynamic_rhi.cpp:715](src/lucus-graphics/dx12/dx_dynamic_rhi.cpp#L715)) | queue is never drained, `SetBreakOnSeverity(WARNING)` breaks in CI, GBV is unconditional |
| Metal | nothing | env-based debug layer, `MTLCommandBufferDescriptor.errorOptions` |

Common to all three:

- Validation output never reaches the engine. Vulkan messages go to the loader's
  default sink, DX12 messages sit in the info queue until the process exits.
- Nothing counts errors, so the process still exits `0`. `run_tests.sh` checks
  the exit code only, so a run that emits 400 validation errors passes.
- `#ifndef NDEBUG` is the only switch: Release has no validation at all, Debug
  always pays for it, and neither can be changed from the command line.

---

## Target behaviour

```
bin/app --script tests/cube_gpu_test.as --ticks 5
    validation ON  (default)

bin/app --validation=off ...
bin/app --no-validation ...
    nothing enabled, zero cost

bin/app --validation=gpu ...
    Vulkan: GPU-assisted + synchronization validation
    DX12:   GPU-based validation
    slow, opt-in

bin/app --validation-fatal ...
    abort at the first error instead of at the end of the run
```

End of run, always printed when validation is on:

```
[validation] 0 errors, 3 warnings (2 suppressed)
```

Non-zero error count → `EXIT_FAILURE`, with no other change to `run_tests.sh`.

---

## Steps

### V1 — Command-line plumbing

`src/lucus-core/commandline_args.{hpp,cpp}`

`getArgument` returns `""` both for "absent" and for "present but empty", so
`--validation` alone cannot be distinguished from no flag at all. Add:

```cpp
bool hasArgument(const std::string& name) const;
```

Implemented over the same scan as `getArgument`: matches `--name`, `--name=…`.

**Done when:** `--no-validation` (a valueless flag) is detectable.

---

### V2 — Validation level and RHI init descriptor

`src/lucus-graphics/render_types.hpp`

```cpp
enum class rhi_validation_level : u8
{
    OFF = 0,
    ON  = 1,
    GPU = 2,
};

struct rhi_init_desc
{
    rhi_validation_level validation { rhi_validation_level::ON };
    bool validation_fatal { false };
};
```

`src/lucus-graphics/dynamic_rhi.hpp`

```cpp
- virtual void init() = 0;
+ virtual void init(const rhi_init_desc& desc) = 0;
```

Three backends update their signature. Each stores the desc in a member —
Vulkan needs `validation` at `createInstance` and again at `createDevice`, DX12
at `createInstance` (factory flags) and `createDevice`.

Parsing lives in one free function next to the struct, not in the backends:

```cpp
rhi_init_desc rhi_init_desc_from_commandline();
```

`--validation=off|on|gpu` (also `0|1|2`, and `--validation` bare = `on`),
`--no-validation` = `off`, `--validation-fatal` sets the flag. Unknown value →
throw with the list of accepted values; a typo must not silently disable
validation.

Called from [renderer.cpp:24](src/lucus-graphics/renderer.cpp#L24):

```cpp
_dynamicRHI = create_dynamic_rhi();
_dynamicRHI->init(rhi_init_desc_from_commandline());
```

`#ifndef NDEBUG` is removed from both backends — the level is the only switch,
and its default is `ON` in every configuration.

**Done when:** `--validation=off` measurably drops the Vulkan layer (visible in
the instance-creation log), Release build still validates by default.

---

### V3 — `validation_sink`

New: `src/lucus-graphics/validation_sink.{hpp,cpp}` (module uses `file(GLOB)`,
no CMake edit needed — reconfigure only).

Backend-agnostic. Both backends push into it, `engine` reads it.

```cpp
enum class validation_severity : u8 { INFO, WARNING, ERROR };

class validation_sink : public singleton<validation_sink>
{
public:
    void configure(const rhi_init_desc& desc);

    void report(validation_severity severity,
                std::string_view id,
                std::string_view message);

    bool is_suppressed(std::string_view id) const;

    u32 error_count() const;
    u32 warning_count() const;
    u32 suppressed_count() const;

    void print_summary() const;

private:
    std::vector<std::string> _suppressed;   // message ids to ignore
    u32 _errors{0}, _warnings{0}, _suppressed_hits{0};
    bool _fatal{false};
};
```

Behaviour:

- Prints every non-suppressed message as
  `[validation][error] VUID-vkCmdDraw-None-08600: <text>` — one line prefix so
  the test log is greppable.
- Counts by severity.
- `_fatal` → `std::abort()` right after printing an error, so the backtrace
  still points at the offending call.
- Suppression list: seeded with the three DX12 ids currently hard-coded in
  `createDevice` ([dx_dynamic_rhi.cpp:764](src/lucus-graphics/dx12/dx_dynamic_rhi.cpp#L764)),
  and it becomes the single place where "known and accepted" lives, for both
  backends. Later can be loaded from a file.

There is no logging facility in the engine (everything is `printf`/`cerr`), so
the sink prints directly. If a logger appears later this is the one place to
reroute.

**Done when:** a hand-made `report()` call prints and increments the counter.

---

### V4 — Exit code

`src/lucus-sim/engine.cpp`, after `renderer::instance().cleanup()`:

```cpp
validation_sink::instance().print_summary();
if (validation_sink::instance().error_count() > 0) {
    throw std::runtime_error("validation reported errors");
}
```

`main` already turns an exception into `EXIT_FAILURE`, so `run_tests.sh` needs
no change to start failing on validation errors.

Ordering matters: `cleanup()` destroys GPU resources, and destruction is exactly
when leak and lifetime errors are reported. Summary must come after it. For the
same reason the Vulkan messenger must outlive the device (see V5).

**Done when:** an intentionally broken barrier makes `run_tests.sh` fail.

---

### V5 — Vulkan

`src/lucus-graphics/vulkan/vk_dynamic_rhi.{hpp,cpp}`, `createInstance`

1. Query instance extensions, add `VK_EXT_debug_utils` to
   `instanceCreateInfo.ppEnabledExtensionNames` when the level is not `OFF`.
   The current code passes the GLFW array straight through, so it needs a
   `std::vector<const char*>` copy first.
2. Keep the existing layer-presence check, drop the `#ifndef NDEBUG` around it,
   drive it from the level. Keep the graceful fallback message when the layer is
   absent — Vulkan SDK not installed must not be a hard failure.
3. Fill a `VkDebugUtilsMessengerCreateInfoEXT` (WARNING|ERROR severities,
   GENERAL|VALIDATION|PERFORMANCE types), and:
   - chain it into `instanceCreateInfo.pNext` — this is what covers
     `vkCreateInstance` and `vkDestroyInstance` themselves;
   - create the real messenger right after the instance, destroy it last in the
     destructor, after `_device.reset()`, before `vkDestroyInstance`.
4. Callback maps `VkDebugUtilsMessageSeverityFlagBitsEXT` → `validation_severity`
   and forwards `pCallbackData->pMessageIdName` as the id. Returns `VK_FALSE`
   always — returning `VK_TRUE` aborts the call, which we do not want.
5. `GPU` level: chain `VkValidationFeaturesEXT` with
   `VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT` and
   `VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`. Both
   `pNext` structures chain onto the same `instanceCreateInfo`.

`vkCreateDebugUtilsMessengerEXT` / `vkDestroyDebugUtilsMessengerEXT` are
extension entry points — resolve with `vkGetInstanceProcAddr`.

New members: `VkDebugUtilsMessengerEXT _debugMessenger{VK_NULL_HANDLE};` and the
stored `rhi_init_desc`.

Separate follow-up, not part of this step: `vkSetDebugUtilsObjectNameEXT` on
buffers, images, pipelines and command buffers. Without it messages name raw
handles; with it they name the mesh or material. Worth doing once the messages
start being read.

**Done when:** running any test prints `[validation]` lines through the engine's
own output, and `--validation=off` prints none.

---

### V6 — DX12

`src/lucus-graphics/dx12/dx_dynamic_rhi.cpp`

1. `createInstance`: `DXGI_CREATE_FACTORY_DEBUG` by level, not by `NDEBUG`.
2. `createDevice`:
   - `EnableDebugLayer()` on `ON` and `GPU`;
   - `SetEnableGPUBasedValidation(TRUE)` on `GPU` only;
   - `SetBreakOnSeverity` only for `CORRUPTION` and `ERROR`, and only when
     `IsDebuggerPresent()`. Breaking on `WARNING` in a headless CI run turns a
     diagnosable warning into an unexplained crash, which is the opposite of
     what this whole plan is for.
   - Keep the storage filter for `INFO`, but move the `DenyIds` list into
     `validation_sink`'s suppression list so both backends share one place.
3. Drain the queue. Store `Com<ID3D12InfoQueue> _infoQueue;` and add:

```cpp
void dx_dynamic_rhi::drainValidationMessages();
```

`GetNumStoredMessages` → for each, `GetMessage(i, nullptr, &len)` to size a
buffer, `GetMessage(i, msg, &len)` to read, map `D3D12_MESSAGE_SEVERITY` →
`validation_severity`, forward with the numeric `D3D12_MESSAGE_ID` as the id
string, then `ClearStoredMessages()`.

Call it at the end of `execute()` ([dx_dynamic_rhi.cpp:161](src/lucus-graphics/dx12/dx_dynamic_rhi.cpp#L161))
and once more in the destructor before the device goes away.

**Done when:** it compiles. Cannot be run here — no Windows machine. Must be
marked unverified when reported.

---

### V7 — Metal

`src/lucus-graphics/metal/m_dynamic_rhi.mm`

Metal's debug layer is process-wide and read at device creation, so it cannot be
switched inside the RHI after the fact. Options, in order of preference:

1. `setenv("MTL_DEBUG_LAYER", "1", 1)` and, on `GPU`,
   `setenv("MTL_SHADER_VALIDATION", "1", 1)` at the very top of `engine::run`,
   before anything touches Metal. Ugly but honest, and it is the documented
   mechanism.
2. `MTLCommandBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus`,
   then read `commandBuffer.error` in the completion handler and forward to the
   sink. This part is real API and belongs in the backend regardless.

Blocked behind `PLAN.md` **S1** — the Metal backend does not currently match the
RHI interface, so this step lands with, or after, that.

**Done when:** compiles on macOS and a deliberate error surfaces. Not verifiable
here.

---

### V8 — Test suite integration

`scripts/run_tests.sh`, `scripts/run_tests.bat`

- Pass `--validation=${VALIDATION:-on}` to every run so the level can be
  overridden from the environment without editing the script.
- Second, optional pass with `VALIDATION=gpu` — too slow for every run, useful
  before a release or after touching barriers and descriptors.
- The summary line lands in `scripts/logs/` with everything else.

**Done when:** `VALIDATION=gpu ./scripts/run_tests.sh` runs the whole suite under
GPU-assisted validation, and the default run reports `0 errors`.

---

## Order and risk

| step | depends on | risk |
|------|-----------|------|
| V1 | — | none |
| V2 | V1 | touches all three backend signatures; Metal is already broken (S1) |
| V3 | V2 | none, new file |
| V4 | V3 | **existing tests may go red immediately** — that is the point, but expect a triage pass |
| V5 | V3 | none; strictly more information than today |
| V6 | V3 | compile-only here |
| V7 | V3, S1 | not verifiable here |
| V8 | V4 | none |

V1–V5 + V8 are one sitting on Linux. V6 is a compile-only change. V7 waits for
S1.

**Expected fallout at V4:** the suite has never been run with a messenger
attached, so the first honest run is likely to surface real, pre-existing
errors. Those are findings, not regressions — triage into `PLAN.md` as bugs
rather than suppressing them into `validation_sink`. Suppression is for messages
that are understood and accepted, never for messages that are merely
inconvenient.

---

## Decided

`--validation-fatal` defaults to **off**: collect every message, count, fail at
the end of the run. Abort-on-first-error stays available as an opt-in flag for
when a specific error needs a live backtrace — it hides every error after the
first, which would make the V4 triage pass needlessly slow.

---

## Review

The overall direction is sound, but the plan is not implementation-ready until
validation availability, shutdown, concurrency and command-line semantics are
made explicit. Without those changes the suite can report a clean validated run
when validation never ran, or lose messages during an exceptional or
asynchronous shutdown.

### R1 — Distinguish requested, effective and required validation · blocking

The graceful Vulkan fallback in V5 conflicts with the test-suite goal. If the
validation layer or a required extension is absent, the current design can
print `0 errors` and exit successfully even though no validation happened.
DX12 has the same class of problem when the debug components are unavailable,
and a requested GPU mode may be downgraded silently on any backend.

Keep the level and availability policy separate:

```cpp
enum class validation_requirement : u8
{
    BEST_EFFORT,
    REQUIRED,
};

struct validation_status
{
    rhi_validation_level requested{rhi_validation_level::ON};
    rhi_validation_level effective{rhi_validation_level::OFF};
    bool available{false};
};
```

Normal application runs can use `ON + BEST_EFFORT`. Tests should use
`ON + REQUIRED`; GPU validation runs should use `GPU + REQUIRED`. Failure to
enable the requested level must then fail before the test starts. The summary
must print both requested and effective levels so a downgrade is visible.

Suggested CLI addition: `--validation-required`. V8 should pass it for every
test run. `--validation=off --validation-required` is invalid and should be
rejected.

### R2 — Complete the Vulkan extension contract · blocking

V5 adds `VK_EXT_debug_utils`, but its GPU path also puts
`VkValidationFeaturesEXT` into `VkInstanceCreateInfo::pNext`. That structure
requires `VK_EXT_validation_features` in the enabled instance-extension list.
Both extensions must be enumerated before use; blindly enabling an unavailable
extension makes `vkCreateInstance` fail before a useful availability report can
be produced.

For `GPU`, enumerate and enable:

- `VK_EXT_debug_utils` for the messenger;
- `VK_EXT_validation_features` for `VkValidationFeaturesEXT`;
- `VK_LAYER_KHRONOS_validation` for validation itself.

Build the `pNext` chain only from available and enabled pieces. Apply the R1
policy when any required piece is missing. Do not report an effective `GPU`
level if only ordinary validation was enabled.

### R3 — Make finalization exception-safe · blocking

V4 runs `cleanup`, drains destruction messages and prints the summary only on
the normal return path from `engine::run`. An exception from script setup,
rendering, presentation or cleanup skips that path. The process still fails,
but validation output and resource-lifetime diagnostics can be lost.

Introduce a run-scoped finalizer that always performs this order:

1. stop new rendering work;
2. wait for submitted GPU work;
3. destroy RHI resources while callbacks are alive;
4. drain backend queues and wait for asynchronous callbacks;
5. print the validation summary;
6. preserve the original exception, while also recording validation failure.

The Vulkan messenger must remain alive through device destruction. DX12 must
drain after GPU completion and on exceptional paths. Metal must wait for all
completion handlers before the sink is read or destroyed.

### R4 — Replace the ambiguous command-line lookup · high

Adding `hasArgument` does not make bare `--validation` work reliably. The
current `getArgument` throws when a valueless option is followed by another
option, for example `--validation --script test.as`.

Use one parser result that distinguishes absence, a bare flag and a value, for
example `{present, optional_value}`. Define and test these cases:

- `--validation`, `--validation=on`, `--validation=`;
- repeated `--validation` options;
- `--no-validation` combined with `--validation=...`;
- `--validation-fatal` while validation is off;
- unknown values and numeric aliases.

Prefer rejecting contradictory or repeated options instead of relying on
argument order.

### R5 — Make `validation_sink` thread-safe · high

The proposed counters, suppression storage and direct printing are not safe for
concurrent callbacks. Vulkan calls can originate from multiple application
threads, and Metal completion handlers are asynchronous.

Synchronize counters, suppression lookup and output. Each emitted message must
remain one complete log record. `configure` must reset all per-run state. The
sink must remain alive until every backend callback has stopped.

### R6 — Qualify and audit suppressions · high

A shared unqualified ID list mixes unrelated Vulkan string IDs, DX12 numeric
IDs and future Metal codes. It also loses the reason why a message is accepted.
A suppressed error can otherwise turn a broken test green indefinitely.

Use a backend-qualified key and reviewable metadata:

```cpp
struct validation_suppression
{
    validation_backend backend;
    std::string id;
    std::string reason;
};
```

Report suppressed warnings and errors separately. Confirm the three existing
DX12 IDs before moving them; do not treat existing filters as proof that the
messages remain valid to suppress. New suppressions should require a concrete
reason in this document or `PLAN.md`.

### R7 — Correct fail-fast guarantees · high

`std::abort()` does not guarantee a backtrace and may run from an asynchronous
callback rather than at the offending API call. It also intentionally skips
cleanup and the end-of-run summary, contradicting the statement that the
summary is always printed.

Describe `--validation-fatal` as fail-fast, not as a guaranteed live backtrace.
Flush the validation record before aborting. A debugger-specific breakpoint can
be used when a debugger is attached; otherwise abnormal termination is the only
guarantee. Explicitly exempt fail-fast and other abnormal termination from the
"summary always printed" requirement.

### R8 — Harden DX12 queue handling · high

Define mappings for every `D3D12_MESSAGE_SEVERITY`, including `CORRUPTION` and
`MESSAGE`. Drain only after the relevant GPU fence has completed; GPU-based
validation messages may not exist immediately after command submission. Drain
again during finalization and on error paths where practical.

Also inspect `GetNumMessagesDiscardedByMessageCountLimit`. Any discarded
message means validation results are incomplete and must fail a `REQUIRED` run.

### R9 — Treat Metal environment variables as platform tooling · medium

The plan calls `MTL_DEBUG_LAYER` and `MTL_SHADER_VALIDATION` a documented
mechanism, but they should not be treated as a stable engine API without a
verified Apple contract for the supported deployment targets. Prefer setting
them in the macOS test launcher before process start. Keep
`MTLCommandBufferDescriptor.errorOptions` and `commandBuffer.error` as the
runtime backend path, with availability checks for the minimum macOS version.

The command buffer must be created with the descriptor, and finalization must
wait for its completion handler. Metal errors also need a stable
backend-qualified ID derived from their domain and code.

### R10 — Add durable regression coverage · high

The proposed hand-made `report()` call and temporarily broken barrier prove the
first implementation but do not prevent regression. Add automated coverage for:

- the full command-line parsing matrix;
- severity mapping and suppression accounting;
- state reset between runs;
- concurrent reports;
- missing layer and extension behaviour in both availability policies;
- requested versus effective level reporting;
- queue overflow or discarded-message handling;
- `OFF` creating no validation callback or queue;
- one synthetic backend validation error producing `EXIT_FAILURE`.

Keep the deliberate real API error as a platform integration test where it can
be made deterministic. Unit-test the sink and parser independently of a GPU.

### R11 — Bound and normalize validation output · medium

One defect can produce thousands of identical messages. Message text may be
multiline and an API may provide no stable ID. Printing every raw occurrence
can hide the first useful error and exhaust CI log limits.

Normalize each record to a greppable form, provide a backend-qualified fallback
ID, group duplicates by `{backend, id, message}`, print the first small number
of occurrences and retain the full count in the summary. Log truncation must
not affect failure accounting.

### Recommended implementation order

1. R1 and R4: define policy, status and unambiguous parsing.
2. V2 and R5: pass configuration into RHI and create a thread-safe sink.
3. R3: establish guaranteed finalization before backend callbacks are added.
4. V5 with R2, then verify Vulkan end to end on Linux.
5. V6 with R8, compile on Windows and run on a Windows validation machine.
6. V7 with R9 after S1, then run on the supported macOS versions.
7. V8 with `--validation-required` and the R10 regression tests.

The implementation is ready to start when R1-R5 have concrete acceptance
criteria. The test-suite claim is complete only when absence, downgrade,
discarded messages and asynchronous finalization all fail closed in
`REQUIRED` mode.

---

## Response to review

Verdict per item. "Take" means it changes the plan, "simplify" means the concern
is real but the proposed machinery is heavier than this codebase warrants,
"defer" means it is a separate piece of work that should not gate V1-V8.

| | verdict | why |
|---|---|---|
| R1 requested / effective / required | **take**, simplified | the one hole that breaks the whole point of the plan |
| R2 `VK_EXT_validation_features` | **take** | factually correct; the plan as written would fail `vkCreateInstance` on the `gpu` path |
| R3 exception-safe finalization | **take**, simplified | real gap; a scope guard covers it, a six-stage finalizer does not exist to be written yet |
| R4 command-line parsing | **simplify** | the trap is real, the fix is to not accept the ambiguous form |
| R5 thread safety | **take**, reduced | engine is single-threaded today; a mutex is three lines of insurance |
| R6 qualified suppressions | **take** | cheap, and it is what makes a suppression reviewable |
| R7 fail-fast wording | **take** | documentation fix |
| R8 DX12 hardening | **take**, extended | correct, and it misses the better fix for discarded messages |
| R9 Metal env vars | **defer** | blocked on S1 either way; the launcher-script point is accepted for when it lands |
| R10 regression coverage | **defer**, replaced | there is no unit-test framework in this repo; adding one is its own decision |
| R11 output bounding | **take**, folded into V3 | same code as the counters |

### R1 — accepted, without the second enum

The failure mode is real and is the reason the plan exists: today the Vulkan
path prints "validation is disabled" to `cerr` and carries on, so a machine
without the Vulkan SDK would report `0 errors` from a run that validated
nothing. A green suite that proves nothing is worse than a red one.

Simplification: `validation_requirement` as a separate enum buys nothing over a
`bool required` in `rhi_init_desc`, since it has exactly two states and no third
is plausible. Keep `validation_status` — three fields, and the RHI must expose
it:

```cpp
struct validation_status
{
    rhi_validation_level requested { rhi_validation_level::ON };
    rhi_validation_level effective { rhi_validation_level::OFF };
    std::string detail;   // why it was downgraded, empty when it was not
};

virtual validation_status getValidationStatus() const = 0;
```

`required && effective < requested` → throw at the end of `init`, before the
first frame. Summary always prints both levels, so a downgrade is visible even
in `BEST_EFFORT`. `--validation=off --validation-required` is rejected at parse
time.

### R2 — accepted, and it changes V5

`VkValidationFeaturesEXT` needs `VK_EXT_validation_features` in the enabled
instance extensions. V5 as written enables only `VK_EXT_debug_utils` and would
have failed `vkCreateInstance` on the `gpu` path. Both extensions get enumerated
before use, the `pNext` chain is built only from what is present, and anything
missing feeds the R1 status rather than being ignored.

Worth noting for the implementation: `VK_EXT_validation_features` is deprecated
in favour of `VK_EXT_layer_settings` (Vulkan 1.3.272+). The engine requests API
1.1, so the older extension is the right target now; the newer one is the path
if the target version moves.

### R3 — accepted as a scope guard

The gap is real: an exception anywhere in `engine::run` skips the summary today.
The six-stage finalizer describes a lifecycle the engine does not have — there
is no "stop new rendering work" phase, `cleanup()` is one call, and nothing
waits on asynchronous callbacks because nothing is asynchronous.

What lands instead: an RAII guard in `engine::run` whose destructor runs
`cleanup()`, drains the backend queue and prints the summary. Exceptions
propagate normally and `main` still returns `EXIT_FAILURE`; the validation
record survives either way. The Vulkan messenger outliving the device is already
in V5.

`cleanup()` itself is not exception-safe today. That is a pre-existing defect
adjacent to `PLAN.md` **S2**, not something this plan fixes.

### R4 — the ambiguity gets removed, not parsed

Correct that `--validation --script test.as` throws today: `getArgument` sees
the next token starting with `-` and raises "Missing value for argument". A
tri-state parser plus a duplicate-option policy is a rewrite of
`commandline_args` for every flag in the engine, which is not this plan's job.

Decision: **drop bare `--validation`.** Accepted forms are `--validation=<value>`
and the valueless `--no-validation`. `hasArgument` from V1 is still needed for
`--no-validation` and `--validation-fatal`. The R4 case list becomes the
acceptance criteria for those two forms only. If `commandline_args` is
rewritten later, it is its own item.

### R5 — mutex, nothing more

There is no `thread`, `async` or `mutex` anywhere in `src/` — the engine is
single-threaded and the "async" texture load in `AGENTS.md` is aspirational. So
the concurrency risk is hypothetical today, with one exception worth covering: a
driver may call the debug messenger from its own thread. A `std::mutex` around
report and counters costs nothing on a path that only runs when validation is
on. The rest of R5 — callback lifetime, sink outliving the backends — is R3's
ordering, not separate work.

### R6, R7, R11 — taken as written

R6: suppressions become `{backend, id, reason}`, and suppressed errors are
counted separately from suppressed warnings. The three DX12 IDs move across
unaudited and marked as such — they cannot be confirmed from Linux.

R7: `--validation-fatal` is described as fail-fast, flushes the record before
aborting, uses a debugger break when one is attached, and is explicitly exempt
from "the summary is always printed".

R11: dedupe by `{backend, id}`, print the first few occurrences, keep full
counts in the summary, truncation never affects the error count. Same data
structure as the counters, so it lands in V3.

### R8 — accepted, plus the fix it does not mention

Map every `D3D12_MESSAGE_SEVERITY`, drain after fence completion, drain again in
the finalizer: all correct. `GetNumMessagesDiscardedByMessageCountLimit` detects
the problem, but the queue's storage limit can also be removed outright —
`SetMessageCountLimit(D3D12_MESSAGE_ID_UNKNOWN, -1)` at setup. Do both: raise
the limit, and still fail a `REQUIRED` run if anything was discarded.

### R10 — the concern is right, the remedy needs infrastructure this repo lacks

"Unit-test the sink and parser independently of a GPU" assumes a unit-test
framework. There is none: `run_tests.sh` runs the app over AngelScript scenes
and checks the exit code, and no test library is linked anywhere in `cmake/`.
Introducing one is a reasonable thing to do and a bad thing to smuggle in under
a validation change.

What lands now instead: `--validation-selftest`, a mode that runs the parser
matrix and a set of synthetic `report()` calls, asserts the resulting counters,
prints a summary and exits non-zero on mismatch. Added to the suite as one more
entry, no new dependency, and it covers most of the R10 list: parsing matrix,
severity mapping, suppression accounting, state reset, concurrent reports,
`OFF` emitting nothing, and a synthetic error producing `EXIT_FAILURE`.

Not covered by it, and honestly not covered at all until there is real
infrastructure: missing-layer and missing-extension behaviour under both
policies. That needs an environment where the layer can be hidden
(`VK_LAYER_PATH` manipulation in the launcher is the cheap approximation) and
belongs with `PLAN.md` **S9**, the CI matrix.

**New `PLAN.md` item to file: S11 — no unit-test framework.**

---

## Revised step list

Amendments to V1-V8. Steps not listed are unchanged.

| step | change |
|------|--------|
| V1 | scope reduced: `hasArgument` only, for `--no-validation` and `--validation-fatal`. Bare `--validation` is not supported |
| V2 | `rhi_init_desc` gains `bool required`; `validation_status` and `getValidationStatus()` added to `dynamic_rhi`; `--validation-required` parsed; `off + required` rejected |
| V3 | sink gains a mutex, `{backend, id, reason}` suppressions, dedupe with occurrence counts, separate suppressed-error and suppressed-warning counters, and requested/effective levels in the summary |
| V3b | **new** — `--validation-selftest`: parser matrix plus synthetic reports, asserted counters, non-zero exit on mismatch |
| V4 | summary and drain move into an RAII guard so they run on the exception path; fail-fast is exempt and flushes first |
| V5 | `VK_EXT_validation_features` enumerated and enabled for the `gpu` path; every missing piece reported through `validation_status` instead of a `cerr` line |
| V6 | all severities mapped; `SetMessageCountLimit(D3D12_MESSAGE_ID_UNKNOWN, -1)`; discarded-message count checked and fatal under `required`; drain after fence completion and in the finalizer |
| V7 | unchanged, still blocked on S1; env vars move to the macOS launcher script rather than `setenv` inside `engine::run` |
| V8 | every test run passes `--validation=on --validation-required`; `V3b` added to the suite; `VALIDATION=gpu` second pass unchanged |

### Order

1. **V1 + V2** — parsing, level, requirement, status plumbing.
2. **V3 + V3b** — sink and its self-test. Verifiable with no GPU work at all.
3. **V4** — finalizer guard and exit code.
4. **V5 with R2** — Vulkan, end to end on Linux. First honest run of the suite.
5. **V8** — suite integration, then triage whatever step 4 surfaced.
6. **V6 with R8** — DX12, compile-only here.
7. **V7 with R9** — Metal, after S1.

The review's order puts R3 before any backend callback exists. Moving the sink
and its self-test ahead of it is cheaper: both are testable with no GPU, and the
guard is easier to write once there is something whose output proves it ran.

---

## Implementation log

### V1 + V2 — done, Linux/Vulkan

`hasArgument` added to `commandline_args`. `rhi_validation_level`,
`rhi_init_desc`, `validation_status` and `rhi_init_desc_from_commandline()` in
`render_types.{hpp,cpp}`. `dynamic_rhi::init` takes the desc;
`beginValidationSetup` / `finishValidationSetup` in the new `dynamic_rhi.cpp`
hold the requested-versus-effective policy for all backends. Parsing happens at
the top of `engine::run`, before the script runs, so a bad flag fails before a
window exists. `#ifndef NDEBUG` is gone from both the Vulkan and DX12 paths.

Accepted forms, verified: `--validation=off|on|gpu|0|1|2`, `--no-validation`,
`--validation-required`, `--validation-fatal`. Rejected, verified: bare
`--validation`, `--validation off`, `--validation=`, an unknown value,
`--validation` together with `--no-validation`, and `--validation-required` or
`--validation-fatal` while validation is off.

Effective levels reported by this step: Vulkan `ON` when the layer is present,
`OFF` otherwise, never `GPU` (V5 owns that). DX12 `ON` for the debug layer and
`GPU` when `SetEnableGPUBasedValidation` succeeds. Metal always `OFF`.

**Layer availability.** The first `--validation-required` run failed with
`VK_LAYER_KHRONOS_validation is not present` — `vulkan-validationlayers` was not
installed on the Linux dev machine, so every Debug run here had been validating
nothing while the engine reported it in one `cerr` line and carried on. R1's
failure mode, observed before V5 was written. The package has since been
installed and `--validation=on --validation-required` now reports
`effective=on`.

**Finding — one real error, already present.** With the layer active the suite
emits `VUID-VkShaderModuleCreateInfo-pCode-08740` fifteen times, once per
pipeline creation: SPIR-V declares the `DrawParameters` capability while neither
`shaderDrawParameters` nor `VK_KHR_shader_draw_parameters` is enabled at device
creation. Filed as `PLAN.md` **B15**. The suite still exits 0 — V4 is what
changes that.

**Finding — exceptions after RHI init crash the process.** `--validation-required`
was originally thrown from `finishValidationSetup`, inside `renderer::add_window`,
inside an AngelScript-called binding. Two problems surfaced at once: the VM
swallows the message down to `Caught an exception from the application`, and the
process then died with SIGSEGV because `renderer::cleanup()` only runs on the
normal return path. Filed as `PLAN.md` **B16**.

Reworked accordingly: `finishValidationSetup` records `validation_status::satisfied`
and prints the reason instead of throwing, and `engine::run` fails on it after
the script returns and before the first tick. That path calls `cleanup()`
explicitly as a stopgap — V4's RAII guard is what removes the stopgap and covers
every other throw site.

Consequence for V8: adding `--validation-required` to the suite is now possible,
but should land together with V4 so that the fifteen B15 errors are counted
rather than printed and ignored.

Not verified: DX12 and Metal, no machine to build them on. The DX12 changes are
edits to a file that cannot be compiled here.

### V3 + V3b — done, Linux/Vulkan

`validation_sink.{hpp,cpp}`: singleton, `std::mutex` around every counter and the
printing, `{backend, id, reason}` suppressions, dedupe keyed on `backend/id` with
per-record occurrence counts, printing capped at `g_maxPrintsPerMessage` (3) with
a one-line notice that the rest are counted, separate suppressed-error and
suppressed-warning counters, and `printSummary()` reporting requested and
effective levels. `configure()` resets everything, so a run cannot inherit the
previous one's state. With the level `OFF` the sink drops reports outright.

`validation_selftest.{hpp,cpp}` behind `--validation-selftest`: 59 checks
covering the parser matrix (13 accepted forms, 8 rejected), severity counting,
duplicate accounting, backend-qualified suppression, reset on reconfigure, `OFF`
dropping reports, and 1000 reports from 4 threads arriving intact. It runs before
anything else in `engine::run` and returns without touching a window or a device.

Verified the self-test can fail: a deliberately wrong expectation produced
`[selftest] FAIL parse [app] level=on expected=gpu`, `1 failures`, exit 1.

To make the sink testable without a device, the argument scanning moved out of
`commandline_args` into free functions `get_argument(args, name)` /
`has_argument(args, name)`; the member functions now forward to them, and
`parse_rhi_init_desc(args)` takes a plain vector so the self-test can feed it a
synthetic argv. `rhi_init_desc_from_commandline()` is a one-line wrapper.

`beginValidationSetup` configures the sink and `finishValidationSetup` hands it
the status, so the plumbing is in place before V5 and V6 start emitting. Nothing
calls `printSummary()` at end of run yet and no backend reports into the sink
yet — V4 wires the first, V5 and V6 the second.

Behaviour pinned by the self-test that was previously undefined: repeated
`--validation=` options take the first occurrence.

### V4 — done, Linux/Vulkan

`run_finalizer` in `engine::run`: its destructor runs `renderer::cleanup()` and
`validation_sink::printSummary()`, so both happen on the normal path and during
stack unwinding. `cleanup()` is wrapped in try/catch inside the destructor —
throwing out of a destructor during unwinding would call `std::terminate` and
lose the original exception. The `cleanup()` stopgap added in V2 is gone; this
closes `PLAN.md` **B16**.

The error-count check lives in `main`, after `run` returns, rather than at the
end of `run`. Putting it inside `run` needed an extra scope around the whole
function body so the guard would fire before the check; `main` already owns
`EXIT_FAILURE` and gets the ordering for free.

Verified end to end with a temporarily injected report:

```
[validation][vulkan][error] TEMP-INJECTED: temporary check of the exit-code path
[validation] summary: requested=on effective=on
[validation] 1 errors, 0 warnings (0 suppressed)
[validation]   error vulkan/TEMP-INJECTED x1
Validation reported 1 error(s)          exit=1
```

The injection was reverted. Also verified that the exception path
(`--validation=gpu --validation-required`) exits 1 with cleanup and summary done
and no crash, where before V4 it segfaulted.

The startup line is now `[validation] enabled: ...` and the end-of-run one
`[validation] summary: ...`, so the two are distinguishable in a test log.

Still nothing reports into the sink from a backend — V5 and V6. Until then every
run ends with `0 errors` honestly: the layer is active, but its messages still go
to the loader's default output rather than through the sink.

### V5 — done, Linux/Vulkan

`VK_EXT_debug_utils` is enumerated and enabled whenever the level is not `OFF`,
a `VkDebugUtilsMessengerCreateInfoEXT` is chained into
`VkInstanceCreateInfo::pNext` so instance creation and destruction are covered,
and the real messenger is created right after the instance and destroyed after
`_device.reset()` but before `vkDestroyInstance`. The callback maps severity to
`validation_severity`, passes `pMessageIdName` as the id and always returns
`VK_FALSE`. For `gpu`, `VkValidationFeaturesEXT` with `GPU_ASSISTED` and
`SYNCHRONIZATION_VALIDATION` chains onto the messenger struct.

Every piece is checked before use and any missing one lands in
`validation_status::detail` rather than being ignored, so `--validation-required`
sees it. The layer-presence check, the extension checks and the messenger
creation each have their own detail message.

**One correction against R2 while implementing.** R2 was right that
`VK_EXT_validation_features` must be enabled, but enumerating with
`vkEnumerateInstanceExtensionProperties(nullptr, ...)` does not find it: the
extension is provided by the validation layer, so it only appears when
enumerating with the layer name. The first attempt reported
`VK_EXT_validation_features is not present` on a machine where it was available.
Instance extensions are now enumerated twice — globally, and again for
`VK_LAYER_KHRONOS_validation` when the layer is present — and the lists merged.

Verified: `--validation=on` reports `effective=on`, `--validation=gpu`
`effective=gpu`, `--no-validation` emits nothing at all, and the exit code
follows the error count.

**Fallout, as predicted.** The suite is now red: 5 of 14 tests fail, each on one
`VUID-VkShaderModuleCreateInfo-pCode-08740` (`PLAN.md` **B15**). Two more
findings came out of the same run:

- `SYNC-HAZARD-WRITE-AFTER-READ`, three per run under `--validation=gpu`: the
  swapchain layout transition is not covered by the acquire-semaphore wait.
  Filed as **B17**.
- `WARNING-Shader-OutputNotConsumed`, three to five per run: vertex input state
  describes attributes the vertex shader does not read. Warnings do not affect
  the exit code. Filed as **B18**.

None of the three is a regression from this work; all three predate it and were
invisible because no messenger existed. The suite stays red until B15 is fixed —
that is the decision to make before V8, not a reason to suppress the message.

### B15 fixed — the suite is green again

`SV_VertexID` was the source: Slang lowers it to `gl_VertexIndex - gl_BaseVertex`
for HLSL semantics, and `gl_BaseVertex` pulls in the `DrawParameters` capability.
The three shaders using it (`triangle`, `cube`, `deferred_lighting`) are exactly
the ones behind the five failing tests.

Fixed by querying `VkPhysicalDeviceShaderDrawParametersFeatures` and chaining it
into device creation, next to the existing descriptor-indexing chain. All 14
tests now report `0 errors` under the default `--validation=on`, and
`run_tests.sh` passes.

`--validation=gpu` is still red on **B17** (`SYNC-HAZARD-WRITE-AFTER-READ` x4),
which is a separate defect and not in the default suite path.

### B17 fixed — clean under `--validation=gpu` too

The acquire-side layout transition took its source stage from the tracked
resource state (`BOTTOM_OF_PIPE` for `PRESENT`, `TOP_OF_PIPE` for `UNDEFINED`),
neither of which is ordered against the acquire-semaphore wait at
`COLOR_ATTACHMENT_OUTPUT`. For swapchain-backed targets in those two states the
barrier now uses `COLOR_ATTACHMENT_OUTPUT` as its source stage, matching the
wait. Every other barrier is untouched.

All 14 tests now report `0 errors` under `--validation=gpu --validation-required`
as well as under the default level. The only remaining message is B18's
`WARNING-Shader-OutputNotConsumed`, three to five per run, which does not affect
the exit code.

This makes V8 straightforward: both levels are clean, so `--validation-required`
can go into the suite without a pre-existing failure to explain away.

### V6 — done, compile-only claim NOT possible

`ID3D12InfoQueue` is now a member, `drainValidationMessages()` reads it at the
end of `execute()` — after the frame fence has been waited on, so GPU-based
validation messages exist by then — and again in the destructor before the queue
and device are released. Every `D3D12_MESSAGE_SEVERITY` is mapped
(`CORRUPTION` and `ERROR` to error, `WARNING` to warning, `INFO` and `MESSAGE`
to info). The storage-filter limit is lifted with
`SetMessageCountLimit(UINT64(-1))`, and `GetNumMessagesDiscardedByMessageCountLimit`
is still checked — a growing count reports an error through the sink, so a
`--validation-required` run fails on incomplete validation results rather than
passing quietly.

`SetBreakOnSeverity` keeps only `CORRUPTION` and `ERROR`, and only under
`IsDebuggerPresent()`. The three hard-coded `DenyIds` moved out of the queue
filter into `validation_sink` suppressions, keyed by backend and carrying the
reason "carried over from the previous hard-coded deny list, not re-audited" —
they are now counted and visible in the summary instead of silently dropped. The
`INFO` severity deny stays in the queue filter, since that is pure volume.

**This code has never been compiled.** There is no Windows machine here. One
error was caught by reading the API rather than by a compiler:
`SetMessageCountLimit` takes a single `UINT64`, not an id plus a limit as first
written. Assume there are more; the first Windows build is the real test.

### V8 — done, Linux verified

`run_tests.sh` and `run_tests.bat` run `--validation-selftest` before the scene
tests and pass `--validation=${VALIDATION:-on} --validation-required` to every
test. `VALIDATION=off` switches to `--no-validation` instead, since
`--validation-required` with validation off is rejected at parse time.

Verified on Linux: default, `VALIDATION=gpu` and `VALIDATION=off` all pass, and
the selftest line appears in every run.

Verified that the suite fails closed. With `VK_LAYER_PATH=/nonexistent` the
layer disappears and the run stops at the first test:

```
[validation] 'on' was requested with --validation-required, but only 'off' is
available: VK_LAYER_KHRONOS_validation is not present
[FAIL] tests/cube_gpu_test.as
```

That is R1's whole point demonstrated end to end: a machine that cannot validate
no longer reports a green suite.

`AGENTS.md` documents the `VALIDATION` variable and the package requirement.

---

## State

| step | status |
|------|--------|
| V1 V2 V3 V3b V4 V5 V8 | done, verified on Linux/Vulkan |
| V6 | written, never compiled |
| V7 | open again — Metal backend restored, `PLAN.md` S1 |

Fixed along the way: **B15**, **B16**, **B17**. Open: **B18**
(`WARNING-Shader-OutputNotConsumed`, warnings only), **S11** (no unit-test
framework), and the R10 gap that needs it — missing-layer behaviour is now
covered by hand, not by a test.

### V7 dropped — Metal archived

S1 was decided on 2026-08-20: macOS/Metal leaves the supported build matrix.
`src/lucus-graphics/metal/` is now `archive/metal/`, the macOS build and Metal
shader scripts are in `archive/scripts/`, and configuring on macOS fails with a
message pointing at `archive/README.md`.

V7 is therefore dropped, not deferred. The design notes stay in this document as
the starting point if the backend is ever revived — the env-vars-in-launcher
point from R9, `MTLCommandBufferDescriptor.errorOptions` as the runtime path, and
the completion-handler wait that R3's asynchronous finalization was written for.

`validation_backend::METAL` stays in `validation_sink`: it costs nothing and is
one less thing to reintroduce.

**The validation plan is now complete for every supported backend** — Linux/Vulkan
verified end to end, Windows/DX12 written but never compiled.
