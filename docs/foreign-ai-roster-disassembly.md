# Foreign AI roster placement disassembly notes

Date: 2026-05-11

Scope: OOTP27 native roster-placement path around the hooked AI team/player fit
function and player/team status lookup. This note records static disassembly
findings and how they line up with the latest runtime logs. No new experiment
was run for this note.

## Current hook surface

Source: `native/src/team/add_player_guard/team_add_player_guard.c`

- `ootp_kbo_ai_team_player_fit_trace_wrapper` hooks the function at
  `0x140a35560`. The wrapper treats return values as adjustable AI fit values.
- `kbo_apply_ai_team_player_fit_bias` now suppresses result `1` to `0` at
  return site `0xa39d24` for eligible KBO minor-league foreign-player cases, so
  a player already inserted into the candidate id array is not immediately
  removed by the native compaction branch.
- `ootp_kbo_player_team_status_lookup_trace_wrapper` can write status bytes for
  allowlisted test players `5320` and `5381`, including `status[0x24] = 5` and
  `status[0x25] = 0` at final-candidate lookup sites.

## 0x140a35560: not a normal score function

Observed disassembly:

- `0x140a3557b-0x140a35587`: if arg3 is null, the function uses
  `team + 0x1864`.
- `0x140a35592-0x140a355b1`: compares `player + 0xb4` against the first six
  `uint32_t` entries in arg3.
- `0x140a357eb`: returns `1` when the player id is already in that six-slot
  id array.
- If the player id is not in that array and arg5 permits deeper status checks,
  `status[0x24]` is mapped to return values:
  - status `7` or `8` -> `6`
  - status `6` or `3` -> `5`
  - status `5` -> `4`
  - status `4` -> `3`
  - status `2` -> `2`
  - otherwise -> `0`

Conclusion: return `1` is not "good roster fit". It means "already in the
candidate id array". At `0xa39d24`, this branch is a removal/penalty check, so
the useful bias is `1 -> 0` for quota-allowed minor foreign players, not
`0 -> 1`.

## Candidate id array construction

The six-slot id array is `team + 0x1864` in the direct call path, and appears as
`0x1a0(%rbp)` inside the large roster function.

Observed insertion gates:

- `0x140a39858 -> 0x140a3985d`: call `0x140a35560`; nonzero skips insertion.
  If zero, call `0x140833900` at `0x140a39874`; require return `>= 3`; then
  write `player + 0xb4` into the array at `0x140a39899-0x140a398a5`.
- `0x140a39919 -> 0x140a3991e`: same pattern; `0x140833900` at
  `0x140a39939`; require return `>= 3`; write id at `0x140a3995e-0x140a3996a`.
- `0x140a39a0d -> 0x140a39a12`: if not already in the array, a third insertion
  path calls `0x140833900` at `0x140a39a67`; require return `>= 2`; write id at
  `0x140a39a71-0x140a39a7e`.
- `0x140a39c39 -> 0x140a39c3e`: replacement path calls `0x140833900` at
  `0x140a39c51`; require return `>= 3`; replace the last candidate id at
  `0x140a39c76-0x140a39c81`.

Runtime correlation:

- With the insertion-quality bias enabled, player `5320` reaches
  `candidate_insert_primary` with `result=0 adjusted=3`, then reaches
  `0xa39d24` with `arg3_self_id_offset=16`. He is inserted into the id array.
- In the same run, `5320` does not reach the later `0xa3a6af`/`0xa3ad15`
  status24 pass because `0xa39d24` returns `1` and the native branch compacts
  him out of the pointer list.
- Player `5381` does reach `0xa3a6af`/`0xa3ad15`; his `0xa39d24` return is
  `0`, so he avoids that candidate-removal branch.

Conclusion: there are two distinct gates. The insertion gate is
`0x140833900 >= threshold`; once that is passed, `0xa39d24` can still remove a
candidate whose id is now present in the candidate array.

## 0xa39d24 branch likely removes or penalizes candidates

Observed disassembly:

- `0x140a39d1f`: call `0x140a35560`.
- `0x140a39d24`: branch only if return equals `1`.
- If `team + 0x187f == 0`, then `0x140a39d41-0x140a39d54` decrements the
  pointer-list count and replaces the current pointer with the last pointer.
  That is list compaction/removal.
- If `team + 0x187f != 0`, then `0x140a39d5f-0x140a39d96` scales down
  `player + 0xd44` and `player + 0xd46`.

Conclusion: forcing a non-array player to return `1` at `0xa39d24` sends him
into a removal/penalty path, not a promotion path. For an already-inserted
minor foreign candidate, suppressing `1 -> 0` is the safer way to let the
native status24/status25 stages judge him naturally.

## Why 0xa3b24c did not fire

Observed disassembly:

- `0x140a39092`: initializes stack counter `0x44(%rsp)` to zero.
- `0x140a3a405-0x140a3a413`: when a player gets `status[0x24] = 4` and
  `player[0xc78] == 1`, increments `0x44(%rsp)`.
- `0x140a3aaf8-0x140a3ab05`: another `status[0x24] = 4` path also increments
  `0x44(%rsp)` for `player[0xc78] == 1`.
- `0x140a3ac0b-0x140a3ac45`: some `status 4` players can be converted to
  `status 5`, decrementing the local counter.
- `0x140a3ac6d`: writes the final counter back to `0x44(%rsp)`.
- `0x140a3b1db`: if `0x44(%rsp) != 0`, jumps past the `0xa3b24c` loop.
- `0x140a3b247-0x140a3b260`: if the loop runs, it looks for
  `status[0x24] == 5 && status[0x25] == 0`, then writes `status[0x25] = 4`.

Runtime correlation:

- Latest logs show player `5406` reaches `0xa3ad15` with `status24=4` and
  `player_c78=1`, then reaches `0xa3b2cb` with `status24=4 status25=2`.
- Latest logs do not show `0xa3b24c` for `5320` or `5381`.

Conclusion: missing `0xa3b24c` is probably expected. The fallback loop only runs
when no `status 4`/active counter remains. It is not the main gate for the
foreign minor-league players we are studying.

## 0x140833900 is now the main research target

The roster routine uses `0x140833900` repeatedly as a thresholded player/team
quality function. In the insertion phase, returns below `3` or `2` prevent
candidate-array insertion.

Immediate read-only instrumentation targets:

- Return site `0xa39879`: log `0x140833900` result before the `>= 3` gate.
- Return site `0xa3993e`: log `0x140833900` result before the `>= 3` gate.
- Return site `0xa39a6c`: log `0x140833900` result before the `>= 2` gate.
- Return site `0xa39c56`: log `0x140833900` result before the replacement
  `>= 3` gate.
- At `0xa39d24`, log `team + 0x187f` because the branch chooses removal vs
  rating penalty.
- Keep logging the first six ids at `team + 0x1864`/arg3 before and after the
  insertion phase.

Implemented follow-up instrumentation:

- Added `OOTP27_AI_PLAYER_QUALITY_TRACE_RVA = 0x00833900`.
- Added `ootp_kbo_ai_player_quality_trace_wrapper`, a read-only wrapper around
  `0x140833900`.
- The trace labels the known roster call sites as:
  - `0xa39879` -> `candidate_insert_primary`
  - `0xa3993e` -> `candidate_insert_secondary`
  - `0xa39a6c` -> `candidate_insert_depth`
  - `0xa39c56` -> `candidate_replace`
  - `0xa3aace` / `0xa3aaee` -> later status-gate probes
  - `0xa3b09e` / `0xa3b0e5` -> later `status25` assignment gates
- The trace logs foreign-player result, player id, team/current/active ids,
  league, args, roster status bytes mirrored on the player, and core ratings.

Candidate implementation direction after measurement:

- Do not force `0xa39d24` return `0 -> 1`.
- Bias the `0x140833900` return at insertion call sites for eligible
  KBO minor-league foreign players, raising it only enough to pass the native
  threshold.
