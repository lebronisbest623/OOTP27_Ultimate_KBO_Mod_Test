# OOTP27 KBO Launcher Architecture

This project has two runtime layers:

- `Program.cs`: the managed launcher. It locates OOTP, prepares local data files and flags, starts or attaches to the game process, and injects the native patch DLL.
- `native/KBOFix.c`: the native runtime patch. It is built as one translation unit and includes feature fragments from `native/src/*.inc`.

The native layer is where most KBO rule emulation lives. Because it patches a closed game process, the architecture favors small, explicit feature modules over generic abstractions.

## Current Native Shape

`native/KBOFix.c` is a single-translation-unit build. Feature fragments are included in dependency order:

```c
#include "src/core.inc"
#include "src/build_verify.inc"
#include "src/runtime_memory.inc"
#include "src/team.inc"
#include "src/fa_requalification.inc"
#include "src/military_service_loan.inc"
#include "src/foreign_waiver_ai.inc"
#include "src/custom_events.inc"
#include "src/allstar.inc"
#include "src/patch_helpers.inc"
#include "src/hook_stubs.inc"
#include "src/patch_installers.inc"
#include "src/hotkey_window.inc"
#include "src/entrypoint.inc"
```

This means `.inc` files can share `static` functions and state, but it also means file order is an API. Moving code must preserve declaration order and any cross-module forward declarations in `KBOFix.c` or earlier fragments.

## Foreign Player Rules

`native/src/foreign_waiver_ai.inc` used to own too many responsibilities:

- foreign reserve-right negotiation window
- reserve-right storage, loading, pruning, and memory sync
- user retain/skip commands
- AI retain/skip decisions
- signability and offer blocking hooks
- Asian quota policy
- injury replacement slots
- replacement-player seed resolution
- foreign roster audit and snapshots
- helper APIs consumed by the F2 hub UI

The file name said "waiver AI", but the file represented the whole foreign-player policy subsystem. The current direction is to keep `native/src/foreign_waiver_ai.inc` as a thin assembler/orchestrator and move domain responsibilities under `native/src/foreign/`.

## Foreign Policy Modules

The foreign-player rules are split into a subsystem under `native/src/foreign/`.

```text
native/src/foreign/
  foreign_waiver_decls.inc
  foreign_waiver_config.inc
  foreign_waiver_state.inc
  foreign_waiver_policy.inc
  foreign_waiver_events.inc
  foreign_waiver_player_eval.inc
  foreign_waiver_window.inc
  foreign_waiver_results.inc
  foreign_waiver_rights.inc
  foreign_waiver_io.inc
  foreign_signability_hooks.inc
  foreign_waiver_status.inc
  foreign_waiver_ai.inc
  foreign_replacement_seed.inc
  foreign_injury_replacement.inc
  foreign_asian_quota.inc
  foreign_roster_audit.inc
```

### Assembly Layer

`native/src/foreign_waiver_ai.inc` includes the foreign subsystem in dependency order and still owns the remaining glue:

- original-club decision-team resolution
- retain/skip decision record writes
- command execution after IO reads a command line
- scanner thread startup and loop
- bridge calls into AI, audit, injury replacement, and UI helpers

This file should keep shrinking. New gameplay rules should go into a focused module under `native/src/foreign/`.

### `foreign_waiver_decls.inc`

Forward declarations for cross-module static functions. This exists because the native DLL is built as one translation unit and include order is effectively an API.

### `foreign_waiver_config.inc`

Local config and manual override readers:

- read leading numeric values from local flag/config files
- read forced foreign-player candidate ids
- resolve the optional AI target team id

Important rule: a player does not start with an exercised reserve right just because OOTP memory has an `active_team_id`. The reserve right is created only when the retain decision is made during the reserve-right event window or loaded from persisted `foreign_waiver_rights.csv`.

### `foreign_waiver_state.inc`

Shared foreign-waiver state and DTOs:

- negotiation-window cache fields
- pending event queue state
- retained-right record table
- AI target candidate struct
- shared locks

