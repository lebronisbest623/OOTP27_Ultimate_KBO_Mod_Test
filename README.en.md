# OOTP27 KBO Launcher

OOTP27 does not natively support the Korean Baseball Organization ruleset. This project fills that gap.

The launcher starts or attaches to an OOTP27 process, prepares KBO data files, and injects a native patch DLL written in C. The native layer hooks the running game binary and applies KBO-specific rules at runtime without access to OOTP source code.

Korean documentation: [`README.md`](README.md)

---

## Disclaimer

This is an unofficial fan-made launcher/mod for Out of the Park Baseball 27. It is not affiliated with, endorsed by, sponsored by, or approved by OOTP Developments, Com2uS, KBO, any KBO club, any players' association, or Steam.

This project injects a native DLL into a running OOTP27 process and patches game memory/runtime behavior. Because of that, OOTP updates, unsupported builds, local machine differences, antivirus/security policy, or unexpected save state may cause crashes, broken behavior, save corruption, or data loss.

Back up important saves before using this project. The launcher includes safety guards such as verified-build checks, roster marker checks, and disabled-by-default injection, but those safeguards cannot guarantee safety in every environment. Use this project at your own risk.

Before commercial use, redistribution, inclusion in another project, or public release packaging, verify the licenses for bundled fonts, images, seed data, OOTP-related files, and any other included assets.

---

## Implemented KBO Rules

### Foreign Player Reserve Rights

After the season, teams can retain reserve rights for eligible foreign players during a decision window. Only the team holding the right may re-sign that player. AI-controlled teams can make automatic retain/skip decisions.

### Military Service Loans

Players selected for military service are temporarily assigned to a service team. Service days are ticked down automatically, and players return to their original clubs when service is complete.

### Asian Quota

Asian-quota foreign players are tracked separately from regular foreign-player limits. Active roster and call-up limits are patched to follow KBO-style policy.

### Foreign Injury Replacement Slots

When a registered foreign player is injured, a temporary replacement slot can be opened. The slot closes when the original player returns.

### Competitive Balance Tax

Domestic payroll is measured from the opening-day salary snapshot, using each club's top configured domestic salaries. Clubs above the threshold receive tax records, league news, and draft-priority penalties after repeated violations.

### Asian Games National Team Events

The native runtime creates custom Asian Games events and handles selection, departure, replacement, return, and news generation. Roster selection balances club representation automatically.

### Single-Division All-Star Support

OOTP does not fully support All-Star games for a single-division league. This project patches the relevant runtime behavior so All-Star voting, selection, and games can proceed.

---

## Architecture

```text
KBOLauncher.exe          <- C# launcher
  |
  |- start OOTP27 or attach to an existing process
  |- prepare seed data under %LOCALAPPDATA%\OOTP-KBO\
  |- inject KBOFix.dll
       |
       |- memory hooks / runtime patches
          |- native/src/foreign/          foreign-player policy
          |  |- rights/                   reserve-right storage
          |  |- signability/              signing and offer blockers
          |  |- replacement_seed/         replacement-player seed parsing
          |  |- injury/                   injury replacement slots
          |  |- quota/                    Asian quota and call-up policy
          |  |- roster_audit/             read-only roster diagnostics
          |- native/src/competitive_balance_tax/
          |                                payroll cap records and draft penalties
          |- native/src/military_service/ military-service loans
          |  |- seed/                     player seed resolution
          |- native/src/custom_events/    Asian Games and priority events
          |- native/src/hotkey_window/    in-game F2 hub UI
```

The native layer is built as multiple translation units. `native/KBOFix.c` is a thin assembly shell, and `native/build.ps1` recursively compiles and links the `.c` modules under `native/src/`.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the detailed architecture notes.

---

## Requirements

- **OOTP27** (`ootp27.exe`, Steam layout supported)
- **.NET 8** for the launcher
- **GCC / MinGW-w64** for the native DLL
- **Microsoft WebView2 NuGet package** for the in-game F2 hub UI

---

## Build

```powershell
# Native DLL
powershell -ExecutionPolicy Bypass -File native\build.ps1

# C# launcher
dotnet build .\OOTP27-KBO-Launcher.sln
```

Build outputs:

- `native/bin/KBOFix.dll`
- `bin/Debug/net8.0/KBOLauncher.exe`

---

## Run

### Normal Startup

```bat
KBOLauncher.exe
```

By default, the launcher starts OOTP without injecting KBOFix. To enable launcher-controlled injection, set the JSON flag in:

```text
%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json
```

```json
{
  "enable_launcher_injection": true
}
```

### Explicit Injection

```powershell
# Start OOTP and inject the native patch DLL
KBOLauncher.exe --dll native\bin\KBOFix.dll

# Attach to an existing OOTP process
KBOLauncher.exe --dll native\bin\KBOFix.dll --attach-existing

# Use an explicit OOTP path
KBOLauncher.exe --ootp "D:\Steam\steamapps\common\Out of the Park Baseball 27\ootp27.exe" --dll native\bin\KBOFix.dll
```

---

## Safety Guards

KBOFix injection is intentionally fail-closed.

- Unknown or unreadable OOTP builds are not injected.
- Explicit injection requests fail visibly on unsupported builds.
- A current OOTP `.lg` save must be marked as an Ultimate KBO roster save.
- Default no-argument runs leave existing unsupported OOTP processes untouched.
- Native build verification remains as a second line of defense if the DLL is loaded by another path.

The roster marker is read from the current save's `description.txt` and must contain:

```text
https://github.com/lebronisbest623/OOTP27_Ultimate_KBO
```

---

## Runtime Data

| Path | Purpose |
|---|---|
| `%LOCALAPPDATA%\OOTP-KBO\` | Global settings, flags, seed files, diagnostics |
| `<save folder>/` | Save-scoped data such as reserve rights and military-service records |
| `data/seeds/` | Bundled seed CSV/JSON files copied into runtime data |

Save-scoped data is preferred when available. Global data is used as a fallback for bundled seeds and configuration.

---

## Key Runtime Flags

Runtime flags live in one JSON file:

```text
%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json
```

| Setting | Default | Description |
|---|---:|---|
| `enable_launcher_injection` | off | Enable automatic launcher injection |
| `enable_foreign_waiver_ai` | on | Enable AI reserve-right decisions |
| `enable_single_division_allstar_events` | off | Enable single-division All-Star support |
| `disable_kbo_competitive_balance_tax` | off | Disable CBT processing and draft penalties |
| `kbo_league_id.txt` | 100 | KBO league ID override file |

---

## Tests

```powershell
powershell -ExecutionPolicy Bypass -File tools\generate-runtime-flags.ps1
dotnet test .\OOTP27-KBO-Launcher.sln --no-restore
powershell -ExecutionPolicy Bypass -File native\tests\run_tests.ps1
```

---

## License

The source code in this project is distributed under the **Mozilla Public License 2.0 (MPL-2.0)**. See [`LICENSE`](LICENSE) for the full license text.

MPL-2.0 is a file-level weak copyleft license. If you distribute modified MPL-covered source files from this project, you must make those modified files available under the same license terms. Separate files in a larger work do not automatically need to be relicensed under MPL-2.0.

Bundled fonts, images, seed data, OOTP/KBO names, and other third-party assets remain subject to their own licenses and rights. This project's license does not grant additional rights to third-party assets.