- Keep quota rules inside this low-level bias. The bias should express
  "foreign player carries opportunity cost and should be considered seriously",
  not "force every foreign player onto every active roster".

## 2026-05-11 follow-up patch

- Disabled the earlier `0xa39d24` team-player-fit `0 -> 1` probe by making the
  final-target predicate return false.
- Added `disable_ai_player_quality_bias.txt` as a runtime kill switch.
- `ootp_kbo_ai_player_quality_trace_wrapper` now applies a narrow candidate
  insertion bias:
  - only at `0xa39879`, `0xa3993e`, `0xa39a6c`, and `0xa39c56`;
  - only when the original result is below `3`;
  - only for KBO minor-league foreign players;
  - only when the resolved parent active team passes
    `kbo_custom_foreign_policy_callup_allows`.
- The log now records both `result` and `adjusted`, plus `bias`,
  `bias_allowed`, and `bias_team`.

## 2026-05-11 candidate-prune patch

- Added a narrow `0xa39d24` team-player-fit bias:
  - only when native result is `1`;
  - only for KBO minor-league foreign players;
  - only when the resolved parent active team passes
    `kbo_custom_foreign_policy_callup_allows`;
  - returns `0` so the native candidate-removal/penalty branch is skipped.
- Added phase labels to the team-player-fit trace. The important one here is
  `candidate_prune_or_penalty` at `0xa39d24`.
- This patch does not force `status24`, `status25`, or a roster move. It only
  keeps a quota-allowed foreign candidate alive long enough to reach the
  existing OOTP status assignment passes.

## 2026-05-11 status25 gate patch

- Latest logs showed natural success cases returning `4` at `0xa3b0e5`
  (`status25_2_gate`) and then reaching `status24=4 status25=2`.
- Added a narrow `0xa3b0e5` player-quality bias:
  - only when native result is below `4`;
  - only for KBO minor-league foreign players;
  - only when `player[0xc78] == 1`, matching the native status candidate path;
  - only when the resolved parent active team passes
    `kbo_custom_foreign_policy_callup_allows`.
- Added `disable_ai_player_quality_status25_bias.txt` as a runtime kill switch.
- The player-quality trace now logs `bias_kind`, so the next expected signal is
  `phase=status25_2_gate result=0 adjusted=4 bias=1 bias_kind=status25`,
  followed by a later status lookup with `status24=4 status25=2`.
- Bias application now happens before trace suppression, so suppressing noisy
  logs does not silently disable the AI roster bias.

## 2026-05-11 status25 11 overwrite finding

- The `0xa3b0e5` gate was not simply a `result >= 4` check. Static
  disassembly shows it compares the player-quality result against `r13 + 1`
  and writes `status25=2` only when the result is greater than that slot count.
- For the target foreign players, the new status25 player-quality bias did
  fire:
  - `player=5381`: `phase=status25_2_gate result=0 adjusted=4 bias=1`
  - `player=5320`: `phase=status25_2_gate result=0 adjusted=4 bias=1`
- Immediately afterward, native code around `0xa3b18d` can overwrite that
  `status25=2` with `status25=11` when `player[0xac]` is high enough. The
  target players both had `value_ac=220`, while a natural success comparison
  had `value_ac=4` and stayed on `status25=2`.
- Added a narrow status-lookup rescue:
  - only at `0xa3b2cb` and `0xa3b3f6`;
  - only for `status24=4 status25=11` KBO minor-league foreign players;
  - only when `player[0xc78] == 1`;
  - only when the resolved KBO parent active team passes the custom foreign
    callup quota check.
- Added `disable_ai_status25_11_rescue.txt` as the runtime kill switch.
- Status-lookup trace suppression now happens after the adjustment attempt, so
  the patch cannot be silently disabled by the 1200-line log cap. Adjusted rows
  should show `adjust_kind=status25_11_to_2` and `adj_status25=2`.
- The older `foreign_status_write_probe` experiment is now default-off and only
  runs when `enable_foreign_status_write_probe.txt` is present. It had been
  useful for early measurement on players `5320` and `5381`, but leaving it
  default-on could contaminate the new status25 experiment.

Current status-byte interpretation from logs and disassembly:

- `status24=4 status25=2` is the strongest observed success signature. Active
  or naturally promoted examples keep this pair through later lookup callers
  such as `0x836a8d`, `0xa7417b`, `0xa77797`, `0xa8f741`, `0xa35279`, and
  `0xa3b2cb`.
- `status24=4 status25=11` is the failing foreign-player signature after the
  status25 bias. The player reached the desired `status24=4` bucket but was
  moved into a protected secondary bucket before final role assignment.
- `status25=11` by itself is not necessarily bad for already-active players;
  logs show active KBO players with `status24=0 status25=11`. The dangerous
  case for this experiment is specifically `status24=4 status25=11` on a KBO
  minor-league foreign candidate.
- The loop at `0xa3ac90` copies `status24` into `status25` unless `status25`
  is already `11`, then zeroes `status24`. This makes `11` a native protected
  bucket and explains why it can survive later normalization.
- `player[0xc79]` is a separate role/result byte. Around `0xa3b2ed`, OOTP
  calls `0x140a35560`; result `1` writes `c79=11`, result `6` writes `c79=13`,
  and all other results write `c79=12`. That is related to role display or
  assignment class, but it is not the same thing as `status25`.

Next validation after restarting OOTP with the new DLL:

- Expected player-quality line:
  `phase=status25_2_gate result=0 adjusted=4 bias=1 bias_kind=status25`.
- Expected new status-lookup line:
  `caller_rva=0xa3b2cb ... status24=4 status25=11 adjusted=1 adjust_kind=status25_11_to_2 ... adj_status25=2`.
- If the player still remains in the minors after that, the next target is not
  the player-quality score anymore. It is a later consumer of the final status
  table, likely one of the roster-move traces after `0xa3b3f6`.

## 2026-05-11 default-status consumer finding

The next consumer is now narrower than the previous note suggested. Static
disassembly found a second status lookup helper:

- `0x1407ef820` returns `player + 0xe88` when present, otherwise tail-calls
  `0x1407ef670(player, player[0xec4])`.
- The roster consumer around `0x140ddb18e-0x140ddb1a3` calls this helper and
  directly checks `status[0x25] == 2`.
- If that check passes, it pushes the player into the local candidate vector
  via `0x14043d9e0`.

Patch added:

- Added `ootp_kbo_player_default_status_lookup_trace_wrapper` for `0x7ef820`.
- This hook is implemented without a trampoline because the native function is
  a tiny branchy wrapper; copying the short `jne` into a normal trampoline would
  be unsafe.
- The wrapper logs `phase=roster_default_status25_2_gate` for caller
  `0xddb196`.
- It also applies the same narrow `status24=4 status25=11 -> status25=2`
  rescue at that consumer if the earlier `0x7ef490` path did not persist the
  byte change.

Next validation after loading a fresh DLL:

- Look for:
  `ootp player-default status lookup trace ... caller_rva=0xddb196 phase=roster_default_status25_2_gate`.
- If the line shows `status25=2`, the native consumer is seeing the intended
  active-roster candidate marker.
- If it still shows `status25=11`, the new default-status rescue should show
  `adjust_kind=default_status25_11_to_2`.
- If the line is absent, the player is not reaching the later roster-consumer
  pass, so the problem is before the `DDB196` consumer rather than in the final
  move functions.

## 2026-05-11 roster eligibility gate finding

Static disassembly narrowed the pre-consumer failure point to
`0x140836800`. This is not a player-quality scorer. It is a roster/status
eligibility gate used before several AI candidate-vector inserts.

Relevant caller sites:

- `0xdda0e5`, `0xdda1ba`, `0xdda2d6`
- `0xdda471`, `0xdda521`
- `0xddb18a`, `0xddb229`

Every observed roster pass sets:

