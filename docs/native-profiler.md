# Native DLL Profiler

The KBOFix DLL has an opt-in aggregated profiler for game-side runtime hooks.
It is designed for simulation slowdowns where per-call logging would make the
problem worse.

## Enable

Edit `%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json` and add:

```json
{
  "enable_kbo_profiler": true
}
```

Restart the game after changing the flag. The flag is cached inside the DLL for
the current process.

## Output

Profiler output is written once per aggregation window to:

```text
%LOCALAPPDATA%\OOTP-KBO\perf\kbo_perf_<pid>.csv
```

Each row is one profiler zone over the last flush window:

- `zone`: measured hook or subsystem path
- `delta_calls`: calls during the window
- `delta_us`: total measured microseconds during the window
- `avg_us`: average microseconds per call during the window
- `max_us`: worst call observed during the window
- `delta_slow_calls`: calls at or above 1000 microseconds

The profiler also measures `log.append_line`, so log IO itself can be separated
from game logic cost.

## Summarize

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File native\tools\analyze_perf.ps1
```

Or point it at a specific CSV:

```powershell
powershell -ExecutionPolicy Bypass -File native\tools\analyze_perf.ps1 -Path "$env:LOCALAPPDATA\OOTP-KBO\perf\kbo_perf_1234.csv" -Top 40
```

The script prints the heaviest zones by total time, call count, and max latency.