### `foreign_waiver_policy.inc`

Policy switches and league resolution:

- `enable_foreign_waiver_ai.txt`
- `disable_kbo_custom_foreign_policy.txt`
- configured or inferred KBO league id
- negotiation-window path ownership

### `foreign_waiver_events.inc`

The event window and news layer:

- open the reserve-right decision window
- close the window
- persist `foreign_waiver_negotiation_window.txt`
- queue/flush custom league events
- current-date/date math helpers used by the foreign subsystem
- announcement record paths and write helpers

This module owns when decisions may be made. It should not choose which players to retain.

### `foreign_waiver_player_eval.inc`

Player identity, classification, and value helpers:

- player lookup by id
- foreign-player classification
- Asian quota nation classification
- foreign waiver value score and threshold

### `foreign_waiver_window.inc`

Negotiation-window reads and open-state checks:

- read `foreign_waiver_negotiation_window.txt`
- cache open-window state
- decide whether retain/skip decisions are currently legal

### `foreign_waiver_results.inc`

Reserve-right result summary generation:

- summarize AI/user retain and skip decisions
- count active retained rights
- record result announcements

### `foreign_waiver_rights.inc`

The KBO foreign reserve-right domain:

- retained-right records
- load/persist/prune rights
- apply a retained right to player/team memory
- find active right holder
- answer whether a team already holds an active right

### `foreign_waiver_io.inc`

Command and candidate CSV IO only:

- read and rewrite `foreign_waiver_commands.txt`
- append retain/skip commands from UI calls
- write `foreign_waiver_candidates.csv`

It should not own window rules, rights storage, or signability hooks.

### `foreign_signability_hooks.inc`

Offer/signability and FA candidate hook wrappers:

- block non-holder signings for players with active retained rights
- record recent UI/AI offer blocks for F2 hub context
- apply custom foreign policy and injury-replacement exceptions

### `foreign_waiver_status.inc`

F2 hub status text for the reserve-right decision window.

### `foreign_waiver_ai.inc`

AI decision logic only:

- scan eligible foreign players during an open decision window
- score candidates
- choose retain or skip
- write AI decision records

This module should call rights/orchestrator helpers to execute a retain. It should not own the rights table, event window, or signability hooks.

### `foreign_replacement_seed.inc`

Seed and cache handling for known replacement players:

- read global/save `foreign_replacement_players_seed.csv`
- resolve player keys from memory or `players.dat`
- persist resolved ids
- answer whether a player matches a replacement seed

This module is data-resolution infrastructure. It should not enforce roster rules.

### `foreign_injury_replacement.inc`

Temporary foreign-player injury replacement rules:

- replacement slot records
- seed import for open replacement cases
- slot status transitions
- injury replacement news
- signing/callup exceptions for replacement candidates
- replacement scanner thread

It may depend on `foreign_replacement_seed.inc` and `foreign_asian_quota.inc` for classification, but it should own only injury-replacement lifecycle.

### `foreign_asian_quota.inc`

Asian quota and foreign roster-count policy:

- configurable Asian quota nation ids
- effective foreign count
- team foreign/asian/non-asian counts
- active roster count neutralization
- callup limit wrappers
- custom foreign policy allow/deny checks

It may ask `foreign_injury_replacement.inc` whether an extra temporary slot exists.

### `foreign_roster_audit.inc`

Diagnostics only:

- foreign roster state snapshots
- change log CSV
- baseline reset per save

This module should never change gameplay state.

## Include Order Goal

The current foreign include section is:

```c
#include "foreign/foreign_waiver_decls.inc"
#include "foreign/foreign_waiver_config.inc"
#include "foreign/foreign_waiver_state.inc"
#include "foreign/foreign_waiver_policy.inc"
#include "foreign/foreign_waiver_events.inc"
#include "foreign/foreign_waiver_player_eval.inc"
#include "foreign/foreign_replacement_seed.inc"
#include "foreign/foreign_injury_replacement.inc"
#include "foreign/foreign_asian_quota.inc"
#include "foreign/foreign_waiver_rights.inc"
#include "foreign/foreign_waiver_results.inc"
#include "foreign/foreign_waiver_window.inc"
#include "foreign/foreign_signability_hooks.inc"
#include "foreign/foreign_waiver_status.inc"
```

