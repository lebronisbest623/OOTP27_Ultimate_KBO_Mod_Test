# OOTP27 KBO Launcher Architecture

This document describes the current structure of the repository. The governing
principles live in `docs/CONSTITUTION.md`; when this file and the constitution
disagree, the constitution wins.

## Runtime Layers

The project has two runtime layers.

- `src/KBOLauncher/Program.cs` is the managed entry point. It delegates to the
  `LauncherApp` application layer, locates OOTP, prepares runtime data, checks
  the target process, and injects the native DLL only when the guards allow it.
- `native/KBOFix.c` is the native patch shell. It includes public module
  headers and assembles native startup order, but gameplay behavior lives in
  named modules under `native/src/`.

Most KBO rule emulation lives in the native layer because the project patches a
closed game process. The architecture therefore favors explicit modules and
fail-closed guards over generic abstractions.

## Current Native Shape

The native layer is now a multi-translation-unit build.

`native/build.ps1` compiles `native/KBOFix.c` and every `*.c` file under
`native/src/`, emits dependency files, and links the resulting objects into
`native/bin/KBOFix.dll`.

`native/KBOFix.c` should remain a thin shell. Its responsibilities are:

- process-wide preprocessor setup needed by Win32/WebView2 headers
- OOTP ABI and build-verification includes
- public module header includes
- startup and patch-installation ordering

It should not own domain policy, persistence, scanners, UI rendering, or large
helper bodies.

## Native Source Layout

The important native folders are:

```text
native/
  KBOFix.c                         native shell and startup include surface
  build.ps1                        native build/link script
  tests/                           native helper tests
  src/
    bootstrap/                     ABI offsets, typedefs, startup declarations
    build_verify/                  native supported-build verification
    core/                          logging, paths, dates, flags, SQL helpers
    runtime_memory/                process-memory safety helpers
    team/                          team lookup, names, roster-array mutation
    foreign/                       foreign-player policy subsystem
    military_service/              military-service loan subsystem
    custom_events/                 event scheduling, scanning, dispatch
    allstar/                       single-division All-Star support
    fa_*                           FA rules, filing, market, compensation
    amateur_player_quality/        amateur reputation assignment
    season_phase_monitor/          read-only phase monitor
    patch_helpers/                 low-level patch helpers
    hook_stubs/                    generated/executable hook stubs
    patch_installers/              byte patches and detour installers
    hotkey_window/                 in-game F2 hub UI
    entrypoint/                    native startup and runtime install flow
```

New native code should be added as a focused `.c/.h` module under the owning
subsystem. Headers expose the narrow contract; implementation details stay
`static` in the `.c` file.

## Managed Launcher

`Program.cs` calls `LauncherApp.Run(args)`. `LauncherApp` is a partial
application shell split by flow:

- `LauncherApp.cs` parses options, builds run context, applies guards, and
  delegates to attach or launch flow.
- `LauncherAttachFlow.cs` handles explicit attach paths.
- `LauncherLaunchFlow.cs` handles launch and injection paths.
- `LauncherRunSetup.cs` prepares runtime files and default flag state.
- Infrastructure modules under `src/KBOLauncher/Infrastructure/` own build
  guard reads, process discovery, injection, paths, seed copying, and flag JSON.

The managed layer must not implement KBO gameplay rules. It prepares data and
controls whether the native runtime is allowed to load.

## Supported Build Manifest

Supported OOTP builds are sourced from:

```text
config/ootp-supported-builds.json
```

`tools/generate-supported-builds.ps1` generates:

- `src/KBOLauncher/Infrastructure/OotpSupportedBuilds.Generated.cs`
- `native/src/build_verify/supported_builds.generated.h`
- `native/src/build_verify/supported_builds.generated.c`

`KBOLauncher.Tests/OotpSupportedBuildManifestTests.cs` verifies that the JSON,
managed generated source, and native generated source match.

The remaining build-related debt is the build-specific RVA mapping in
`native/src/build_verify/build_verify.c`. The supported-build list is generated,
but the per-build RVA map is still hand-maintained C code. Future OOTP build
updates should move those mappings into generated data as well.

## Safety Guards

KBOFix injection is fail-closed.

