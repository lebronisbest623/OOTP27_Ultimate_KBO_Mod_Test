using System.Diagnostics;
using static InjectionTargetResolver;
using static InjectedModuleDetector;
using static DllPayloadStager;
using static DllInjector;
using static KboFlags;
using static KboSeedFiles;
using static LauncherGuardStatus;
using static LauncherLog;
using static LauncherPaths;
using static ProcessDiscovery;

internal static class LauncherApp
{
    public static int Run(string[] args)
    {
        
        LauncherOptions options;
        try
        {
            options = LauncherOptions.Parse(args);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            LauncherOptions.PrintHelp();
            return 1;
        }
        
        if (options.ShowHelp)
        {
            LauncherOptions.PrintHelp();
            return 0;
        }
        
        var exePath = ResolveOotpPath(options.OotpPath);
        if (exePath is null)
        {
            Console.Error.WriteLine("Could not find ootp27.exe. Pass --ootp \"C:\\path\\to\\ootp27.exe\".");
            return 2;
        }
        
        EnsureKboLeagueIdConfig();
        EnsureBundledKboDataFile("foreign_injury_replacements_seed.csv", "Foreign injury replacement seed");
        EnsureBundledKboDataFile("foreign_replacement_players_seed.csv", "Foreign replacement player seed");
        EnsureBundledKboDataFile("military_service_seed.csv", "Military service seed");
        
        var existing = Process.GetProcesses()
            .Select(TryDescribe)
            .Where(p => p is not null)
            .Cast<ProcessInfo>()
            .Where(p => PathEquals(p.Path, exePath))
            .ToList();
        
        if (args.Length == 0)
        {
            var enableDefaultInjection = ReadKboFlag("enable_launcher_injection.txt");
            string? defaultDll = null;
            if (enableDefaultInjection)
            {
                defaultDll = ResolveDefaultKboFixDllPath();
                if (defaultDll is null)
                {
                    Console.Error.WriteLine("Could not find KBOFix.dll next to the launcher.");
                    return 7;
                }
            }
        
            options = options with
            {
                DllPath = defaultDll,
                EnableMilitaryDraftPool = true,
                EnableForeignWaiverAi = true,
                EnableSingleDivisionAllstarEvents = true,
                AttachExisting = existing.Count > 0,
            };
        }
        
        var logPath = GetLogPath();
        Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);
        
        Log(logPath, $"resolved_ootp={exePath}");
        if (args.Length == 0 && options.DllPath is null)
        {
            Log(logPath, "default_injection=disabled reason=missing_enable_launcher_injection_json_flag");
        }
        Console.WriteLine($"OOTP: {exePath}");
        Console.WriteLine($"WorkDir: {Path.GetDirectoryName(exePath)}");
        Console.WriteLine($"Log: {logPath}");
        if (args.Length == 0 && options.DllPath is null)
        {
            Console.WriteLine("KBOFix injection is disabled for safe startup.");
            Console.WriteLine($"To re-enable launcher injection, set enable_launcher_injection=true in: {GetKboFlagConfigPath()}");
        }
        
        var ootpBuildInfo = OotpBuildGuard.Read(exePath);
        var supportedOotpBuild = OotpBuildGuard.FindSupportedBuild(ootpBuildInfo);
        Console.WriteLine(OotpBuildGuard.FormatConsoleStatus(ootpBuildInfo, supportedOotpBuild));
        Log(logPath, OotpBuildGuard.FormatLogStatus(ootpBuildInfo, supportedOotpBuild));
        WriteLauncherBuildGuardStatus(ootpBuildInfo, supportedOotpBuild, options.DllPath is not null);
        
        if (options.DllPath is not null && supportedOotpBuild is null)
        {
            PrintUnsupportedBuildInjectionWarning(ootpBuildInfo);
            Log(logPath, $"inject_blocked reason=unsupported_ootp_build {OotpBuildGuard.FormatLogStatus(ootpBuildInfo, null)}");
        
            if (options.DryRun)
            {
                Console.WriteLine("Dry-run: KBOFix injection would be blocked for this OOTP build.");
            }
            else if (args.Length == 0)
            {
                options = options with { DllPath = null, AttachExisting = false };
                if (existing.Count > 0)
                {
                    Console.WriteLine("Existing OOTP process left unmodified because this build is not supported.");
                    return 0;
                }
            }
            else
            {
                return 8;
            }
        }
        
        if (existing.Count > 0)
        {
            Console.WriteLine("Existing OOTP process:");
            foreach (var process in existing)
            {
                Console.WriteLine($"  pid={process.Id} name={process.Name} path={process.Path}");
                Log(logPath, $"existing pid={process.Id} name={process.Name} path={process.Path}");
            }
        }
        
        if (options.EnableMilitaryDraftPool is not null)
        {
            WriteKboMilitaryDraftPoolFlag(options.EnableMilitaryDraftPool.Value);
        }
        if (options.EnableForeignWaiverAi is not null)
        {
            WriteKboForeignWaiverAiFlag(options.EnableForeignWaiverAi.Value);
        }
        if (options.EnableSingleDivisionAllstarEvents is not null)
        {
            WriteKboSingleDivisionAllstarEventsFlag(options.EnableSingleDivisionAllstarEvents.Value);
        }
        
        if (options.AttachPid is not null || options.AttachExisting)
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
        
            var rosterMarkerInfo = CheckRosterMarkerForInjectionTarget(targetPid.Value, logPath);
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
        
        var launchStarted = DateTimeOffset.Now;
        var launched = Process.Start(startInfo);
        if (launched is null)
        {
            Console.Error.WriteLine("Process.Start returned null.");
            Log(logPath, "launch_failed=null_process");
            return 4;
        }
        
        Console.WriteLine($"Launched OOTP pid={launched.Id}");
        Log(logPath, $"launched pid={launched.Id}");
        
        if (options.DllPath is not null)
        {
            var injectionTarget = WaitForStableOotpProcess(launched, exePath, launchStarted, TimeSpan.FromSeconds(30), logPath);
            var rosterMarkerInfo = CheckRosterMarkerForInjectionTarget(injectionTarget.Id, logPath);
            if (!rosterMarkerInfo.Ok)
            {
                PrintMissingRosterMarkerWarning(rosterMarkerInfo);
                Log(logPath, $"inject_blocked pid={injectionTarget.Id} reason=missing_roster_marker {KboRosterMarkerGuard.FormatLogStatus(rosterMarkerInfo)}");
                return 9;
            }
        
            if (IsKboFixAlreadyLoaded(injectionTarget.Id, logPath))
            {
                Console.WriteLine($"KBOFix is already loaded in pid={injectionTarget.Id}; skipping injection.");
                Log(logPath, $"inject_skipped pid={injectionTarget.Id} reason=already_loaded");
                return 0;
            }
        
            var injectableDllPath = PrepareInjectableDllCopy(options.DllPath, logPath);
            InjectDll(injectionTarget.Id, injectableDllPath, logPath);
        }

        return 0;
        
    }
}