- `arg2=1`
- `arg3=1`
- `arg4=1`
- `arg5=<cap seed from manager context>`
- `arg6=<context flag byte>`

Important static checks inside `0x140836800`:

- `player[0xf62] != 0` is an immediate reject.
- With `arg2 != 0`, roster type `1` rejects on `player[0xf65] != 0`.
- With `arg2 != 0`, roster types `2..10` reject on `player[0xf6a] != 0`.
- It resolves `player[0xec4]`, falling back to `player[0x58]` and then
  `player[0x68]`; if no default team can be resolved, the player is rejected.
- If `arg4 != 0`, it calls sub-gate `0x140836a60(player, type, arg5_team)`.
- It then reads the default status pointer through `player[0xe88]` or
  `0x1407ef670(player, player[0xec4])`.
- If `arg6 == 0`, `status[0x21] > 0` rejects the player.
- If `arg6 != 0`, `arg2 != 0`, `status[0x21] > 0`, and `player[0x1694] > 0`,
  the player is rejected unless a global bypass flag is active.

Important static checks inside sub-gate `0x140836a60`:

- It calls `0x1407ef490(player, default_team_id)` and reads the default-team
  status.
- It uses `status[0x27]`, `status[0x28]`, and `status[0x08]`.
- `arg5` is not a team id here; it is used as the starting cap seed for the
  role-count comparison when the richer status path is unavailable.
- It compares native role/count helpers around `0x14089b320`,
  `0x14089b510`, and `0x14089b7b0` against a small cap, usually clamped to
  `1..3`.
- This makes the next measurement target the extra status bytes on the
  default-team status table, especially `status8`, `status27`, and `status28`.

Patch added:

- Added `ootp_kbo_ai_roster_eligibility_trace_wrapper` for `0x836800`.
- The wrapper must use six arguments. The first draft used five; that would
  fail to preserve `arg6` when calling the trampoline and could contaminate AI
  behavior. The built/released DLL was corrected to the six-argument ABI.
- The trace logs:
  - caller/phase/result
  - `likely_gate`
  - normalized roster type
  - all six args
  - default status bytes `08/21/22/24/25/26/27/28/2b`
  - player flags `f62/f65/f6a`, `874/896`, and `count1694`.

Operational note:

- OOTP process `42248` loaded a DLL before the six-argument ABI correction.
  Restart OOTP before trusting `0x836800` logs or advancing a test day.

## 2026-05-11 current A3B2 state-machine finding

The live `42248` logs are useful for the earlier path, but not trustworthy for
`0x836800` because that process loaded the pre-ABI-fix DLL.

Useful observations from `42248`:

- `0xa3b0e5` still applies the status25 bias for the target foreign players.
- `0xa3b2cb` still sees `status24=4 status25=11` and applies
  `status25_11_to_2`.
- No `ootp ai roster eligibility trace` rows appeared, so this run has not
  reached the `DDA/DDB` eligibility consumers yet.
- The repeated loop is still in the `A398/A39D/A3B2` state machine.

Static mapping around `0x140a3b2ed-0x140a3b38a`:

- `0x140a3b2ed` calls `0x140a35560(team, player, ...)`.
- If the result is `1`, OOTP writes `player[0xc79]=11`.
- If the result is `6`, OOTP writes `player[0xc79]=13`.
- Any other result writes `player[0xc79]=12`.
- This means the observed final fit rows are not necessarily failures:
  `5320 result=1 -> c79=11`, `5381 result=3 -> c79=12`.

Next likely research step:

- After a clean restart with the fixed DLL, check whether `0x836800` fires.
- If it still does not fire, the next hook should be a status-write probe around
  `0x140a3b2d1`, `0x140a3b2fb`, and `0x140a3b382` to record the exact moment
  `player[0xc79]` is reset and rewritten.

## 2026-05-11 DDB consumer mapping

The clean `41192` run confirms that the current patches reach internal AI
candidate/status paths, but not the native player-add or roster-move functions:

- No `foreign roster-move trace` or `foreign team_add caller trace` rows were
  emitted for `pid=41192`.
- The previous `foreign roster-move trace` rows in the full log came from an
  older process (`pid=29576`) and unrelated league-203 activity.
- Therefore the current failure is still inside OOTP's AI roster candidate
  assembly, before any real assignment/move function is called.

Static DDB consumer mapping:

- `0xddb058` and `0xddb0f7` insert the player into the local candidate vector
  when default `status24 == 2`.
- `0xddb196` inserts when default `status25 == 2`.
- `0xddb250` / `0xddb25e` insert when default `status24 == 11` or
  `status25 == 11`, after `0xa35560(...) != 1`.
- `0xddb316` / `0xddb324` insert when default `status24 == 4` or
  `status25 == 4`, after `0xa35560(...) != 1`.
- `0xddb5f5` inserts when `status25 == 9`.
- `0xddb694` / `0xddb6a2` insert when `status24 == 7` or `status25 == 7`.
- `0xddb7ce` inserts when `status25 == 12`.
- `0xddb8c2` inserts when `status24 == 5`.

Runtime observation from `pid=41192`:

- `5320` and `5381` reached `status24=4 status25=2`, but were not observed at
  the `0xddb196` consumer. They were seen in broader DDA/DDB scans instead.
- `5417` reached `0xddb196`, but with `status24=5 status25=0`, so it was not
  inserted by the `status25 == 2` consumer.
- Some target players later show `f62=1`, which makes `0x836800` reject them
  immediately. Because static scans show `f62` is written by broad roster/cache
  maintenance paths, it should not be globally overridden yet.

Instrumentation added:

- Expanded `ootp player-default status lookup trace` phase labels for the DDB
  consumer sites above.
- Added `status8/status21/status27/status28` plus `f62/f65/f6a/count1694` to
  the default-status trace. The next run should show exactly which DDB consumer
  sees each foreign player and whether the player is blocked by a transient
  roster/cache flag at that moment.

Current hypothesis:

- The A3 status tweaks are real, but they are not enough by themselves because
  the later DDB roster-candidate pass either does not revisit the same player in
  the `status25 == 2` consumer or sees the player after an `f62`-style processed
  flag is set.
- The next safe experiment is not our own roster manager yet. It is to identify
  the exact DDB consumer that should own `status24=4/status25=2` minor foreign
  players, then either bias the earlier status into that consumer's expected
  bucket or narrowly bypass the transient gate only at that consumer.

## 2026-05-11 pointer-vector candidate push hook

The follow-up `pid=17288` log changed the local hypothesis:

- `5381` did reach the DDB final status gates.
- `5381` reached `final_status24_2_gate`, `final_status25_2_gate`, and
  `roster_default_status25_2_gate` with `status24=4 status25=2`.
- No `foreign roster-move trace` or `foreign team_add caller trace` rows were
  emitted for `pid=17288`.
- The daily audit snapshot for `20260511` still has `5381` on
  `current_team_id=11 active_team_id=8 current_league_id=101`.

Static confirmation for the next probe:

- `0x14043d9e0` is a thunk to `0x141cbaa40`.
- `0x141cbaa40(rcx, rdx)` appends `rdx` into a pointer vector whose count lives
  at `vector[0x0c]`; element size is 8 bytes.
- In the DDB roster candidate builders, `rcx = [rbp - 0x79]` and `rdx = rbx`.
  `rbx` is the player pointer fetched from the roster vector via `0x4350e0`.
- Therefore this helper can safely answer whether a foreign player actually
  enters the local candidate vector after the status/eligibility gates.

Instrumentation added:

- Added `OOTP27_POINTER_VECTOR_PUSH_TRACE_RVA = 0x0043D9E0`.
- Added `ootp pointer-vector push trace` under
  `enable_foreign_ai_roster_research_hooks`.
- The wrapper filters to return addresses in the `0xdd9000-0xddc700` roster AI
  candidate range and logs only plausible foreign player pointers.