The exact order can change if compilation proves a different dependency is cleaner. If two modules need each other, prefer a narrow forward declaration in `foreign_waiver_decls.inc` or a tiny shared helper in `foreign_waiver_config.inc` / `foreign_waiver_player_eval.inc` over merging the modules again.

## Remaining Migration Plan

1. Move the remaining decision-record CSV helpers out of the root assembler.
2. Move `kbo_get_foreign_waiver_decision_team_id` and original-club priority checks into a small decision-domain module.
3. Move scanner thread loops to the module that owns each loop.
4. Keep actual AI scoring and automatic retain/skip decisions in `foreign_waiver_ai.inc`.
5. Keep audit/snapshot code observation-only.

After each step, run:

```powershell
powershell -ExecutionPolicy Bypass -File native\build.ps1
dotnet build .\OOTP27-KBO-Launcher.sln
```

## Naming Rules

- Use `reserve_rights` for exercised KBO foreign-player rights.
- Use `priority_events` for the decision window and league events.
- Use `waiver_ai` only for automatic retain/skip decisions.
- Use `foreign_policy` or `asian_quota` for roster limit behavior.
- Avoid using `waiver` as a catch-all term for KBO foreign-player rules.

## Design Rules

- Gameplay state changes must live in domain modules, not UI modules.
- UI modules can select players and submit commands, but must not directly mutate rights tables.
- Event modules decide when a decision is legal.
- AI modules decide what action to take, not how rights are stored.
- Audit modules observe only.
- Thread start functions should be explicit and owned by the module whose loop they run.
- CSV files are module-owned persistence; filename ownership should be documented in that module.
- Existing exported wrapper names should remain stable unless patch installer code is updated in the same change.

## Open Questions

- Should `foreign_waiver_rights.csv` be renamed to `foreign_reserve_rights.csv`, or kept for compatibility?
- Should the F2 hub show retained rights outside the event window separately from event candidates?
- Should user decisions be command-file driven long term, or should the UI call a narrower in-memory API?
- Should all foreign-policy flags move behind one config loader instead of repeated flag-file checks?

## Custom Events

`native/src/custom_events.inc` is also a thin assembler. The actual event responsibilities live under `native/src/custom_events/`.

```text
native/src/custom_events/
  custom_event_state.inc
  custom_event_names.inc
  custom_event_lookup.inc
  asian_games_schedule.inc
  asian_games_player_eval.inc
  asian_games_roster_store.inc
  asian_games_selection.inc
  asian_games_lifecycle.inc
  asian_games_news.inc
  custom_event_markers.inc
  foreign_priority_event_schedule.inc
  custom_event_scan.inc
  nation_id_scan.inc
  custom_event_monitor.inc
```

### `custom_event_state.inc`

Shared monitor state, event titles, processed-event cache, and narrow cross-module declarations.

### `custom_event_names.inc`

Name matching for local custom events. This is the routing vocabulary used by the scanner.

### `custom_event_lookup.inc`

Read-only league-event-manager scans:

- find latest offseason anchor
- detect schedule fallback anchors from league year
- check whether a custom event already exists

### `asian_games_schedule.inc`

Creates Asian Games custom events for selection, departure, and final dates.

### `asian_games_player_eval.inc`

Player scoring, role buckets, organization/team helpers, and Asian Games date helpers.

### `asian_games_roster_store.inc`

Asian Games roster persistence only:

- scoped `asian_games_roster.csv` path
- load/save
- clear memory when save changes

### `asian_games_selection.inc`

Roster construction:

- candidate selection
- wildcard replacement
- required organization coverage

### `asian_games_lifecycle.inc`

Roster lifecycle after selection:

- unavailable-player replacement
- departure restrictions
- final return/exemption state

### `asian_games_news.inc`