- The launcher reads the target `ootp27.exe` PE header and checks timestamp plus
  image size before injection.
- Explicit injection requests fail visibly on unsupported or unreadable builds.
- Default no-argument runs do not inject into unsupported targets.
- The native DLL repeats build verification if loaded by another path.
- Injection requires the current `.lg` save to contain the Ultimate KBO roster
  marker in `description.txt`.

The required marker is:

```text
https://github.com/lebronisbest623/OOTP27_Ultimate_KBO
```

The native save-scoped path helper follows the same fail-closed rule: it uses
the current save path reported by OOTP memory. It must not guess the newest save
folder as an output destination.

## Runtime Flags

Runtime booleans live in:

```text
%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json
```

Native callers still use `read_kbo_localappdata_flag_file("name.txt")` for
compatibility, but that function maps the old filename form to JSON keys.
Status files, command files, seeds, CSVs, and save-scoped persistence remain
separate files.

The F2 hub intentionally exposes recovery flags so users can disable risky
runtime paths without editing JSON by hand. The current debt is that flag
metadata is duplicated between the managed `KboFlags` list and the F2 hub's
native flag table. The next cleanup should introduce one flag manifest and
generate both surfaces.

## Native Subsystems

### Core

`native/src/core/` owns shared infrastructure only: logging, atomic writes,
date/text helpers, SQL escaping, save-scoped paths, runtime flags, league
context, league-event helpers, and news/history helper APIs.

Core must not accumulate feature policy. If a helper knows KBO domain rules, it
belongs in the owning subsystem.

### Foreign Player Policy

`native/src/foreign/` owns foreign-player reserve rights, signability and offer
blocking, Asian quota behavior, injury replacement slots, replacement-player
seed resolution, and foreign roster diagnostics.

Key subfolders:

- `rights/`: exercised reserve-right storage, loading, mutation, and query
- `signability/`: signability, offer, FA candidate, and no-minor-contract probes
- `replacement_seed/`: replacement-player seed parsing and resolution
- `injury/`: injury replacement slot lifecycle
- `quota/`: Asian quota, active count, and call-up limit policy
- `roster_audit/`: read-only roster diagnostics and snapshots

Foreign-player event-window logic decides when a retain/skip decision is legal.
Rights storage owns exercised rights after a valid retain decision exists. The
two concepts must not be collapsed.

### Military Service

`native/src/military_service/` owns military-service candidate queues, seed
assignment, active service-loan records, player service-day state, annual
selection events, return logic, and hook wrappers.

Seed storage and player movement are separate responsibilities. Queueing a
candidate does not move the player; movement happens in seed assignment or the
selection-event path.

### Custom Events

`native/src/custom_events/` owns event-name matching, event scheduling,
processed-event markers, due-event scanning, and dispatch into owning domain
handlers.

The scanner owns timing and idempotency. Domain modules own behavior. New custom
events should add a title matcher and a dispatch target rather than expanding
scanner logic inline.

### FA and Amateur Modules

The FA-adjacent modules are split by responsibility:

- `fa_rules/`: FA rule config loading and query helpers
- `fa_filing/`: FA filing persistence and lookup
- `fa_market_classification/`: market classification, cache, and CSV output
- `fa_salary_snapshot/`: opening-day salary snapshot observation
- `fa_compensation/`: compensation records, protected lists, due tasks, and news
- `fa_requalification/`: FA requalification tracking
- `amateur_player_quality/`: amateur reputation assignment and diagnostics

These modules should continue moving toward smaller files where one file owns
one lifecycle or data table.

### All-Star

`native/src/allstar/` owns single-division All-Star team setup, voting/event
hooks, schedule dates, candidate-team seeding, and related CSV parsing.

### Patch Installers and Hook Stubs

`native/src/patch_installers/` owns byte-pattern verification and patch
installation. It should not contain gameplay policy.

`native/src/hook_stubs/` owns executable call stubs and ABI wrappers. Wrappers
should pass arguments to domain modules and translate results back into OOTP ABI
shape; policy should stay in the domain modules.

### F2 Hotkey Window

`native/src/hotkey_window/` owns the in-game F2 hub UI. It may:

- display runtime state
- select league/team/player context
- write command requests
- call narrow domain query APIs
- expose recovery flags for user safety