- The log records `before_count`, `after_count`, `before_contains`,
  `after_contains`, and `inserted`, plus status bytes and `f62/f65/f6a`.

What the next run should answer:

- If `5381` logs `inserted=1` and still no roster move occurs, the failure is
  after candidate assembly: final selection/ranking or move scheduling.
- If `5381` never logs `inserted=1`, the apparent gate pass did not translate
  into a real candidate-vector append; then the exact caller phase tells which
  consumer needs the next narrow bias.

## 2026-05-11 final roster-select return hook

Static follow-up after the pointer-vector probe:

- The broad DDB candidate/selection function starts at `0x140dd86c0`.
- Its calling convention is consistent with
  `player* fn(team_ptr, roster_index, depth_hint)`.
- The function builds the local candidate vector at `[rbp - 0x79]`, sometimes
  builds a secondary vector at `[rbp - 0x41]`/`[rbp - 0x21]`, then returns the
  chosen player pointer in `rax`.
- The final selector around `0xddc65c-0xddd067` handles:
  - candidate count `1`: return the only candidate.
  - candidate count `>1`: probabilistic/priority passes using `f25`, `f65`,
    status/position helpers, then sorted score fields such as `0xfe0/0xfe4`.
  - major-league-ish path at `0xddc700`: creates a foreign-looking subvector
    from candidates with `f25 == 100`, then randomly selects from it if present.

Runtime context from existing logs:

- `5381` is seen with `f25=0` in early eligibility calls and later with
  `f25=26 f65=1`.
- Because the final selector has explicit `f25 == 100` and `f25`-weighted
  branches, `f25` is now a primary suspect for why a foreign candidate can pass
  earlier gates yet fail the final native AI choice.

Instrumentation added:

- Added `OOTP27_AI_ROSTER_SELECT_TRACE_RVA = 0x00DD86C0`.
- Added `ootp ai roster select trace` under
  `enable_foreign_ai_roster_research_hooks`.
- The wrapper is pure tracing: it calls the original selector and logs the
  returned player pointer, caller RVA, team/team league, `roster_index`,
  `depth_hint`, selected player ids/teams/leagues/status bytes, `f25/f62/f65/f6a`,
  and ratings.

Next interpretation:

- If `5381 inserted=1` appears in `ootp pointer-vector push trace` but
  `ootp ai roster select trace` returns another player for the same team/index,
  the native final selector is the lever.
- If `5381` is selected by `0xdd86c0` but still no move trace fires, the failure
  moves one step later into move scheduling/execution.

Additional `f25` notes:

- Static xref scan found only eight `player+0xf25` write sites.
- The main AI clusters copy it from the temporary output of `0x140835300`
  (`out+5`) into `player+0xf25`; the same helper also produces the `0xf06`
  score from `out+2`.
- The selector reads `f25` as a score/weight rather than a boolean foreign flag:
  it compares against thresholds `75/80/90/95/100`, and at `0xddc6d1-0xddc6e4`
  does `random(100) < f25`.
- Therefore the next likely native lever is not "force foreign" globally. It is
  a narrow, call-site-specific score bias around the `0x835300` output or the
  `0xdd86c0` final selection path.

## 2026-05-11 DD86C0 context/source-vector remap

Static follow-up corrected the meaning of the `0x140dd86c0` arguments:

- `rcx` is not a team pointer. It is an AI roster-selection context pointer.
- `edx` is the slot index.
- `r8d` is a depth/status hint.
- The slot block is loaded from `context + slot_index * 8 + 0x108`.
- The source player vector for that slot is at `slot_block + 0x3ca8`.
- The slot team id is loaded from `context + slot_index * 4 + 0x4f8`.
- The chosen/working pointer is also mirrored at `context + 0x578`.
- Context side pointers worth watching are `context + 0xd8` and
  `context + 0x5b8`; the former exposes a league-ish type word at `+0x26`.

Patch added:

- Reworked `ootp_kbo_ai_roster_select_trace_wrapper` to log the context, slot,
  slot block, slot team id, source vector pointer/data/count, and selected
  source index.
- The wrapper now scans up to 256 entries from the DD86C0 source vector and
  records:
  - total foreign candidates in the source vector;
  - known test-target candidates;
  - whether the returned player pointer came from that source vector;
  - compact summaries for the first three foreign source candidates.

Next interpretation after a fresh restart:

- `source_foreign=0`: the foreign player was filtered before DD86C0's source
  vector. Continue upward into the A3/DDA eligibility and status writers.
- `source_foreign>0`, pointer-vector insert exists, but `selected_foreign=0`:
  the final DD86C0 selector/ranking is the native lever.
- `source_foreign>0` but no pointer-vector insert for the same player: the
  source roster had the player, but the local candidate-vector gate still
  rejected him.
- `selected_source_index=-1` with a plausible selected player: either DD86C0
  selected from a secondary local vector, or the source vector layout/scan limit
  needs one more correction.

## 2026-05-11 pid 28168 log result

The fresh `pid=28168` run loaded the new DD86C0 trace correctly:

- `KBOFix loaded` at 20:39:57.
- AI roster research hooks were installed, including `0x833900`, `0x836800`,
  `0xa35560`, `0x7ef490`, `0x7ef820`, `0x1cbaa40`, and `0xdd86c0`.
- `ootp ai roster select trace`: 149 rows.
- Rows with `source_foreign > 0`: 88.
- Rows with `selected_foreign=1`: 0.
- `ootp pointer-vector push trace`: 1 row.
- Foreign roster move/team-add trace rows: 0.

The important target signal:

- Player `5381` passed eligibility/status as `status24=4 status25=2`.
- Player `5381` was pushed into a DD86C0 local candidate vector:
  `caller_rva=0xddb1a8 phase=push_status25_2 inserted=1`.
- Later DD86C0 source summaries still contained `5381`, but the selected
  player was a domestic player.

Conclusion:

- This is no longer primarily an A3 status-machine failure.
- At least one foreign target reaches the local candidate-vector append.
- The current failure is now likely inside DD86C0's final candidate ranking or
  draw logic, or immediately after it.

Patch added after this finding:

- DD86C0 wrapper now installs a per-thread live trace around the original
  selector call.
- The pointer-vector push hook records local candidate appends into that live
  trace while DD86C0 is executing.
- DD86C0 logs now include `local_candidates`, `local_foreign`,
  `local_targets`, `selected_local_index`, and first local foreign-candidate
  summaries.

Next validation:

- If `local_foreign > 0` and `selected_local_index >= 0`, DD86C0 selected from
  the recorded local vector and we can compare the foreign candidate's local
  index/score against the selected player.
- If `local_foreign > 0` but `selected_local_index == -1`, DD86C0 selected from
  a secondary vector or a later copied/sorted vector, so the next hook should
  target the sorter or random draw after local candidate assembly.

## 2026-05-11 pid 19384 split result and score probe

The next fresh run (`pid=19384`, loaded at 20:50:33) separated the failure into
two distinct native paths:

- `5381` appears in the DD86C0 source vector but not in the DD86C0 live local
  candidate vector. Examples showed `source_foreign=1 source_targets=1` with
  `local_foreign=0 local_targets=0`. This means the player is still blocked by
  the source-to-local filter for that slot.
- `5417` reaches the DD86C0 live local candidate vector through
  `caller_rva=0xdda54c`, but DD86C0 returns a domestic player. Examples showed
  `local_foreign=1 local_targets=1 selected_local_index=3/14` while the foreign
  candidate was at `local_f1_idx=1`.

Static disassembly around `0x140dda450-0x140dda54c` explains the `0xdda54c`
push:

- `0x140dda46c` calls `0x140836800`; failure jumps over the first local push.
- `0x140dda486` calls `0x140a35560`; `eax == 1` skips the push.
- The fallback branch requires enough depth, one of
  `status24 == 5 || status25 == 5 || status26 == 3`, a `0x1408352b0` role check
  returning zero, another `0x140836800` pass, and another `0x140a35560` pass.
