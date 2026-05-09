using System.Diagnostics;
using static DllInjector;
using static DllPayloadStager;
using static InjectedModuleDetector;
using static InjectionRosterMarkerWaiter;
using static InjectionTargetResolver;
using static LauncherGuardStatus;
using static LauncherLog;

internal static partial class LauncherApp
{
    private static int RunLaunchFlow(
        LauncherOptions options,
        string exePath,
        IReadOnlyList<ProcessInfo> existing,
        string logPath,
        LaunchPlan launchPlan)
    {
        if (options.DryRun)
        {
            Console.WriteLine("Dry-run only. No process launched.");
            Log(logPath, "dry_run=true");
            return 0;
        }

        if (existing.Count > 0 && !options.AllowSecondInstance)
        {
            Console.Error.WriteLine("OOTP already appears to be running. Use --allow-second-instance to launch anyway.");
            Log(logPath, "blocked=already_running");
            return 3;
        }

        var launchStarted = DateTimeOffset.Now;
        var launched = StartOotpProcess(exePath, options, logPath);
        if (launched is null)
        {
            return 4;
        }

        if (options.DllPath is null)
        {
            return 0;
        }

        return InjectLaunchedProcess(options, exePath, logPath, launchStarted, launched, launchPlan);
    }

    private static Process? StartOotpProcess(string exePath, LauncherOptions options, string logPath)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = exePath,
            WorkingDirectory = Path.GetDirectoryName(exePath)!,
            UseShellExecute = false,
        };

        foreach (var arg in options.OotpArgs)
        {
            startInfo.ArgumentList.Add(arg);
        }

        var launched = Process.Start(startInfo);
        if (launched is null)
        {
            Console.Error.WriteLine("Process.Start returned null.");
            Log(logPath, "launch_failed=null_process");
            return null;
        }

        Console.WriteLine($"Launched OOTP pid={launched.Id}");
        Log(logPath, $"launched pid={launched.Id}");
        return launched;
    }

    private static int InjectLaunchedProcess(
        LauncherOptions options,
        string exePath,
        string logPath,
        DateTimeOffset launchStarted,
        Process launched,
        LaunchPlan launchPlan)
    {
        var earlyInjector = new PresaveEarlyInjector(
            options.DllPath!,
            logPath,
            launchPlan.AllstarBootstrapRequested);
        var injectionTarget = WaitForStableOotpProcess(
            launched,
            exePath,
            launchStarted,
            TimeSpan.FromSeconds(30),
            logPath,
            earlyInjector.TryInject);

        if (!launchPlan.InjectionDecision.AllowsPresaveInjection
            && !WaitForRosterMarkerBeforeInjection(injectionTarget.Id, launchStarted, logPath))
        {
            return 9;
        }

        InjectIfNeeded(injectionTarget.Id, options.DllPath!, earlyInjector.PreparedDllPath, logPath);

        if (launchPlan.InjectionDecision.AllowsPresaveInjection
            && !WaitForRosterMarkerBeforeInjection(injectionTarget.Id, launchStarted, logPath))
        {
            return 9;
        }

        return 0;
    }

    private static bool WaitForRosterMarkerBeforeInjection(int pid, DateTimeOffset launchStarted, string logPath)
    {
        var rosterMarkerInfo = WaitForMarkedCurrentSave(
            pid,
            MarkedSaveWaitTimeout,
            MarkedSavePollInterval,
            logPath,
            launchStarted.ToUniversalTime());
        if (rosterMarkerInfo.Ok)
        {
            return true;
        }

        PrintMissingRosterMarkerWarning(rosterMarkerInfo);
        Log(logPath, $"inject_blocked pid={pid} reason=missing_roster_marker {KboRosterMarkerGuard.FormatLogStatus(rosterMarkerInfo)}");
        return false;
    }

    private static void InjectIfNeeded(int pid, string dllPath, string? preparedDllPath, string logPath)
    {
        if (IsKboFixAlreadyLoaded(pid, logPath))
        {
            Console.WriteLine($"KBOFix is already loaded in pid={pid}; skipping injection.");
            Log(logPath, $"inject_skipped pid={pid} reason=already_loaded");
            return;
        }

        var injectableDllPath = preparedDllPath ?? PrepareInjectableDllCopy(dllPath, logPath);
        InjectDll(pid, injectableDllPath, logPath);
        Log(logPath, $"inject_complete pid={pid} reason=marked_save_runtime");
    }

    private sealed class PresaveEarlyInjector
    {
        private readonly string dllPath;
        private readonly string logPath;
        private readonly bool allstarBootstrapRequested;
        private readonly HashSet<int> injectedPids = new();

        public PresaveEarlyInjector(string dllPath, string logPath, bool allstarBootstrapRequested)
        {
            this.dllPath = dllPath;
            this.logPath = logPath;
            this.allstarBootstrapRequested = allstarBootstrapRequested;
        }

        public string? PreparedDllPath { get; private set; }

        public void TryInject(Process candidate)
        {
            try
            {
                if (!allstarBootstrapRequested || candidate.HasExited || injectedPids.Contains(candidate.Id))
                {
                    return;
                }

                if (IsKboFixAlreadyLoaded(candidate.Id, logPath))
                {
                    Log(logPath, $"early_inject_skipped pid={candidate.Id} reason=already_loaded");
                    return;
                }

                PreparedDllPath ??= PrepareInjectableDllCopy(dllPath, logPath);
                InjectDll(candidate.Id, PreparedDllPath, logPath);
                injectedPids.Add(candidate.Id);
                Log(logPath, $"early_inject pid={candidate.Id} reason=presave_allstar_candidate");
            }
            catch (Exception ex)
            {
                var pid = 0;
                try
                {
                    pid = candidate.Id;
                }
                catch
                {
                }
                Log(logPath, $"early_inject_candidate_failed pid={pid} error=\"{ex.Message.Replace("\"", "'")}\"");
            }
        }
    }
}