News/article text generation and Asian Games event handlers. This module may read roster state but should not choose the roster.

### `custom_event_markers.inc`

Idempotency layer:

- persisted `custom_events_processed.txt`
- in-memory processed event pointers
- mark game events as over

### `foreign_priority_event_schedule.inc`

Schedules foreign-player priority negotiation close events and the military-service selection event from the offseason anchor.

### `custom_event_scan.inc`

Dispatch loop for due custom events. It should route to domain handlers, not implement those handlers inline.

### `nation_id_scan.inc`

Diagnostics only. It should not affect gameplay state.

### `custom_event_monitor.inc`

Thread startup and polling loop for scheduling/scanning custom events.

## Custom Event Rules

- The scanner owns dispatch timing, not domain behavior.
- Asian Games modules own only national-team lifecycle, not generic custom event mechanics.
- Marker persistence is the idempotency boundary; event handlers should remain safe to call more than once.
- New custom events should add a title matcher and a domain handler instead of expanding `scan_kbo_custom_events_once` with large inline logic.

## Military Service Loans

`native/src/military_service_loan.inc` is a thin assembler for the military-service loan subsystem. The implementation lives under `native/src/military_service/`.

```text
native/src/military_service/
  military_service_decls.inc
  military_service_date.inc
  military_service_state.inc
  military_service_queue.inc
  military_service_parse.inc
  military_service_seed_store.inc
  military_active_loans.inc
  military_player_state.inc
  military_selection_news.inc
  military_seed_assignments.inc
  military_native_loan.inc
  military_return.inc
  military_days_tick.inc
  military_selection_event.inc
  military_hooks.inc
```

### `military_service_decls.inc`

Forward declarations and the original OOTP military-service hook typedef.

### `military_service_date.inc`

Date serial helpers used by service duration, return dates, and event routing.

### `military_service_state.inc`

Shared seed, draft-candidate, and return-history state.

### `military_service_queue.inc`

Draft candidate queueing and return-history idempotency. This module should not move players.

### `military_service_parse.inc`

CSV token parsing, numeric parsing, and military date conversion helpers.

### `military_service_seed_store.inc`

Assembler for the military-service seed subsystem. The implementation is split again under `native/src/military_service/seed/`.

```text
native/src/military_service/seed/
  military_seed_paths.inc
  military_seed_line_parse.inc
  military_seed_registry.inc
  military_seed_resolved_cache.inc
  military_seed_player_lookup.inc
  military_seed_players_dat_record.inc
  military_seed_memory_resolve.inc
  military_seed_players_dat_resolve.inc
  military_seed_resolve.inc
```

Seed file ownership:

- save/global seed paths
- resolved cache path
- seed line parsing
- seed registry load and mutation
- player lookup and memory probes
- player id resolution from memory or `players.dat`
- unresolved seed resolution orchestration

### `military_active_loans.inc`

In-memory active military-service loan registry. This is the source of original-team/service-team linkage while a player is serving.

### `military_player_state.inc`

Player flags and day counters:

- stored days left
- effective days left
- effective return date
- unavailable/status flag cleanup

### `military_selection_news.inc`

News text and OOTP link helpers for the annual military-service selection event.

### `military_seed_assignments.inc`

Applies configured seed assignments to players and registers active service loans.

### `military_native_loan.inc`

Tiny helpers around OOTP native loan flags. This module is intentionally small.

### `military_return.inc`

Returns completed service players to original clubs and writes return history.

### `military_days_tick.inc`

Daily service-time ticking and the optional background tick thread.

### `military_selection_event.inc`

Annual custom-event handling for queued military-service draft candidates.

### `military_hooks.inc`

Hook wrappers around OOTP military entry/status update behavior.

## Military Service Rules

- Seed storage and seed application are separate responsibilities.
- Queueing a candidate does not move the player; movement happens in the selection event or seed assignment module.
- Active-loan registry owns the temporary relationship between original club and service club.
- Return logic owns player restoration and history writing.
- Hook wrappers should remain thin and delegate policy to the service modules.