- The push at `0x140dda547` returns to `0xdda54c`; this is now labelled
  `push_status5_depth_candidate` in the pointer-vector trace.

Static disassembly around `0x140ddc600-0x140ddd080` shows DD86C0 is not a simple
"take best player" function:

- It has an early random branch using `random(100) < player+0xf25`.
- It has several scan-and-return branches using role/status helpers.
- Later branches write score-like fields at `player+0xfe0` and `player+0xfe4`,
  sort candidate vectors, and then return either the first or a random member.

Patch added:

- `ootp ai roster select trace` now logs selected-player `score_fe0` and
  `score_fe4`.
- Added `ootp ai roster select score probe`, which compares the selected player
  against the first source foreign candidate and first live local foreign
  candidate after DD86C0 returns.
- Added `KBO_POINTER_VECTOR_PUSH_CALLER_DDA54C_RVA` and labelled it as
  `push_status5_depth_candidate`.

Next validation:

- If `5417 local_f1_fe0/fe4` is lower than the domestic selected player, the
  next native lever is the DD86C0 score writer/sorter path.
- If `5417 local_f1_fe0/fe4` is higher but still not selected, DD86C0 likely
  returned before the sort branch through an earlier role/random branch.
- If `5381` still appears only in source summaries, instrument or patch the
  source-to-local gates around `0x836800`, `0xa35560`, and `0x8352b0`.

## 2026-05-11 pid 5572 selector succeeds, apply path is next

The next one-day run (`pid=5572`) showed that the selector can now return
foreign players:

- `5417` was selected at `caller_rva=0xdf4d81` with
  `selected_foreign=1`, `source_f1=5417`, `score_fe0=0`,
  `score_fe4=4`.
- `5381` was selected at `caller_rva=0xdf4d81` with
  `selected_foreign=1`, `local_f1=5381`, `score_fe0=0`,
  `score_fe4=3`.
- No `foreign roster-move trace` rows fired for `pid=5572`, and the scoped
  snapshot still had `5381` at `current=11 active=8` and `5417` at
  `current=13 active=6`.
- Immediately after `5381` was selected, later eligibility checks rejected him
  with `likely_gate=f62` and `f62=1`.

Static disassembly explains that `f62` flip:

- The `0xdf4d81` caller stores the selected pointer at `context+0x578`, then
  calls `0x140de4160(context, slot_index, selected)`.
- `0x140de4160` builds/repairs slot assignment state and calls
  `0x140de6730` for actual slot application.
- `0x140de6730` writes `player+0xf62 = 1`, `player+0xf68 = 1`, updates
  `player+0xef8`, and writes the selected player id into the slot block.

Patch added:

- Added `OOTP27_AI_ROSTER_APPLY_SELECTION_TRACE_RVA = 0x00DE6730`.
- Added `ootp ai roster apply-selection trace`, installed with the foreign AI
  research hooks.
- The new trace logs the `0xDE6730` call before/after for foreign or targeted
  players, including `slot_index`, `target_slot`, `roster_code`, slot player id
  changes, `current/active`, `f61/f62/f65/f68/f1a`, `ef8`, and selector scores.

Next validation:

- Restart/inject with the new DLL, advance one day, and inspect
  `ootp ai roster apply-selection trace` for `5381` and `5417`.
- If `0xDE6730` logs `before_slot_player != after_slot_player` for them but
  the audit still shows no team change, the slot assignment is lineup/depth
  state only and not the team roster move.
- If `0xDE6730` never fires for selected foreign players, the cancellation is
  between `0xDE4160` and `0xDE6730`.

## 2026-05-11 pid 42988 f65 is the current blocker

The `pid=42988` run loaded the `0xDE6730` apply-selection trace and produced one
apply row:

- `player=5444` changed `slot_player 3398 -> 5444`, `slot_code 5 -> 11`,
  and `f62/f68 0 -> 1`.
- `current=20 -> 20` and `active=10 -> 10` did not change, so `0xDE6730`
  is applying slot/depth state, not moving the player between team rosters.

The same run showed `5381` being rejected by `0x836800` with
`likely_gate=f65`, `f62=0`, and `f65=1`. Static disassembly identifies the
relevant writer in the player roster-flag update function at `0x140d98630`:

- `0x140d98963`: `f65 = (f25 <= 0x31)`.
- A later league/settings branch can overwrite `f65` around `0x140d989af`.
- `0x140d98b03`: another low-threshold branch can force `f65 = 1` when
  `f25` is below the team/league threshold.

Patch added:

- Added `OOTP27_AI_ROSTER_F65_UPDATE_TRACE_RVA = 0x00D98630`.
- Added `ootp ai roster f65 update trace`, installed with the foreign AI
  research hooks.
- The new trace logs before/after `f25/f26/f38/f61/f62/f65/f6a/f06/fec`
  for targeted players and changed foreign players, plus caller RVA and status.

Next validation:

- Restart/inject with the new DLL, advance one day, and inspect
  `ootp ai roster f65 update trace` for `5381`.
- If `5381` logs `f25 <= 49` with `f65 0 -> 1`, the immediate lever is the
  native threshold/path that classifies low `f25` players out of roster type 1
  and 11+ candidate pools.

## 2026-05-11 source-to-local candidate gate instrumentation

The `pid=42900` run showed a different failure from the previous `f65` case:

- `5381` had `f25=52`, `f65=0`, and `0x836800` eligibility returned `1`.
- `5417` had `f25=86`, `f65=0`, and also passed eligibility.
- Both appeared in the `DD86C0` source vector, but `local_foreign=0` and
  `local_targets=0`; `0xDE6730` apply-selection never fired.

Static disassembly shows that the source-to-local builder is branch ordered:
once the local candidate vector has any entries, later status branches are
skipped. The next useful question is therefore whether a foreign source
candidate is rejected inside its own early branch, or whether an earlier
domestic branch fills the local vector before the foreign candidate's branch is
visited.

Patch added:

- Added `OOTP27_AI_ROSTER_ROLE_CHECK_TRACE_RVA = 0x008352B0` and
  `ootp ai roster role-check trace`.
- The new role-check trace logs the `0x8352b0(player, role_arg)` result for
  foreign/target players inside the `DD86C0` roster-selection region.
- `DD86C0` live trace now records `first_push_rva`, `first_push_phase`, and
  the push caller for each recorded local candidate.
- Added `ootp ai roster source-local gate trace`, which compares each
  foreign/target source candidate against the recorded local candidate vector
  and logs visible status-branch masks, min branch order, local index, local
  push phase, and whether an earlier local push may have short-circuited the
  candidate.

Next validation:

- Restart/inject, advance one day, and inspect:
  - `ootp ai roster role-check trace ... player=5381`
  - `ootp ai roster source-local gate trace ... player=5381`
  - `first_push_phase` on the matching `ootp ai roster select trace`
- If `role-check result != 0` at `source_initial_role_gate` or
  `source_status5_role_gate`, the role/position helper is the immediate native
  gate.
- If `prior_local_possible=1`, the likely lever is branch priority rather than
  a per-player rejection.

## 2026-05-12 role-slot trace expansion

The last run showed `0x8352b0` returning nonzero at source-to-local gates such
as `0xDDA440` and `0xDDA4F0`, which skips the local candidate push for target
foreign players. Patch expanded the next trace pass:

- `ootp ai roster role-check trace` now logs the native-equivalent computed
  result, scan bounds, scan mask, full role-slot mask/count, first nonzero role
  slot, and whether the current caller treats the result as a source-push
  blocker.
- `ootp ai roster source-local gate trace` now includes `f06`, `fec`, and all
  six role slots (`0x800..0x80a`) for the foreign/target source candidate.
- `ootp ai roster f65 update trace` now logs before/after values for all six
  role slots, so we can see whether the f65 updater also mutates the role gate
  state.
