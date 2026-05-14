using System.Diagnostics;
using Polly;
using Polly.Retry;
using Spectre.Console;
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

        AnsiConsole.MarkupLineInterpolated($"[green]Launched OOTP[/] pid={launched.Id}");
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
        var markerWaitDeadline = DateTimeOffset.Now + MarkedSaveWaitTimeout;
        var injectionTarget = WaitForStableOotpProcess(
            launched,
            exePath,
            launchStarted,
            TimeSpan.FromSeconds(30),
            logPath,
            earlyInjector.TryInject);

        while (true)
        {
            if (!launchPlan.InjectionDecision.AllowsPresaveInjection)
            {
                var rosterMarkerInfo = WaitForRosterMarkerForInjectionTarget(
                    injectionTarget.Id,
                    launchStarted,
                    markerWaitDeadline,
                    logPath);
                if (!rosterMarkerInfo.Ok)
                {
                    if (TryResolveReplacementInjectionTarget(
                            rosterMarkerInfo,
                            injectionTarget.Id,
                            launched,
                            exePath,
                            launchStarted,
                            markerWaitDeadline,
                            logPath,
                            earlyInjector.TryInject,
                            out var replacementTarget))
                    {
                        injectionTarget = replacementTarget;
                        continue;
                    }

                    PrintMissingRosterMarkerWarning(rosterMarkerInfo);
                    Log(logPath, $"inject_blocked pid={injectionTarget.Id} reason=missing_roster_marker {KboRosterMarkerGuard.FormatLogStatus(rosterMarkerInfo)}");
                    return 9;
                }
            }

            var injectReason = launchPlan.InjectionDecision.AllowsPresaveInjection
                ? "presave_allstar_bootstrap"
                : "marked_save_runtime";
            InjectIfNeeded(injectionTarget.Id, options.DllPath!, earlyInjector.PreparedDllPath, logPath, injectReason);

            if (!launchPlan.InjectionDecision.AllowsPresaveInjection)
            {
                return 0;
            }

            var postInjectRosterMarkerInfo = WaitForRosterMarkerForInjectionTarget(
                injectionTarget.Id,
                launchStarted,
                markerWaitDeadline,
                logPath);
            if (postInjectRosterMarkerInfo.Ok)
            {
                return 0;
            }

            if (TryResolveReplacementInjectionTarget(
                    postInjectRosterMarkerInfo,
                    injectionTarget.Id,
                    launched,
                    exePath,
                    launchStarted,
                    markerWaitDeadline,
                    logPath,
                    earlyInjector.TryInject,
                    out var postInjectReplacementTarget))
            {
                injectionTarget = postInjectReplacementTarget;
                continue;
            }

            PrintMissingRosterMarkerWarning(postInjectRosterMarkerInfo);
            Log(logPath, $"inject_blocked pid={injectionTarget.Id} reason=missing_roster_marker {KboRosterMarkerGuard.FormatLogStatus(postInjectRosterMarkerInfo)}");
            return 9;
        }
    }

    private static RosterMarkerInfo WaitForRosterMarkerForInjectionTarget(
        int pid,
        DateTimeOffset launchStarted,
        DateTimeOffset deadline,
        string logPath)
    {
        var timeout = deadline - DateTimeOffset.Now;
        if (timeout < TimeSpan.Zero)
        {
            timeout = TimeSpan.Zero;
        }

        return WaitForMarkedCurrentSave(
            pid,
            timeout,
            MarkedSavePollInterval,
            logPath,
            minSaveCompletedAt: null,
            fallbackMinSaveCompletedAt: launchStarted.ToUniversalTime(),
            allowIncompleteMarkedSave: true);
    }

    private static bool TryResolveReplacementInjectionTarget(
        RosterMarkerInfo rosterMarkerInfo,
        int oldPid,
        Process launched,
        string exePath,
        DateTimeOffset launchStarted,
        DateTimeOffset markerWaitDeadline,
        string logPath,
        Action<Process>? onCandidate,
        out Process replacementTarget)
    {
        replacementTarget = null!;
        if (rosterMarkerInfo.Status != "process_unavailable")
        {
            return false;
        }

        var remaining = markerWaitDeadline - DateTimeOffset.Now;
        if (remaining <= TimeSpan.Zero)
        {
            return false;
        }

        Log(logPath, $"stable_injection_target_lost old_pid={oldPid} reason=process_unavailable");
        AnsiConsole.MarkupLine("[yellow]OOTP process exited before injection; looking for a replacement OOTP process...[/]");

        try
        {
            var resolveTimeout = remaining < TimeSpan.FromSeconds(30)
                ? remaining
                : TimeSpan.FromSeconds(30);
            replacementTarget = WaitForStableOotpProcess(
                launched,
                exePath,
                launchStarted,
                resolveTimeout,
                logPath,
                onCandidate);
            Log(logPath, $"stable_injection_target_replaced new_pid={replacementTarget.Id}");
            return true;
        }
        catch (TimeoutException ex)
        {
            Log(logPath, $"stable_injection_target_replacement_timeout error=\"{ex.Message.Replace("\"", "'")}\"");
            return false;
        }
    }

    private static void InjectIfNeeded(int pid, string dllPath, string? preparedDllPath, string logPath, string reason)
    {
        if (IsKboFixAlreadyLoaded(pid, logPath))
        {
            AnsiConsole.MarkupLineInterpolated($"[yellow]KBOFix is already loaded in pid={pid}; skipping injection.[/]");
            WarnIfLoadedKboFixLooksStale(pid, dllPath, logPath);
            Log(logPath, $"inject_skipped pid={pid} reason=already_loaded");
            return;
        }

        var injectableDllPath = preparedDllPath ?? PrepareInjectableDllCopy(dllPath, logPath);
        InjectDllWithShortRetry(pid, injectableDllPath, logPath);
        Log(logPath, $"inject_complete pid={pid} reason={reason}");
    }

    private static readonly ResiliencePipeline InjectRetry = new ResiliencePipelineBuilder()
        .AddRetry(new RetryStrategyOptions
        {
            ShouldHandle = new PredicateBuilder()
                .Handle<System.ComponentModel.Win32Exception>()
                .Handle<InvalidOperationException>()
                .Handle<TimeoutException>(),
            MaxRetryAttempts = 4,
            Delay = TimeSpan.FromMilliseconds(500),
            BackoffType = DelayBackoffType.Constant,
        })
        .Build();

    private static void InjectDllWithShortRetry(int pid, string dllPath, string logPath)
    {
        var attempt = 0;
        InjectRetry.Execute(() =>
        {
            attempt++;
            try
            {
                InjectDll(pid, dllPath, logPath);
            }
            catch (Exception ex) when (ex is System.ComponentModel.Win32Exception or InvalidOperationException or TimeoutException)
            {
                Log(logPath, $"inject_retry_wait pid={pid} attempt={attempt} error=\"{ex.Message.Replace("\"", "'")}\"");
                throw;
            }
        });
        if (attempt > 1)
        {
            Log(logPath, $"inject_retry_succeeded pid={pid} attempt={attempt}");
        }
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
                    WarnIfLoadedKboFixLooksStale(candidate.Id, dllPath, logPath);
                    Log(logPath, $"early_inject_skipped pid={candidate.Id} reason=already_loaded");
                    return;
                }

                PreparedDllPath ??= PrepareInjectableDllCopy(dllPath, logPath);
                InjectDllWithShortRetry(candidate.Id, PreparedDllPath, logPath);
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