It must not directly mutate gameplay tables or OOTP player/team memory.

Current debt: `hotkey_window.c` is still an oversized module containing state,
HTML rendering, WebView runtime, command URI routing, settings rendering, and
Win32 window code. This is the largest remaining architectural hotspot.

## Thread Lifecycle

Background loops should:

- be started by the module that owns the loop
- check `kbo_runtime_threads_should_continue()`
- sleep through `kbo_runtime_sleep_should_continue()`
- clear their own started flag before exit

`DllMain` requests runtime thread shutdown on process detach. The project does
not currently join every detached thread because most thread handles are closed
after creation. Any new thread owner should make shutdown behavior explicit and
avoid introducing loops that cannot observe the runtime stop flag.

## Known Architectural Debt

These are the current high-cost areas. They are not new precedent; they are the
next cleanup targets.

1. `native/src/hotkey_window/hotkey_window.c` is too large and mixes UI state,
   rendering, command routing, settings, and WebView lifecycle.
2. `native/src/bootstrap/forward_declarations.h` remains a central declaration
   bucket. Declarations should move into owning headers.
3. Runtime flag metadata is duplicated between managed code and native F2 UI.
4. Supported-build rows are generated, but build-specific RVA maps are still
   hand-maintained.
5. Several domain modules are mechanically migrated but still too broad:
   `foreign_waiver_core.c`, `military_service.c`, `amateur_player_quality.c`,
   `fa_market_classification.c`, `foreign/quota/foreign_quota.c`, and
   `foreign/signability/submit_offer_probe.c`.

## Migration Plan

Do not resume broad mechanical migration just to move lines around. The next
steps should close one responsibility boundary at a time:

1. Split the F2 hub by responsibility: state, rendering, WebView runtime,
   command routing, settings/flags.
2. Shrink `forward_declarations.h` by moving declarations into owning headers.
3. Create a runtime-flag manifest and generate managed/native UI flag metadata.
4. Move build-specific RVA data into generated sources.
5. Continue splitting large domain modules only when the new file owns a clear
   lifecycle, table, scanner, policy, or parser.

Each code-change commit should close one subsystem or one responsibility unit.
Docs-only commits may describe multiple known debts.

## Verification

After native or launcher changes, run:

```powershell
powershell -ExecutionPolicy Bypass -File native\build.ps1
dotnet build .\OOTP27-KBO-Launcher.sln
dotnet test .\OOTP27-KBO-Launcher.sln
powershell -ExecutionPolicy Bypass -File native\tests\run_tests.ps1
```

CI runs managed build/test, native build, and native helper tests on Windows.

## Naming Rules

- Use `reserve_rights` for exercised KBO foreign-player rights.
- Use `priority_events` for decision windows and league events.
- Use `waiver_ai` only for automatic retain/skip decisions.
- Use `foreign_policy` or `asian_quota` for roster-limit behavior.
- Avoid using `waiver` as a catch-all term for KBO foreign-player rules.

## Design Rules

- Gameplay state changes live in domain modules, not UI modules.
- UI modules can submit requests, but must not directly mutate gameplay tables.
- Event modules decide when a decision is legal.
- AI modules decide what action to take, not how rights are stored.
- Audit modules observe only.
- Patch installers patch bytes; they do not own policy.
- Hook wrappers stay thin and delegate to domain modules.
- CSV files are module-owned persistence.
- Thread start functions are explicit and owned by the loop's module.
- Public wrapper names are stable unless the corresponding patch installer is
  updated in the same change.

## Repository Data

Bundled seed/config data lives under `data/seeds/`. Runtime setup copies those
files into `%LOCALAPPDATA%\OOTP-KBO\`. Native modules read save-scoped data
first, then global local-app-data inputs as fallback. Generated/cache/output
files should be written to the save scope or the owning module's documented
path, not guessed global destinations.

## Open Questions

- Should `foreign_waiver_rights.csv` be renamed to
  `foreign_reserve_rights.csv`, or kept for compatibility?
- Should F2 user actions remain command-file based long term, or move to a
  narrower in-memory request API?
- Should numeric foreign-policy config values move into a structured manifest?
- Should WebView command routes be generated or table-driven once the F2 hub is
  split?