- `ootp ai roster apply-selection trace` now logs before/after `f06`, `fec`,
  and all six role slots around `0xDE6730`.
- The candidate push at `0xDDA49C` is now labelled
  `push_initial_status4_or_26_candidate`, matching its source branch order.

Next validation:

- Restart with the rebuilt DLL, advance one day, and compare `result`,
  `computed`, `scan_mask`, `source_push_blocked`, and the corresponding
  `role800..role80a` fields for `5381`/`5417`.
- If role slots change before the source gate, trace the writer next. If they
  are already nonzero before roster selection starts, inspect roster/depth-chart
  initialization rather than final selection.

## 2026-05-12 DD86C0 sort-path instrumentation

Static pass after the `pid=44724` logs narrowed the two active failure modes:

- `5381` can still be filtered before local insertion by `0x8352b0` role-slot
  checks at the source gates.
- `5417` can enter the DD86C0 local vector but lose to domestic players after
  the selector computes `0xfe0/0xfe4` scores and sorts the local vector.

Relevant disassembly:

- `0x1CBAB40` is the pointer-vector sort helper used by DD86C0.
- DD86C0 calls it from `0xDDCCE1`, `0xDDCF1B`, and `0xDDD055`.
- The comparator is `0x91E920`, comparing `player+0xfe0` first and
  `player+0xfe4` second.
- `0x844DD0` is not the root role writer; it shifts role slots down
  (`0x802 -> 0x800`, `0x804 -> 0x802`, etc.) and clears the tail slot.

Patch added:

- Added `OOTP27_POINTER_VECTOR_SORT_TRACE_RVA = 0x01CBAB40`.
- Added `ootp pointer-vector sort trace/top/focus`.
- The sort trace only logs while the DD86C0 live trace is active and the
  comparator is `0x91E920`, so normal vector sorts are ignored.
- The new logs capture sorted top-six candidates, first sorted foreign, first
  target, and explicit `5381`/`5417` positions with `fe0`, `fe4`, `f25`,
  `f62`, `f65`, role mask, status, and original push index/RVA.

Next validation:

- Restart/inject with the rebuilt DLL, advance one day, and inspect
  `ootp pointer-vector sort focus`.
- If `5417` ranks below domestic candidates after sort, the score/comparator
  path is the lever.
- If `5417` ranks high but a lower domestic is returned, the next lever is the
  post-sort gate/random-return branches at `0xDDCD30..0xDDD067`.

## 2026-05-12 f25/f65 opportunity-cost bias

The `pid=38376` log showed the next blocker more clearly:

- `5381` reached `DD86C0`, entered the local candidate vector through
  `push_status25_2`, and was selected as `selected=5381`.
- Later passes rejected the same player through `0x836800` with
  `likely_gate=f62`, then after a fresh `0xD98630` roster-flag update with
  `likely_gate=f65`.
- The `0xD98630` update produced low `f25` values for target foreign players
  (`5381 f25=39 f65=1`, `5293 f25=45 f65=1`), while earlier successful passes
  had `f25=100 f65=0`.

Patch added:

- Added a narrow post-`0xD98630` opportunity-cost bias for KBO minor-league
  foreign players.
- It only applies when the custom foreign policy allows a callup for the
  resolved parent active team.
- It raises `player+0xf25` to `100` and clears `player+0xf65` when the native
  update has pushed the player below the active-roster candidate threshold.
  The first draft used `75`, matching the `0xDD26A0` eligibility cutoff, but
  `DD86C0` also has league-type-2 candidate branches that require
  `player+0xf25 == 100`, so the bias now uses OOTP's own full-fit value.
- Added `disable_ai_roster_foreign_f25_bias.txt` as a runtime kill switch.
- The `ootp ai roster f65 update trace` log now includes `f25_bias`,
  `bias_team`, `bias_allowed`, `native_f25`, and `native_f65`, so the next run
  can distinguish native output from the adjusted value.

Next validation:

- Restart OOTP with the rebuilt DLL, advance one day, and inspect
  `ootp ai roster f65 update trace` for `f25_bias=1`.
- Expected follow-up signal: later `0x836800` eligibility rows for the same
  player should no longer show `likely_gate=f65`.

## 2026-05-12 eligibility pre-bias follow-up

The next run showed the post-`0xD98630` patch firing, but `0x836800`
eligibility still saw stale or recomputed values for `5381`:

- `0xD9A396` updated `5381` with `f25_bias=1`, changing native
  `f25=35/f65=1` to `f25=100/f65=0`.
- Immediately afterward, `0x836800` callers still logged repeated
  `likely_gate=f65` rows with `f25=35/f65=1`.
- Later rows recovered to `f25=71/f65=0`, which means OOTP is still rewriting
  or reloading these roster flags between the update hook and the eligibility
  gate.

Patch added:

- Added the same narrow KBO minor-league foreign opportunity-cost correction
  before calling the original `0x836800` eligibility function.
- It reuses `disable_ai_roster_foreign_f25_bias.txt`.
- Eligibility logs now include `pre_f25_bias`, `pre_bias_team`,
  `pre_bias_allowed`, `pre_native_f25`, and `pre_native_f65`.

Next validation:

- Restart OOTP with the rebuilt DLL, advance one day, and inspect
  `ootp ai roster eligibility trace` for `pre_f25_bias=1`.
- If this works, those rows should return `result=1` without
  `likely_gate=f65`.
- If `pre_f25_bias=1` appears but the original still returns `0`, the next
  target is the deeper branch inside `0x836800`, not the upstream writer.

## 2026-05-12 source role-gate bias

The latest run showed `f25/f65` eligibility working for `5381`, but the source
candidate still failed to enter the local candidate vector in later passes.
The active blocker moved to `0x8352b0` role-slot checks in DD86C0 source
branches:

- `source_initial_role_gate` (`0xDDA440`) treats any nonzero result as a
  source-push block.
- `source_status5_role_gate` (`0xDDA4F0`) and
  `source_status24_2_role_gate` (`0xDDB04C`) have the same nonzero block
  behavior.
- `source_status9_role_gate`, `source_status25_9_role_gate`, and
  `source_status25_12_role_gate` block on `result >= 2`.
- `source_status7_role_gate` blocks on `result > 2`.

Patch added:

- Added `disable_ai_roster_foreign_role_bias.txt` as a runtime kill switch.
- Added a narrow source role-gate bias for KBO minor-league foreign players
  that already passed the opportunity-cost path (`f25 >= 100`, `f65 == 0`) and
  are not already marked selected/used (`f62 == 0`).
- The bias only applies when the custom foreign policy says the parent active
  team can call the player up.
- Instead of erasing role slots, it returns the minimum non-blocking role-check
  result for the current DD86C0 source branch.
- `ootp ai roster role-check trace` now logs `native_result`, `role_bias`,
  `role_bias_team`, `role_bias_allowed`, and
  `native_source_push_blocked`.

Next validation:

- Restart OOTP with the rebuilt DLL, advance one day, and inspect
  `ootp ai roster role-check trace` for `role_bias=1`.
- Expected signal: source gate rows that previously had
  `native_source_push_blocked=1` should now show `source_push_blocked=0`.
- Then check whether matching `ootp ai roster source-local gate trace` rows
  move from `local_idx=-1` to a real local candidate index.

## 2026-05-12 post-sort recent-move gate

The next run showed the source gate and sort path working for `5381`:

- The player reached the local candidate vector.
- `0xDDC8C5` sorted him to rank 0 under comparator `0x917380`.
- The final selected player was still a domestic candidate at sorted rank 2.

Disassembly of the return path after `0xDDC8C5` exposed another gate:

- `0xDDC8FA` calls `0x834ED0(player)`.
- The caller immediately checks `result >= 10` before returning the current
  sorted candidate.
- `0x834ED0` reads the player's latest history/date entry and the league
  current date, returning an elapsed-day-like score, with `-1` on missing data.

Patch added:

