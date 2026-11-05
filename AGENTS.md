# Slim UI — AGENTS.md

Pure C, header-only Direct2D + DirectWrite UI toolkit. Single-file
implementation idiom (`SUI_IMPLEMENTATION` in exactly one TU).

## Documentation rule (read first)

`README.md` is for people. `AGENTS.md` is for LLMs. `docs/reference.md` is the
full API reference, for both.

**`README.md` stays short and comfortable to read.** It opens with what people
gain from using Slim UI. Use plain language and short sentences. Keep
maintenance rules and anything addressed to agents out of it; they belong here.
It isn't a reference, so don't add API detail to it. If a change makes something
it says untrue, fix it in the same commit. Its `hello.c` example, snippets and
commands must keep working, so build and run them after changing anything they
use. The sizes it quotes come from real builds, so re-measure them when they
change noticeably.

**Keep `docs/reference.md` current — but only for things it already covers.**

If a change modifies something the reference *does* describe (a documented
symbol, feature macro, constant, control, event handler, lifecycle
contract, declarative API behavior, etc.), update the reference in the same
commit so the doc still matches reality.

If a change touches something the reference *doesn't* currently mention
(a new internal helper, a new private control field, a new feature the
reference isn't scoped to cover), **do not silently add it to the reference**.
The reference's scope is intentional; do not grow it on the side.

**Significant additions — ask first.** If the change introduces something
that would warrant a *new section or category* in the reference (e.g. a new
public subsystem, a new control kind that doesn't fit existing tables, a
new feature macro, a new top-level concept), the LLM must surface this
to the user and explicitly ask whether to add it to the reference. Do not
assume yes; do not assume no. Surface the question before declaring the
task complete. Out-of-scope additions otherwise belong in commit
messages, source comments, or a deliberate follow-up change to the reference.

In short: keep documented things accurate; don't expand what's
documented as a side effect of unrelated work; for significant new
surface, ask the user.

The reference is the contract for consumers. Stale docs cause integration
drift; over-eager docs cause API surface creep.

## Product neutrality

**Product-neutral.** This library must not reference any consuming
product: no product names, no consumer files or paths, no consumer
features as examples and no consumer plan/task numbers — in code,
comments, docs, tests or build files. Describe each feature
generically; consumer-specific code, docs and tests live with the
consumer.

## Layout

```
slim-ui/
├── README.md                       ← for people: what it is and how to start
├── AGENTS.md                       ← this file
├── docs/reference.md               ← the full API reference (keep current)
├── logo.svg, logo.png              ← the logo; the demo embeds the PNG
├── CMakeLists.txt                  ← `slim_ui` interface target, demo and tests
├── demo/main.cpp                   ← exercises every control; treat as smoke test
├── demo/shader_effect.cpp          ← the Shader page's Direct2D effect; each shader_*.hlsl compiles to shader_*_ps.h
├── examples/tiny.c                 ← the smallest app; the README quotes its size
├── dotnet/                         ← C#/.NET wrapper and AOT-capable demo
├── tests/
│   ├── sui_tests.c                 ← utest.h-based regression suite
│   ├── sui_vsync_tests.c           ← the vsync pump
│   ├── sui_popup_clock_tests.c     ← the combo popup's frame clock
│   ├── utest.h                     ← sheredom/utest.h (Unlicense)
│   ├── CMakeLists.txt              ← test project
│   └── build-tests.ps1             ← configure + build + run
└── include/slim_ui/
    ├── slim_ui.h                   ← umbrella header, registries, types
    ├── sui_renderer.h              ← D2D + DWrite primitives
    ├── sui_corner.h                ← continuous rounded corner paths
    ├── sui_controls.h              ← host lifecycle + event dispatch + draw_all
    ├── sui_decl.h                  ← declarative API + flex (opt-in: SUI_FEATURE_DECL)
    ├── sui_layout.h                ← SuiLayout helpers
    ├── sui_vsync.h                 ← opt-in vblank pump (SUI_FEATURE_VSYNC)
    ├── sui_d2d.h / sui_uia*.h      ← D2D / UIA glue
    └── sui_ctrl_*.h                ← per-control add / draw / event handlers
```

## Coding conventions

- **Pure C** (the headers compile under both C and C++ TUs — see the
  `SUI_IID_REF` macro that flips between `&iid` and `iid` for D2D / DWrite
  factory calls).
- **No allocator beyond `sui_malloc / sui_free`**. With `SUI_NO_CRT` they
  route through `HeapAlloc` on the process heap.
- **Static functions everywhere** (single-file implementation idiom).
- **No CRT string ops in hot paths** — use the helpers in `slim_ui.h`
  (`sui__streq`, `sui__strncpy_z`).
- **DPI**: imperative APIs take physical pixels; declarative APIs take
  DIPs and scale via `sui__decl_dpi_s(host)`.
- **State storage**: control data lives in the union `SuiControl::d.*`;
  do not heap-allocate per-control state.

## Public-symbol additions

When adding a new control type or public function:

1. Add the implementation in the appropriate `sui_ctrl_*.h` (or new
   header, included from `slim_ui.h`).
2. Add the type to `SuiCtrlType` and the union slot to `SuiControl::d`.
3. Wire dispatch in `sui_controls.h` (hit-test, mouse-down/up, keyboard,
   draw, cursor).
4. If declarative-friendly, add the `sui_<name>` leaf to `sui_decl.h`
   (gated by `SUI_FEATURE_DECL`; consumers without that macro skip the
   declarative API entirely).
5. Only update the reference if the new symbol falls inside something it
   already documents (e.g. a new control belongs in the existing factory
   tables; a new constant fits the constants reference). New *categories*
   of public surface stay out of it until that's a deliberate, separately
   reviewed decision.
6. Add coverage to `demo/main.cpp`.

## .NET wrapper sync

Keep `dotnet/` in sync with the C Slim UI source. Any change to public
controls, control state, event behavior, layout semantics, theme /
typography registry behavior, DPI handling, image handling, or lifecycle
contracts must be reflected in the native bridge and C# wrapper in the
same change when the wrapper exposes or depends on that behavior.

When adding or changing a demo-covered control in `demo/main.cpp`, update
`dotnet/samples/SlimUi.Sample/Program.cs` so the .NET demo continues to
replicate the native SUI demo. The Shader page is the exception: it uses
Direct2D directly, which the wrapper doesn't expose. If the native control has persistent
runtime state across rebuilds, update the .NET bridge state preservation
logic too.

## Build / test

`demo/main.cpp` is the smoke test. The toolkit itself produces no
artifacts — it ships as headers. Verify by building the demo
(`demo/build.ps1`), the .NET wrapper (`dotnet/build.ps1`) and the tests.

### Unit tests (`tests/`)

Pinned-behavior regression suite using `utest.h`
(public-domain single-header framework). Run from `tests/`:

```powershell
.\build-tests.ps1          # configure + build + run
.\build-tests.ps1 -NoRun   # build only
.\build-tests.ps1 -Filter <pattern>   # run a subset
.\build-tests.ps1 -List    # enumerate tests without running
```

Conventions:

- **Pure-math tests** (no fixture) cover layout helpers, color math,
  string utilities, scheme registry.
- **Host-fixture tests** (`UTEST_F(host_fx, …)`) borrow a suite-owned invisible
  top-level HWND + own a field-initialized `SuiRenderer` (no D2D factory) +
  zeroed `SuiControlHost`. Tests must not assume the renderer has a
  live DWrite factory — text-measuring helpers short-circuit to 0 in
  that path.
- **Native Windows fixtures** reuse one HWND per executable, with a fresh host
  and renderer per case. Stop workers and timers before returning the window;
  `win32_test_support.h` clears native focus, capture and queued messages.
- **Phase-N placeholders** are written as failing-by-design tests
  wrapped in `UTEST_SKIP("Phase N: …")` so the suite stays green while
  surfacing pending work in the run output. Replace `UTEST_SKIP` with
  real assertions when the corresponding fix lands.
- Add a test for every public-API behavior you preserve when refactoring.

## Project default tints

- Light scheme → blue accent.
- Dark scheme → orange accent.
