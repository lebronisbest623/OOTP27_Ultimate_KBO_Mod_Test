using static DllInjector;
using static DllPayloadStager;
using static InjectedModuleDetector;
using static InjectionRosterMarkerWaiter;
using static LauncherGuardStatus;
using static LauncherLog;
using static ProcessDiscovery;

internal static partial class LauncherApp
{
    private static int RunAttachFlow(
        LauncherOptions options,
        bool isDefaultRun,
        IReadOnlyList<ProcessInfo> existing,
        string logPath)
    {
        if (options.DllPath is null)
        {
            Console.Error.WriteLine("--dll is required when using --attach-pid or --attach-existing.");
            return 5;
        }

        var targetPid = options.AttachPid ?? ResolveSingleExistingProcess(existing);
        if (targetPid is null)
        {
            Console.Error.WriteLine("Could not resolve a single existing OOTP process.");
            return 6;
        }

        var rosterMarkerInfo = isDefaultRun
            ? WaitForMarkedCurrentSave(
                targetPid.Value,
                MarkedSaveWaitTimeout,
                MarkedSavePollInterval,
                logPath)
            : CheckRosterMarkerForInjectionTarget(targetPid.Value, logPath);
        if (!rosterMarkerInfo.Ok)
        {
            PrintMissingRosterMarkerWarning(rosterMarkerInfo);
            Log(logPath, $"inject_blocked pid={targetPid.Value} reason=missing_roster_marker {KboRosterMarkerGuard.FormatLogStatus(rosterMarkerInfo)}");
            if (options.DryRun)
            {
                Console.WriteLine("Dry-run: KBOFix injection would be blocked until the currently opened save description contains the roster marker.");
                return 0;
            }
            return 9;
        }

        if (options.DryRun)
        {
            Console.WriteLine($"Dry-run attach target pid={targetPid}");
            Console.WriteLine($"DLL: {Path.GetFullPath(options.DllPath)}");
            Log(logPath, $"dry_run_attach pid={targetPid} dll={Path.GetFullPath(options.DllPath)}");
            return 0;
        }

        if (IsKboFixAlreadyLoaded(targetPid.Value, logPath))
        {
            Console.WriteLine($"KBOFix is already loaded in pid={targetPid.Value}; skipping injection.");
            Log(logPath, $"inject_skipped pid={targetPid.Value} reason=already_loaded");
            return 0;
        }

        var injectableDllPath = PrepareInjectableDllCopy(options.DllPath, logPath);
        InjectDll(targetPid.Value, injectableDllPath, logPath);
        return 0;
    }
}