- Added `disable_ai_roster_post_sort_gate_score_trace.txt` to disable this
  hook entirely.
- Added `disable_ai_roster_foreign_post_sort_bias.txt` to disable only the
  behavior change.
- Added a custom trampoline for `0x834ED0`; the original prologue contains a
  relative conditional branch, so the generic copied-byte trampoline is unsafe
  there.
- For KBO minor-league foreign candidates that are already in the active
  DD86C0 select trace, sorted by `0xDDC8C5/0x917380`, have `f25 >= 100`,
  `f65 == 0`, `f62 == 0`, and pass the foreign callup limit, a native
  `0..9` result is lifted to `10`.
- The new log line is `ootp ai roster post-sort gate trace` with
  `native_result`, `post_sort_bias`, live candidate index, sorted rank, and the
  last sort caller/comparator.

Next validation:

- Restart OOTP with the rebuilt DLL, advance one day, and inspect
  `ootp ai roster post-sort gate trace` for `post_sort_bias=1`.
- Expected signal: the formerly rank-0 foreign candidate should now be returned
  by the `0xDDC8FA` pass instead of being skipped for the rank-2 domestic
  fallback.

Observed validation:

- `2026-05-12 04:06:21` logged `post_sort_bias=1` for player `5381`,
  changing `native_result=1` to `result=10` at caller `0xDDC8FA`.
- The matching select trace returned `selected=5381`, `selected_foreign=1`,
  `local_foreign=1`, `local_targets=1`.
- After selection, later eligibility rows showed `f62=1`, confirming OOTP had
  marked the player as used/selected in the current roster pass.

## 2026-05-12 fixed-slot/source-local correction

The next logs showed that score and post-sort selection can succeed without
the player actually landing in a roster slot:

- DE4160 reconcile rows marked selected foreign players as used
  (`f62/f68/f1a/ef8`) but still showed `player_slot=-1->-1` and
  `first_empty=-1->-1`.
- Several foreign players stayed in the DD86C0 source vector with
  `local_idx=-1`.
- Static DD86C0 disassembly corrected an earlier assumption: the fixed-depth
  branch scans six player IDs at `slot_block+0x1864`, not the source vector
  directly. A player with `f25=100` is only eligible for that branch if his ID
  is present in that fixed-slot table.

Patch added:

- `ootp ai roster source-local gate trace` now derives the slot block from
  the source vector and logs `slot_flag`, both marked player IDs, the
  fixed-depth table index, and whether the player is already the marked
  candidate.
- Added a trace-only hook for `0x836BF0`, the roster availability gate used by
  DD86C0 before eligibility/fit/push checks.
- The new `ootp ai roster availability trace` records the caller phase,
  native result, default status bytes, `c78/c79`, `f25/f62/f65/f6a`,
  `flag101f`, `count1694`, roles, and current score fields.
- Added `enable_kbo_hot_reinject_availability_trace.txt` so this trace can be
  injected into an already-running OOTP process.

Next validation:

- Hot-inject or restart with the rebuilt DLL, advance one day, and compare
  `availability trace` rows against `source-local gate trace`.
- If a foreign player is source-visible but `fixed_slot_idx=-1` and never
  reaches local candidates, the next lever is the native code that fills
  `slot_block+0x1864` or the branch that chooses source-vector versus fixed
  depth slots.

## 2026-05-12 team-player fit source-candidate patch

Follow-up logs showed several KBO minor-league foreign players with
`availability=1`, a valid fixed-depth table index, and source visibility, but
still `local_idx=-1`. The blocking edge is `0xA35560`:

- `0xA35560` falls back to `team + 0x1864` when the third argument is null.
- It scans six fixed-depth player IDs at `slot_block+0x1864`.
- If the player ID is already present there, it returns `1`.
- DD86C0 source-candidate branches then compare `eax == 1` and skip the local
  candidate push.

Patch added:

- For KBO minor-league foreign players that pass the custom foreign callup
  limit, `0xA35560` result `1` is treated as non-blocking `0` at the confirmed
  DD86C0 source candidate branches:
  `DD96A2`, `DD98B5`, `DDA48B`, `DDA53B`, `DDA619`, `DDB243`, and `DDB309`.
- The earlier A3991E/A39D24 candidate-prune bias remains in the same narrow
  path.
- Team-player fit trace suppression now only suppresses logging; the bias path
  still runs after 800 foreign trace rows.

Next validation:

- Restart OOTP with the rebuilt DLL, advance one day, and inspect
  `ootp ai team-player fit trace` for `result=1 adjusted=0 bias=1` at one of
  the DD86C0 source branches.
- The matching `source-local gate trace` should move from `local_idx=-1` toward
  an actual local candidate index for the same player.

## 2026-05-12 priority sort bonus expansion

The next run showed the source-candidate branch working, but the foreign
candidate still lost inside the native priority sort:

- `5381` reached the local candidate vector for context `5C86D860`.
- `0xDDC8C5` sorted the vector with comparator `0x917380`.
- Static disassembly confirms `0x917380` compares `player+0xf06` first, then
  `player+0xbce` on ties.
- With the previous foreign f06 bonus of `24`, `5381` had
  `f06=5`, `effective_f06=29`.
- The sorted domestic leaders had f06 values `58`, `41`, `41`, `41`, `34`,
  and `33`, so `5381` remained rank `26`.
- The selected player was domestic `5380`, with `sort_f06=32` and sorted rank
  `12`, while `5381` stayed at rank `26`.

Patch changed:

- Raised `KBO_AI_ROSTER_FOREIGN_SORT_F06_BONUS` from `24` to `64`.
- With the observed `5381` sample, this should lift `effective_f06` from `29`
  to `69`, above the current domestic top `58`, while keeping the behavior
  scoped to the existing foreign-sort-bias context and kill switch.

Next validation:

- Restart OOTP with the rebuilt DLL, advance one day, and inspect
  `ootp pointer-vector sort keys` for `foreign_sort_bonus=64`.
- Expected signal: the first target/foreign entry should move near rank `0`
  under comparator `0x917380`, and the matching select trace should return the
  foreign candidate instead of a domestic fallback.

## 2026-05-12 native apply rescue experiment

The `pid=33944` run confirmed the `64` f06 priority bonus is active, but the
next bottleneck moved later in the native flow:

- `ootp pointer-vector sort keys` reported `foreign_sort_bonus=64`; selected
  minor foreign `372` reached rank `0` with `t1_eff_f06=72`.
- `ootp ai roster reconcile trace` then marked the selected foreign
  (`f62/f68/f1a/ef8`), but the player still had no slot assignment.
- `primary_dd1b20` cleared `selected=372->0` while leaving `ptr528=372->372`.
- There was no matching `0xDE6730` apply-selection call for that selected
  player; the only native apply-selection calls in the run were for other
  foreign players such as `886` and `27313`.

Patch added:

- `primary_dd1b20` now has a guarded apply-rescue path after the native flow
  returns.
- If a plausible foreign selected player is cleared from `selected`, still
  lives in `ptr528`, is a KBO minor-league foreign candidate, passes the custom
  foreign callup limit, and has a clean status candidate shape, the hook calls
  the native `0xDE6730` apply-selection trampoline directly:
  `DE6730(context, primary_slot, selected_player, primary_target_slot, 11)`.
- The experiment is kill-switchable with
  `disable_ai_roster_foreign_apply_rescue.txt`.
- New logs:
  `ootp ai roster foreign apply rescue` for actual rescue calls and
  `ootp ai roster foreign apply rescue skip` for target-player skips.

Next validation:

- Restart OOTP with the rebuilt DLL, advance one day, and inspect
  `ootp ai roster foreign apply rescue`.
- If the rescue fires, check `before_slot_player -> after_slot_player`,
  `after_slot_code`, and the follow-up context flow for whether the player now
  stays in the active roster slot instead of returning to 2군 status.
