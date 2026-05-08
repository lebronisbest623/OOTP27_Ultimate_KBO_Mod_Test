# OOTP27 KBO Launcher Architecture

This project has two runtime layers:

- `src/KBOLauncher/Program.cs`: the managed launcher. It locates OOTP, prepares local data files and flags, starts or attaches to the game process, and injects the native patch DLL.
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
#include "src/season_phase_monitor.inc"
#include "src/patch_helpers.inc"
#include "src/hook_stubs.inc"
#include "src/patch_installers.inc"
#include "src/hotkey_window.inc"
#include "src/entrypoint.inc"
```

This means `.inc` files can share `static` functions and state, but it also means file order is an API. Moving code must preserve declaration order and any cross-module forward declarations in `KBOFix.c` or earlier fragments.

`native/KBOFix.c` should stay as the translation-unit shell: offsets, OOTP ABI typedefs, hook prototypes, shared low-level helpers, and include order. Domain state belongs in the owning subsystem state module, not in the root file.

## Launcher Build Guard

Unknown OOTP builds must fail closed. The launcher reads the target `ootp27.exe` PE header before any DLL injection and compares `TimeDateStamp` plus `SizeOfImage` against the verified supported-build list.

If the build is unknown or unreadable:

- default no-argument runs disable KBOFix injection; if no OOTP process is running, they launch OOTP unmodified, and if one is already running, they leave it untouched
- explicit `--dll`, `--attach-existing`, and `--attach-pid` injection requests fail visibly before injection
- the launcher writes `launcher_build_guard_status.txt` under `%LOCALAPPDATA%\OOTP-KBO\`
- the native `verify_ootp_build()` check remains as a second line of defense if a DLL is loaded by another path

When OOTP patches, do not change offsets or installers speculatively. First record the detected timestamp/size pair, verify the patch behavior on that exact build, then update both the launcher supported-build list in `src/KBOLauncher/Program.cs` and the native supported-build list in `native/src/build_verify/build_verify.inc`.

## Roster Marker Guard

KBOFix injection also requires the currently opened OOTP 27 `.lg` save to be marked as an Ultimate KBO roster save. After the launcher resolves the exact target OOTP process, it reads that process' current save path from the same OOTP global database field used by the native runtime.

The current save's `description.txt` must contain:

```text
https://github.com/lebronisbest623/OOTP27_Ultimate_KBO
```

If no save is open immediately after a new OOTP launch, the launcher waits for a marked current save before injecting KBOFix. This supports the "create a new save, then begin play" path without loading the DLL into an unmarked process. If an explicit attach target is unmarked, or the launched process opens a save whose `description.txt` does not contain the marker URL, KBOFix injection is blocked visibly. The launcher writes `launcher_roster_marker_guard_status.txt` under `%LOCALAPPDATA%\OOTP-KBO\` for diagnostics.

The native save-scoped path helper follows the same fail-closed rule: it uses only the current save path reported by OOTP memory and does not scan for the newest `.lg` folder. If the current save path is unavailable, save-scoped persistence is disabled instead of writing into a guessed save bucket.

## Runtime Flag Config

Runtime boolean feature switches live in one JSON file:

```text
%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json
```

Keys use the old flag-file basename without `.txt`; for example `enable_foreign_waiver_ai.txt` becomes `"enable_foreign_waiver_ai": true`. The launcher reads and writes this JSON for managed flags, and native `read_kbo_localappdata_flag_file()` maps existing call sites to the same JSON keys. Status files, command files, seeds, CSVs, and save-scoped persistence remain separate data files.

The launcher treats the single-division All-Star option as a feature suite. Enabling it writes these native gates together: `enable_single_division_allstar_runtime_patches`, `enable_single_division_allstar_settings_patch`, `enable_single_division_allstar_voting_hook`, and `enable_single_division_allstar_events`. The events hook is nested under the runtime patch gate in native startup, so `enable_single_division_allstar_events` alone is not enough.

## Native Source Layout

`native/src/*.inc` is the public include layer for `native/KBOFix.c`. Files directly under `native/src/` should be thin feature entrypoints or assemblers only. Implementation bodies live in matching domain folders:

```text
native/src/
  core.inc                    -> core/core.inc
  build_verify.inc            -> build_verify/build_verify.inc
  runtime_memory.inc          -> runtime_memory/runtime_memory.inc
  team.inc                    -> team/team.inc
  fa_requalification.inc      -> fa_requalification/fa_requalification.inc
  military_service_loan.inc   -> military_service/
  foreign_waiver_ai.inc       -> foreign/
  custom_events.inc           -> custom_events/
  allstar.inc                 -> allstar/allstar.inc
  season_phase_monitor.inc    -> season_phase_monitor/season_phase_monitor.inc
  patch_helpers.inc           -> patch_helpers/patch_helpers.inc
  hook_stubs.inc              -> hook_stubs/hook_stubs.inc
  patch_installers.inc        -> patch_installers/patch_installers.inc
  hotkey_window.inc           -> hotkey_window/
  entrypoint.inc              -> entrypoint/entrypoint.inc
```

Rule of thumb: if a root `native/src/*.inc` grows beyond include sequencing and tiny constants, move that body into a subfolder and leave the root filename as the stable compatibility wrapper.

## Core Modules

`native/src/core.inc` is the stable root wrapper, and `native/src/core/core.inc` is the core subsystem assembler. Core should contain shared runtime helpers only; feature-specific policy belongs in the owning subsystem.

```text
native/src/core/
  core.inc
  core_log.inc
  core_text_date.inc
  core_decls.inc
  core_sql_escape.inc
  core_sql_league_news.inc
  core_sql_history_transactions.inc
  core_save_paths.inc
  core_message_body_file.inc
  core_news_object.inc
  core_current_date.inc
  core_flags.inc
  core_league_context.inc
  core_league_events.inc
  core_team_collect.inc
  core_live_news.inc
  core_history_stubs.inc
```

- `core_log.inc`: log file helpers.
- `core_text_date.inc`: ASCII comparison and YYYYMMDD history-date formatting.
- `core_decls.inc`: forward declarations needed by single-translation-unit include order.
- `core_sql_escape.inc`: SQL literal escaping.
- `core_sql_league_news.inc`: `messages` and `league_news` SQL writes.
- `core_sql_history_transactions.inc`: player-history and transaction SQL writes.
- `core_save_paths.inc`: current save detection and save-scoped data paths.
- `core_message_body_file.inc`: message body text file writes.
- `core_news_object.inc`: native news object construction helpers.
- `core_current_date.inc`: current date, year, and history-date reads.
- `core_flags.inc`: local flag readers and `kbo_fix_enabled`.
- `core_league_context.inc`: event manager and KBO league id resolution.
- `core_league_events.inc`: league-event existence/create helpers.
- `core_team_collect.inc`: league team id collection.
- `core_live_news.inc`: fanout/native live-news helpers.
- `core_history_stubs.inc`: legacy special-history stubs.

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
  foreign_waiver_date.inc
  foreign_waiver_paths.inc
  foreign_csv_parse.inc
  foreign_waiver_events.inc
  foreign_waiver_player_eval.inc
  foreign_decision_team.inc
  foreign_military_service_team_policy.inc
  foreign_waiver_window.inc
  foreign_waiver_results.inc
  foreign_waiver_rights.inc           -> assembler for rights/
  foreign_waiver_retain.inc
  foreign_waiver_decisions.inc
  foreign_waiver_announcements.inc
  foreign_waiver_io.inc
  foreign_waiver_command_execute.inc
  foreign_signability_hooks.inc       -> assembler for signability/
  foreign_waiver_status.inc
  foreign_waiver_ai.inc
  foreign_waiver_top_candidate.inc
  foreign_waiver_scanner.inc
  foreign_replacement_seed.inc        -> assembler for replacement_seed/
  foreign_injury_replacement.inc      -> assembler for injury/
  foreign_asian_quota.inc             -> assembler for quota/
  foreign_roster_audit.inc            -> assembler for roster_audit/
  rights/
    foreign_waiver_rights_active.inc
    foreign_waiver_rights_persist.inc
    foreign_waiver_rights_load.inc
    foreign_waiver_rights_mutation.inc
    foreign_waiver_rights_query.inc
    foreign_waiver_rights_memory_sync.inc
  signability/
    foreign_fa_block_state.inc
    foreign_fa_block_log.inc
    foreign_fa_block_policy.inc
    foreign_fa_fast_block_policy.inc
    foreign_signability_wrapper.inc
    foreign_signability_block_policy.inc
    foreign_offer_eligibility_wrapper.inc
    foreign_submit_offer_probe.inc
    foreign_ai_fa_candidate_wrapper.inc
  replacement_seed/
    foreign_replacement_seed_state.inc
    foreign_replacement_seed_paths.inc
    foreign_replacement_seed_lock.inc
    foreign_replacement_seed_parse.inc
    foreign_replacement_seed_memory_resolve.inc
    foreign_replacement_seed_players_dat.inc
    foreign_replacement_seed_import.inc
    foreign_replacement_seed_cache.inc
    foreign_replacement_seed_loader.inc
    foreign_replacement_seed_match.inc
  injury/
    foreign_injury_state.inc
    foreign_injury_paths.inc
    foreign_injury_labels.inc
    foreign_injury_csv_load.inc
    foreign_injury_seed_import.inc
    foreign_injury_csv_persist.inc
    foreign_injury_loader.inc
    foreign_injury_queries.inc
    foreign_injury_news.inc
    foreign_injury_exceptions.inc
    foreign_injury_scanner.inc
  quota/
    foreign_asian_quota_counts.inc
    foreign_custom_foreign_signing_policy.inc
    foreign_active_count_policy.inc
    foreign_active_count_wrappers.inc
    foreign_callup_policy.inc
    foreign_asian_quota_probe_log.inc
  roster_audit/
    foreign_roster_audit_state.inc
    foreign_roster_audit_paths.inc
    foreign_roster_audit_state_helpers.inc
    foreign_roster_audit_csv.inc
    foreign_roster_audit_scan.inc
```

### Assembly Layer

`native/src/foreign_waiver_ai.inc` includes the foreign subsystem in dependency order. It should own include sequencing and compile-time constants only. New gameplay rules should go into a focused module under `native/src/foreign/`.

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

- `kbo_flags.json` key `enable_foreign_waiver_ai`
- `kbo_flags.json` key `disable_kbo_custom_foreign_policy`
- `kbo_flags.json` key `disable_foreign_waiver_legacy_auto_detector`
- configured or inferred KBO league id

### `foreign_waiver_date.inc`

Date parsing and YYYYMMDD arithmetic used across foreign-player policy and custom event scheduling:

- parse `YYYYMMDD` and `YYYY-MM-DD`
- add days, months, and years to YYYYMMDD values
- read the current game date as YYYYMMDD

### `foreign_waiver_paths.inc`

Foreign-player file path ownership:

- `foreign_waiver_negotiation_window.txt`
- `foreign_waiver_rights.csv`
- `foreign_waiver_decisions.csv`
- `asian_quota_nation_ids.txt`
- `foreign_waiver_announcements.txt`

### `foreign_csv_parse.inc`

Shared CSV numeric-field parsing used by foreign-player persistence modules. This is intentionally tiny because several modules read comma-delimited ids and dates, but none of them should depend on command IO just to parse a number.

### `foreign_waiver_events.inc`

The event window and league-event layer:

- open the reserve-right decision window
- close the window
- persist `foreign_waiver_negotiation_window.txt`
- queue/flush custom league events
- detect the offseason anchor for the legacy detector

This module owns when decisions may be made. It should not choose which players to retain.

### `foreign_waiver_player_eval.inc`

Player identity, classification, and value helpers:

- player lookup by id
- foreign-player classification
- Asian quota nation classification
- foreign waiver value score and threshold

### `foreign_decision_team.inc`

Decision-team helpers for the reserve-right event:

- read the player's original club from memory
- resolve the team allowed to decide during an open event window
- enforce the original-club priority rule while the window is open

Important rule: this module may expose candidate ownership during the event window, but it must not create an exercised reserve right.

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

### `foreign_waiver_announcements.inc`

Announcement idempotency records:

- check whether a result announcement was already recorded
- append result announcement dates
- append result announcement bodies for diagnostics

### `foreign_waiver_rights.inc`

Assembler for the KBO foreign reserve-right store:

- `foreign_waiver_rights_active.inc`: active-window check for retained-right records
- `foreign_waiver_rights_persist.inc`: CSV writes for `foreign_waiver_rights.csv`
- `foreign_waiver_rights_load.inc`: CSV loading and duplicate collapse
- `foreign_waiver_rights_mutation.inc`: prune expired rights and upsert retained rights
- `foreign_waiver_rights_query.inc`: active-holder lookups
- `foreign_waiver_rights_memory_sync.inc`: sync persisted active rights into player/team memory

This module owns exercised rights after a retain decision exists. It should not expose event-window candidate priority as if it were an exercised right.

### `foreign_waiver_retain.inc`

Retain execution for an approved decision:

- compute retained/expires dates
- update the retained-right table through `foreign_waiver_rights.inc`
- persist the new right
- sync the right into memory

This module mutates gameplay state only after event/window and command/AI code have decided that a retain is legal.

### `foreign_waiver_decisions.inc`

Decision-record persistence:

- resolve the current reserve-right window dates
- append retain/skip records to `foreign_waiver_decisions.csv`
- detect whether a team/player/window decision already exists

### `foreign_waiver_io.inc`

Command and candidate CSV IO only:

- read and rewrite `foreign_waiver_commands.txt`
- append retain/skip commands from UI calls
- write `foreign_waiver_candidates.csv`

It should not own window rules, rights storage, or signability hooks.

### `foreign_waiver_command_execute.inc`

Command execution after `foreign_waiver_io.inc` reads a command line. This module validates retain/skip commands against the current window, decision team, and duplicate-decision rules, then calls the retain or decision-record helpers.

### `foreign_signability_hooks.inc`

Assembler for offer/signability and FA candidate hook wrappers:

- `foreign_fa_block_state.inc`: recent UI/AI block caches for F2 hub context
- `foreign_fa_block_policy.inc`: assembler for signability/FA block policy
- `foreign_fa_block_log.inc`: callsite diagnostics for new reserve-right blocks
- `foreign_military_service_team_policy.inc`: military-service team FA block helper
- `foreign_fa_fast_block_policy.inc`: early FA candidate block decisions before OOTP's original check
- `foreign_signability_block_policy.inc`: retained-right, custom foreign-policy, and injury-replacement signability decisions
- `foreign_signability_wrapper.inc`: player/team signability wrapper
- `foreign_offer_eligibility_wrapper.inc`: offer eligibility wrapper
- `foreign_submit_offer_probe.inc`: submit-offer screen probe wrapper
- `foreign_ai_fa_candidate_wrapper.inc`: AI free-agent candidate insert wrapper

### `foreign_waiver_status.inc`

F2 hub status text for the reserve-right decision window.

### `foreign_waiver_ai.inc`

AI decision logic only:

- scan eligible foreign players during an open decision window
- score candidates
- choose retain or skip
- write AI decision records

This module should call rights/orchestrator helpers to execute a retain. It should not own the rights table, event window, or signability hooks.

### `foreign_waiver_top_candidate.inc`

F2 hub helper for resolving a team's best current reserve-right candidate. This belongs outside the AI module because UI candidate preview is not the same responsibility as automatic AI decision-making.

### `foreign_waiver_scanner.inc`

Scanner thread startup and loop for foreign reserve-right processing. The loop invokes window/event processing, command handling, AI decisions, result announcements, and roster audit ticks without owning those domains.

### `foreign_replacement_seed.inc`

Assembler for known replacement-player seed and cache handling:

- `foreign_replacement_seed_state.inc`: seed record type, table state, and shared constants
- `foreign_replacement_seed_paths.inc`: save/global seed, resolved-cache, and `players.dat` path resolution
- `foreign_replacement_seed_lock.inc`: seed table lock helpers
- `foreign_replacement_seed_parse.inc`: CSV token, slot-type, and seed-line parsing
- `foreign_replacement_seed_memory_resolve.inc`: in-memory seed-key lookup helpers
- `foreign_replacement_seed_players_dat.inc`: `players.dat` fallback resolution
- `foreign_replacement_seed_import.inc`: seed table import and de-duplication
- `foreign_replacement_seed_cache.inc`: resolved id cache load/persist
- `foreign_replacement_seed_loader.inc`: lazy-load and unresolved-key retry orchestration
- `foreign_replacement_seed_match.inc`: public match check used by roster counts

This module is data-resolution infrastructure. It should not enforce roster rules.

### `foreign_injury_replacement.inc`

Assembler for temporary foreign-player injury replacement rules:

- `foreign_injury_state.inc`: constants, record type, table state, feature flag, and forward declarations
- `foreign_injury_paths.inc`: save/global CSV path resolution
- `foreign_injury_labels.inc`: display labels, slot classification, and lock helpers
- `foreign_injury_csv_load.inc`: persisted replacement record loading
- `foreign_injury_seed_import.inc`: save/global seed import for open replacement cases
- `foreign_injury_csv_persist.inc`: persisted replacement record writes
- `foreign_injury_loader.inc`: lazy-load orchestration and seed import trigger
- `foreign_injury_queries.inc`: slot lookup/count helpers used by roster policy and UI
- `foreign_injury_news.inc`: native news for open/pending transitions
- `foreign_injury_exceptions.inc`: signing/callup exception policy
- `foreign_injury_scanner.inc`: scanner thread and injury lifecycle transitions

It may depend on `foreign_replacement_seed.inc` and `foreign_asian_quota.inc` for classification, but it should own only injury-replacement lifecycle and exceptions.

### `foreign_asian_quota.inc`

Assembler for Asian quota and foreign roster-count policy:

- `foreign_asian_quota_counts.inc`: organization and active-roster foreign/asian/non-asian counts
- `foreign_custom_foreign_signing_policy.inc`: signing/offer allow checks for the custom foreign-player limit
- `foreign_active_count_policy.inc`: active foreign count adjustment and neutralization policy
- `foreign_active_count_wrappers.inc`: OOTP active foreign hitter/pitcher count wrappers
- `foreign_callup_policy.inc`: callup limit policy and OOTP callup wrappers
- `foreign_asian_quota_probe_log.inc`: signability/offer diagnostic probes

It may ask `foreign_injury_replacement.inc` whether an extra temporary slot exists.

### `foreign_roster_audit.inc`

Assembler for foreign roster diagnostics only:

- `foreign_roster_audit_state.inc`: audit record type and in-memory snapshot table
- `foreign_roster_audit_paths.inc`: audit/snapshot CSV path resolution
- `foreign_roster_audit_state_helpers.inc`: state capture, comparison, and change-type classification
- `foreign_roster_audit_csv.inc`: CSV headers, file open helpers, and row writers
- `foreign_roster_audit_scan.inc`: scan loop, baseline reset, change detection, and summary logging

This module should never change gameplay state.

## Include Order Goal

The current foreign include section is:

```c
#include "foreign/foreign_waiver_decls.inc"
#include "foreign/foreign_waiver_config.inc"
#include "foreign/foreign_waiver_state.inc"
#include "foreign/foreign_waiver_policy.inc"
#include "foreign/foreign_waiver_date.inc"
#include "foreign/foreign_waiver_paths.inc"
#include "foreign/foreign_csv_parse.inc"
#include "foreign/foreign_waiver_events.inc"
#include "foreign/foreign_waiver_player_eval.inc"
#include "foreign/foreign_decision_team.inc"
#include "foreign/foreign_replacement_seed.inc"
#include "foreign/foreign_injury_replacement.inc"
#include "foreign/foreign_asian_quota.inc"
#include "foreign/foreign_waiver_rights.inc"
#include "foreign/foreign_waiver_retain.inc"
#include "foreign/foreign_waiver_decisions.inc"
#include "foreign/foreign_waiver_announcements.inc"
#include "foreign/foreign_waiver_results.inc"
#include "foreign/foreign_waiver_window.inc"
#include "foreign/foreign_signability_hooks.inc"
#include "foreign/foreign_waiver_status.inc"
#include "foreign/foreign_waiver_command_execute.inc"
#include "foreign/foreign_waiver_ai.inc"
#include "foreign/foreign_waiver_io.inc"
#include "foreign/foreign_waiver_top_candidate.inc"
#include "foreign/foreign_roster_audit.inc"
#include "foreign/foreign_injury_scanner.inc"
#include "foreign/foreign_waiver_scanner.inc"
```

The exact order can change if compilation proves a different dependency is cleaner. If two modules need each other, prefer a narrow forward declaration in `foreign_waiver_decls.inc` or a tiny shared helper in `foreign_waiver_config.inc` / `foreign_waiver_player_eval.inc` over merging the modules again.

## All-Star Modules

`native/src/allstar.inc` is a stable root wrapper, and `native/src/allstar/allstar.inc` is the All-Star subsystem assembler.

```text
native/src/allstar/
  allstar.inc
  allstar_state.inc
  allstar_league_context.inc
  allstar_string.inc
  allstar_csv_parse.inc
  allstar_csv_store.inc
  allstar_team_patch.inc
  allstar_candidate_seed.inc
  allstar_flags.inc
  allstar_event_wrappers.inc
```

- `allstar_state.inc`: shared typedefs, tables, counters, and build-specific layout selection
- `allstar_league_context.inc`: league pointer lookup and KBO All-Star context checks
- `allstar_string.inc`: OOTP string assignment helper resolution
- `allstar_csv_parse.inc`: All-Star team-split CSV parsing helpers
- `allstar_csv_store.inc`: CSV path resolution, row storage, and lazy loading
- `allstar_team_patch.inc`: Nanum/Dream and Futures North/South team name assignment
- `allstar_candidate_seed.inc`: candidate-team seeding wrapper
- `allstar_flags.inc`: league flag enforcement and startup retry
- `allstar_event_wrappers.inc`: All-Star event/voting preparation wrappers

All-Star CSV parsing should stay separate from team-memory mutation. Wrappers should remain thin and call the lower-level helpers.

## Hook Stub Modules

`native/src/hook_stubs.inc` is the stable root wrapper, and `native/src/hook_stubs/hook_stubs.inc` is the executable stub-builder assembler. Stub builders only assemble trampoline/detour byte buffers and flush instruction cache. Policy decisions belong in wrapper/domain modules; patch-site discovery and installation belong in `patch_installers/`.

```text
native/src/hook_stubs/
  hook_stubs.inc
  hook_stubs_military.inc
  hook_stubs_foreign_signability.inc
  hook_stubs_foreign_ai_status.inc
  hook_stubs_foreign_counts.inc
  hook_stubs_near_code.inc
  hook_stubs_season_phase.inc
  hook_stubs_allstar_settings.inc
  hook_stubs_allstar_events.inc
  hook_stubs_allstar_candidate.inc
```

- `hook_stubs_military.inc`: military service entry trampoline and status detour stubs.
- `hook_stubs_foreign_signability.inc`: foreign signability, offer eligibility, submit-offer probe, and FA signing branch stubs.
- `hook_stubs_foreign_ai_status.inc`: AI FA status candidate insertion stub.
- `hook_stubs_foreign_counts.inc`: active foreign count and callup-limit branch stubs.
- `hook_stubs_near_code.inc`: near-target executable allocation helper for relative call sites.
- `hook_stubs_season_phase.inc`: season-phase write probe call stub.
- `hook_stubs_allstar_settings.inc`: All-Star setting enable stub.
- `hook_stubs_allstar_events.inc`: All-Star event preparation and voting-begin stubs.
- `hook_stubs_allstar_candidate.inc`: All-Star candidate team split stub.

## Patch Installer Modules

`native/src/patch_installers.inc` is the stable root wrapper, and `native/src/patch_installers/patch_installers.inc` is the patch installer assembler. Installer modules own byte-pattern verification and patch application only; gameplay policy belongs in the hook wrapper or domain subsystem being called.

```text
native/src/patch_installers/
  patch_installers.inc
  patch_installers_military.inc
  patch_installers_foreign_signability_entry.inc
  patch_installers_global_callback_probe.inc
  patch_installers_foreign_ai_fa_fallback.inc
  patch_installers_foreign_submit_offer_probe.inc
  patch_installers_foreign_signing_branch.inc
  patch_installers_foreign_ai_fa_status.inc
  patch_installers_foreign_counts.inc
  patch_installers_foreign_callup_limits.inc
  patch_installers_allstar_common.inc
  patch_installers_allstar_static.inc
  patch_installers_allstar_candidate.inc
  patch_installers_allstar_events.inc
  patch_installers_allstar_settings.inc
```

- `patch_installers_military.inc`: military service entry and status-update detours.
- `patch_installers_foreign_signability_entry.inc`: player/team signability and offer-eligibility detours.
- `patch_installers_foreign_ai_fa_fallback.inc`: AI FA signability fallback static patch.
- `patch_installers_foreign_submit_offer_probe.inc`: FA submit-offer probe detour.
- `patch_installers_foreign_signing_branch.inc`: FA signing branch signature scan and detour.
- `patch_installers_foreign_ai_fa_status.inc`: AI FA status candidate insertion detour.
- `patch_installers_foreign_counts.inc`: active/secondary foreign-player count detours.
- `patch_installers_foreign_callup_limits.inc`: foreign-player callup limit branch detours.
- `patch_installers_allstar_common.inc`: All-Star installer host and static-pattern helpers.
- `patch_installers_allstar_static.inc`: single-division All-Star static byte patches.
- `patch_installers_allstar_candidate.inc`: All-Star candidate team split hook.
- `patch_installers_allstar_events.inc`: All-Star voting and event preparation hooks.
- `patch_installers_allstar_settings.inc`: All-Star settings UI byte patches.

## Team Modules

`native/src/team.inc` is the stable root wrapper, and `native/src/team/team.inc` is the team subsystem assembler. Team lookup/name helpers are read-only. Roster-array and assignment helpers mutate OOTP memory and should only be used by domain modules that own the state transition being performed.

```text
native/src/team/
  team.inc
  team_string.inc
  team_name_cache.inc
  team_lookup.inc
  team_org_assignment_query.inc
  team_roster_arrays.inc
  team_assignment.inc
```

- `team_string.inc`: bounded ASCII copies, OOTP string-object reads, and team string matching.
- `team_name_cache.inc`: save-scoped `names.dat` cache loading, player name lookup, and display-name formatting.
- `team_lookup.inc`: player pointer plausibility, global player-vector discovery, and team lookup by CSV or numeric id.
- `team_org_assignment_query.inc`: organization/affiliate id matching and current player assignment checks.
- `team_roster_arrays.inc`: fixed-size team roster-array add/remove helpers.
- `team_assignment.inc`: OOTP-like player-to-team assignment mutation and native team add/remove entry resolution.

## Hotkey Window Modules

`native/src/hotkey_window.inc` is the stable F2 hub entrypoint. It includes state, support helpers, text-content fallback, and the UI assembler.

`native/src/hotkey_window/state.inc` is a thin assembler for F2 hub types, global handles, selection state, and small state-adjacent lookup helpers:

```text
native/src/hotkey_window/
  state.inc
  state_types.inc
  state_window.inc
  state_gdi.inc
  state_view.inc
  state_skin.inc
  state_constants.inc
  state_text.inc
  state_team_vector.inc
  state_language.inc
  state_nav.inc
  state_text_utils.inc
  state_league_lookup.inc
  state_league_name.inc
  state_decls.inc
```

- `state_types.inc`: shared F2 hub buffer/search structs.
- `state_window.inc`: Win32/WebView window handles, startup flags, hook handles, and thread state.
- `state_gdi.inc`: GDI font and brush handles.
- `state_view.inc`: selected view, language, dropdown, selected league/team/player, and legacy hit rectangles.
- `state_skin.inc`: skin metrics, bitmap assets, and logo cache keys.
- `state_constants.inc`: hub control ids, view ids, language ids, fixed size, and palette constants.
- `state_text.inc`: current-language text selection helper.
- `state_team_vector.inc`: team vector lookup used by hub selection/league filtering.
- `state_language.inc`: language setting path, load, and save.
- `state_nav.inc`: localized navigation labels.
- `state_text_utils.inc`: UTF-8/wide conversion and ASCII trimming helpers.
- `state_league_lookup.inc`: league pointer cache, miss cache, and global DB league-vector scan.
- `state_league_name.inc`: league-name string scan and display-name formatting.
- `state_decls.inc`: forward declarations for later UI/domain helpers needed by the single translation unit.

`native/src/hotkey_window/content.inc` is a thin assembler for the legacy text-content fallback used by the Win32 edit control:

```text
native/src/hotkey_window/
  content.inc
  content_decls.inc
  content_service_helpers.inc
  content_overview.inc
  content_military.inc
  content_foreign_rights.inc
  content_foreign_injury.inc
  content_mod_info.inc
  content_foreign_policy.inc
  content_asian_games.inc
  content_settings.inc
  content_router.inc
  content_refresh.inc
```

- `content_decls.inc`: local forward declarations for foreign-rights status/candidate helpers.
- `content_service_helpers.inc`: service-team player counts used by overview and military panels.
- `content_overview.inc`: overview text panel.
- `content_military.inc`: military service text panel.
- `content_foreign_rights.inc`: foreign-rights text panel and selected-candidate text.
- `content_foreign_injury.inc`: foreign injury replacement text panel.
- `content_mod_info.inc`: mod information text panel.
- `content_foreign_policy.inc`: Asian-quota/foreign-policy text panel.
- `content_asian_games.inc`: Asian Games roster text panel.
- `content_settings.inc`: settings/hotkey text panel.
- `content_router.inc`: selected-view dispatch for text panels.
- `content_refresh.inc`: Win32 edit-control refresh and UTF-8 to wide text handoff.

`native/src/hotkey_window/ui.inc` is a thin assembler for the WebView2/Win32 UI surface:

```text
native/src/hotkey_window/
  ui.inc
  ui_view_selection.inc
  ui_foreign_controls.inc
  ui_html_helpers.inc
  ui_html_render.inc
  ui_webview_runtime.inc
  ui_window.inc
```

- `ui_view_selection.inc`: current view titles, league/team visibility rules, selection validation, and league/team dropdown commands.
- `ui_foreign_controls.inc`: legacy foreign-rights list/edit interaction and retain/skip command submission.
- `ui_html_helpers.inc`: HTML escaping, table cells, date/flag formatting, file URL/image helpers, and WebView dropdown markup helpers.
- `ui_html_render.inc`: full F2 hub HTML document rendering and current WebView navigation.
- `ui_webview_runtime.inc`: WebView2 COM handlers, command URI dispatch, controller creation, settings, and bounds.
- `ui_window.inc`: fixed-size Win32 window layout, window procedure, keyboard hook thread, and startup.

UI modules can submit command-file actions and refresh views, but direct gameplay memory mutation remains outside `hotkey_window/`.

## Remaining Migration Plan

1. Split `fa_requalification/fa_requalification.inc` into state/policy/wrapper files.
2. Revisit `custom_events/asian_games_selection.inc` and `asian_games_news.inc`; those are the next oversized event-domain files.
3. Keep actual AI scoring and automatic retain/skip decisions in `foreign_waiver_ai.inc`.
4. Keep audit/snapshot code observation-only.
5. Continue splitting `hotkey_window/support.inc`; `state.inc`, `content.inc`, and `ui.inc` are already assemblers.
6. Continue trimming team assignment helpers only when a new domain owner makes the mutation boundary clearer.
7. Decide whether `season_phase_monitor.inc` should remain as its own domain module or be folded into a clearer lifecycle owner.

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

## Repository Root Hygiene

The repository root is for project entry points only: the solution file, launcher project, launcher source, and startup scripts. Domain data should not live in the root.

Bundled seed/config data files live under:

```text
data/seeds/
  asian_games_schedule_seed.csv
  allstar_teams.csv
  fa_rules.json
  foreign_replacement_players_seed.csv
  military_service_seed.csv
```

At runtime, `src/KBOLauncher/Program.cs` copies bundled seed files into `%LOCALAPPDATA%\OOTP-KBO\`. Native modules then read save-scoped seed files first and global `%LOCALAPPDATA%\OOTP-KBO\` seed files as fallback. New seed files should follow the same rule: source bundle under `data/seeds/`, runtime copy under local app data, resolved/cache/output files under save-scoped or module-owned paths.

## Open Questions

- Should `foreign_waiver_rights.csv` be renamed to `foreign_reserve_rights.csv`, or kept for compatibility?
- Should the F2 hub show retained rights outside the event window separately from event candidates?
- Should user decisions be command-file driven long term, or should the UI call a narrower in-memory API?
- Should numeric foreign-policy config values move into a structured config file, or remain as simple text overrides?

## Custom Events

`native/src/custom_events.inc` is also a thin assembler. The actual event responsibilities live under `native/src/custom_events/`.

```text
native/src/custom_events/
  asian_games_state.inc
  custom_event_state.inc
  custom_event_names.inc
  custom_event_lookup.inc
  asian_games_schedule.inc
  asian_games_player_eval.inc
  asian_games_roster_store.inc
  asian_games_selection.inc
  asian_games_lifecycle.inc
  asian_games_lifecycle_roster.inc
  asian_games_lifecycle_replacement.inc
  asian_games_lifecycle_departure.inc
  asian_games_lifecycle_final.inc
  asian_games_news.inc
  custom_event_markers.inc
  foreign_priority_event_schedule.inc
  custom_event_dispatch.inc
  custom_event_scan.inc
  nation_id_scan.inc
  offseason_transition_schedule.inc
  custom_event_monitor.inc
```

### `asian_games_state.inc`

Asian Games roster constants, DTOs, and in-memory roster state.

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

Assembler for roster lifecycle after selection:

- `asian_games_lifecycle_roster.inc`: selected-roster membership, wildcard counts, replacement organization limits, and departure availability checks
- `asian_games_lifecycle_replacement.inc`: unavailable-player replacement search and roster-slot replacement before departure
- `asian_games_lifecycle_departure.inc`: restricted-list mutation, roster transaction/history records, and departure persistence
- `asian_games_lifecycle_final.inc`: final return, military exemption completion, assignment restoration, and finalized-roster checks

The lifecycle assembler may forward-declare news emission, but replacement/departure/final files should not build article bodies themselves.

### `asian_games_news.inc`

News/article text generation and Asian Games event handlers. This module may read roster state but should not choose the roster.

### `custom_event_markers.inc`

Idempotency layer:

- persisted `custom_events_processed.txt`
- in-memory processed event pointers
- mark game events as over

### `foreign_priority_event_schedule.inc`

Schedules foreign-player priority negotiation close events and the military-service selection event from the offseason anchor.

### `custom_event_dispatch.inc`

Routes a due custom event to the owning domain handler:

- foreign-player reserve-right open/close events
- annual military-service selection
- Asian Games selection, departure, and final events

The dispatcher may call domain modules, but it should not scan the event manager or persist processed markers.

### `custom_event_scan.inc`

Due-event scanner. It owns event-manager traversal, due checks, duplicate marker checks, processed-marker writes, and scan logging. It delegates domain behavior to `custom_event_dispatch.inc`.

### `nation_id_scan.inc`

Diagnostics only. It should not affect gameplay state.

### `offseason_transition_schedule.inc`

Read-only league-phase transition watcher used by the custom-event monitor. When the KBO league moves from postseason/regular phase into offseason phase after October 1, it treats the current date as the offseason anchor and schedules the same foreign-priority and military-selection events that would otherwise wait for OOTP's `Offseason starts` league event to become visible.

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

Military-service constants, DTOs, and shared state:

- active service-loan registry
- seed registry
- draft-candidate queue
- return-history idempotency keys
- service hook/tick log counters

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