- If it crashes, create
  `%LOCALAPPDATA%\OOTP-KBO\disable_ai_roster_foreign_apply_rescue.txt` and
  restart to disable only this experiment.

## 2026-05-12 native overwrite shield follow-up

The next `pid=11552` run showed the apply-rescue patch working, but also
identified the next native overwrite edge:

- `ootp ai roster foreign apply rescue #1` inserted target foreign `5381` into
  slot `0`: `before_slot_player=3312`, `after_slot_player=5381`, and
  `before_slot_code=6`, `after_slot_code=11`.
- Immediately afterward, native apply-selection caller `0xDE5D86` wrote
  domestic `5475` over the same slot: `before_slot_player=5381`,
  `after_slot_player=5475`, `roster_code=6`.
- After this, `5381` repeatedly appeared with `f62=1`, so the native pass had
  marked him as used even though the slot was lost.

Patch added:

- Successful apply-rescue calls now record a short-lived rescued slot entry
  keyed by context, slot block, target slot, and rescued player ID.
- `ootp_kbo_ai_roster_apply_selection_trace_wrapper` checks that short-lived
  record before calling the native apply-selection trampoline.
- For five seconds after a rescue, an immediate non-foreign, non-target
  overwrite of the rescued slot is skipped and logged as
  `ootp ai roster foreign apply rescue shield`.
- The shield is deliberately narrow: it only fires when the slot currently
  contains the recent rescued player and the incoming player is a different
  domestic/non-target player.

Next validation:

- Rebuild, restart OOTP with the new DLL, advance one day, and inspect
  `ootp ai roster foreign apply rescue shield`.
- Expected signal: the observed `0xDE5D86` domestic overwrite should be
  skipped, and the follow-up apply/context traces should show the rescued
  foreign still occupying the target active slot.

## 2026-05-12 active move follow-up

The shield run proved that the immediate overwrite can be blocked, but the
player still showed as a minor-league player in the UI. The log explains why:

- After rescue, `5381` occupied the target slot, but his player fields stayed
  `current=11`, `active=8`, `league=101`, `status24=4`, `status25=2`.
- Therefore `0xDE6730` appears to update the AI roster/depth slot, not the
  player's real assignment.
- Disassembly of `0xA52950` shows the normal active-roster move signature is
  consistently `A52950(team, player, r8=1, r9=0)` in call-up style call sites.

Retired experiment:

- After a successful apply-rescue slot insert, we tested resolving the active
  parent team and calling `A52950(active_team, selected_player, 1, 0)`.
- Validation failed: in this context the call returned success but changed
  players such as `5381`/`5320` to `current=0`, `active=0`, `league=0`.
  The roster audit also logged `RELEASE_OBSERVED`, so this path behaves like
  removal/unassignment rather than a safe call-up.
- The active-move rescue branch and its opt-in flag were removed after the
  A49D80 team-add path proved to be the safe follow-up.

Next validation:

- Do not reintroduce `A52950(active_team, player, 1, 0)` as a rescue follow-up
  unless an isolated test save is prepared.

## 2026-05-12 EXE disassembly: roster assignment split

Direct disassembly of `ootp27.exe` clarified that the roster slot, team-array,
and player-assignment paths are separate:

- `0xDE6730` is still an AI/depth-slot writer. It writes slot entries at
  `slot_block + 0x14d4/0x14d8` and AI slot bytes such as `f61/f62/f1a`, but it
  does not write player assignment fields `0x48/0x58/0x60`.
- `0xA4EFF0` is a removal/unassignment path. Around `0xA4F2F0` it preserves
  old team/league values, then clears player `league/current/active`
  (`0x48`, `0x58`, `0x60`) to zero. Around `0xA4F576` it also clears
  `0x83d` and `0x890`.
- `0x7D78E0` is the compact player team setter:
  `set_player_team(player, team_id, parent_team_id, league_id, loan_flag)`.
  It writes `current_team=team_id`, `current_league=league_id`, and writes
  `active_team=parent_team_id` when the destination team has a parent, otherwise
  `active_team=team_id`.
- `0xA49D80` is the higher-level team-add path. It eventually calls
  `0x7D78E0` at `0xA4BB00` after updating team roster arrays and related
  player/team state. This is safer than calling `0x7D78E0` directly.
- `0xA544F0` contains a remove-then-add transfer sequence:
  it calls `0xA4EFF0` around `0xA54F9C`, then calls `0xA49D80` around
  `0xA54FC1` with destination team and player.
- The player-action UI area around `0x1075138` also calls `0xA49D80` with
  zeroed extra args, which makes `A49D80` the strongest next candidate for a
  controlled call-up/rescue experiment.

Current inference:

- The failed active-move experiment hit the removal side of the transaction
  machinery. The correct follow-up should not be another `A52950` call.
- The successful follow-up is the higher-level
  `A49D80(active_team, player, 0, 0, 0, 0, 0, 0)` path, not the compact setter
  or the active-move trampoline.

## 2026-05-12 follow-up: real assignment instrumentation and A49D80 experiment

The 5368 log narrowed the failure further:

- `5368` is not a release/zero-team case. The latest snapshot kept him at
  `current=18`, `active=9`, `league=101`.
- The AI did evaluate and select him: the traces show `selected=5368`,
  `f25=100`, sort rank `0`, and the custom foreign call-up policy allowed the
  move for team `9`.
- The previous rescue then skipped him as `not_clean_candidate` only because
  `status26=1`. This was too strict for the common minor foreign case.

Patch added during validation:

- Temporarily added read-only traces for `0x7D7870` and `0x7D78E0` to confirm
  that native team assignment was separated from the AI slot write. These
  traces were removed after the A49D80 validation succeeded.
- Added an explicit opt-in experiment:
  `enable_ai_roster_foreign_apply_rescue_team_add.txt`.
- When that flag is present, apply-rescue allows the observed
  `status26=1` minor foreign candidate, writes the AI slot, then calls the
  original `A49D80(active_team, player, 0, 0, 0, 0, 0, 0)` path.
- The previous dangerous `A52950` active-move attempt was removed.

Next validation:

- Do not hot-inject into an in-progress play session.
- For a clean test run, restart with the rebuilt DLL and enable only:
  `enable_foreign_ai_roster_research_hooks.txt` and
  `enable_ai_roster_foreign_apply_rescue_team_add.txt`.
- Advance one day and inspect:
  `ootp ai roster foreign apply rescue team-add` and the roster audit for
  players such as `5368`.

## 2026-05-12 source select rescue experiment

The `pid=34968` run showed that the assignment traces and A49D80 team-add
experiment were installed, but `5368` did not reach the apply-rescue path:

- `5368` repeatedly passed availability and custom call-up checks:
  `current=18`, `active=9`, `league=101`, `status26=1`, `f25=100`.
- In `ootp ai roster select source foreign summary #173/#175`, `5368` was the
  only foreign source candidate (`source_targets=1`) but the returned selection
  was domestic (`3576` / `5411`).
- No `ootp ai roster foreign apply rescue team-add` fired for `5368`, so the
  failure moved earlier than team-add: the native select return pointer still
  favored domestic fallback players.

Patch added:

- Added opt-in `enable_ai_roster_foreign_source_select_rescue.txt`.
- When the native DD86C0 select function returns a non-foreign player, the
  wrapper now scans the same source vector for the best KBO minor-league foreign
  candidate that passes the custom foreign call-up limit.
- The override is narrow: it requires `f25 >= 100`, `f65 == 0`,
  `status26 == 0 || status26 == 1`, a valid active parent team, and the
  existing `enable_ai_roster_foreign_apply_rescue_team_add.txt` experiment.
- New log:
  `ootp ai roster foreign source-select rescue`.

Next validation:

- Restart OOTP with the rebuilt DLL; do not hot-inject into the current session.
- Advance one day and inspect for:
  `ootp ai roster foreign source-select rescue`,
  `ootp ai roster foreign apply rescue team-add`, and the roster audit.
