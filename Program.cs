using System.Diagnostics;
using System.ComponentModel;
using System.Runtime.InteropServices;

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
EnsureBundledTesseractConfig(logPath);

Log(logPath, $"resolved_ootp={exePath}");
if (args.Length == 0 && options.DllPath is null)
{
    Log(logPath, "default_injection=disabled reason=missing_enable_launcher_injection_flag");
}
Console.WriteLine($"OOTP: {exePath}");
Console.WriteLine($"WorkDir: {Path.GetDirectoryName(exePath)}");
Console.WriteLine($"Log: {logPath}");
if (args.Length == 0 && options.DllPath is null)
{
    Console.WriteLine("KBOFix injection is disabled for safe startup.");
    Console.WriteLine($"To re-enable launcher injection, create: {GetKboFlagPath("enable_launcher_injection.txt")}");
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

static string? ResolveOotpPath(string? explicitPath)
{
    var candidates = new List<string>();
    if (!string.IsNullOrWhiteSpace(explicitPath))
    {
        candidates.Add(explicitPath);
    }

    candidates.Add(@"C:\Program Files (x86)\Steam\steamapps\common\Out of the Park Baseball 27\ootp27.exe");
    candidates.Add(@"C:\Program Files\Steam\steamapps\common\Out of the Park Baseball 27\ootp27.exe");

    return candidates
        .Where(File.Exists)
        .OrderByDescending(File.GetLastWriteTimeUtc)
        .FirstOrDefault();
}

static string? ResolveDefaultKboFixDllPath()
{
    var baseDir = AppContext.BaseDirectory;
    var candidates = new[]
    {
        Path.Combine(baseDir, "KBOFix.dll"),
        Path.GetFullPath(Path.Combine(baseDir, "..", "..", "..", "native", "bin", "KBOFix.dll")),
    };

    return candidates
        .Where(File.Exists)
        .FirstOrDefault();
}

static ProcessInfo? TryDescribe(Process process)
{
    try
    {
        return new ProcessInfo(process.Id, process.ProcessName, process.MainModule?.FileName ?? "");
    }
    catch
    {
        return new ProcessInfo(process.Id, process.ProcessName, "");
    }
}

static string GetLogPath()
{
    var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    return Path.Combine(local, "OOTP-KBO", "launcher.log");
}

static string GetKboFlagPath(string fileName)
{
    var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    return Path.Combine(local, "OOTP-KBO", fileName);
}

static void WriteKboFlag(string fileName, string label, bool enabled)
{
    var path = GetKboFlagPath(fileName);
    Directory.CreateDirectory(Path.GetDirectoryName(path)!);
    File.WriteAllText(path, enabled ? "1\n" : "0\n");
    Console.WriteLine($"{label}: {path} = {(enabled ? "enabled" : "disabled")}");
}

static bool ReadKboFlag(string fileName)
{
    var path = GetKboFlagPath(fileName);
    if (!File.Exists(path))
    {
        return false;
    }

    var text = File.ReadAllText(path).Trim();
    return text.Equals("1", StringComparison.OrdinalIgnoreCase)
        || text.Equals("true", StringComparison.OrdinalIgnoreCase)
        || text.Equals("yes", StringComparison.OrdinalIgnoreCase)
        || text.Equals("on", StringComparison.OrdinalIgnoreCase)
        || text.Equals("enabled", StringComparison.OrdinalIgnoreCase);
}

static void WriteKboMilitaryDraftPoolFlag(bool enabled)
{
    WriteKboFlag("enable_military_draft_pool.txt", "Military draft pool flag", enabled);
}

static void WriteKboForeignWaiverAiFlag(bool enabled)
{
    WriteKboFlag("enable_foreign_waiver_ai.txt", "Foreign waiver AI flag", enabled);
}

static void WriteKboSingleDivisionAllstarEventsFlag(bool enabled)
{
    WriteKboFlag("enable_single_division_allstar_events.txt", "Single-division all-star events flag", enabled);
}

static void WriteLauncherBuildGuardStatus(OotpBuildInfo info, OotpSupportedBuild? supportedBuild, bool injectionRequested)
{
    var path = GetKboFlagPath("launcher_build_guard_status.txt");
    Directory.CreateDirectory(Path.GetDirectoryName(path)!);

    var lines = new List<string>
    {
        $"checked_at={DateTimeOffset.Now:O}",
        $"injection_requested={(injectionRequested ? "1" : "0")}",
        $"build_read={(info.Ok ? "1" : "0")}",
    };

    if (info.Ok)
    {
        lines.Add($"timestamp=0x{info.Timestamp:X8}");
        lines.Add($"size_of_image=0x{info.SizeOfImage:X8}");
    }
    else
    {
        lines.Add($"error={info.Error}");
    }

    if (supportedBuild is null)
    {
        lines.Add("status=unsupported");
        lines.Add("kbo_injection=disabled");
    }
    else
    {
        lines.Add("status=supported");
        lines.Add($"label={supportedBuild.Label}");
        lines.Add(injectionRequested ? "kbo_injection=allowed" : "kbo_injection=not_requested");
    }

    lines.Add("supported_builds=" + string.Join(", ", OotpBuildGuard.SupportedBuildDescriptions()));
    File.WriteAllLines(path, lines);
}

static void PrintUnsupportedBuildInjectionWarning(OotpBuildInfo info)
{
    Console.Error.WriteLine("KBOFix injection disabled: this OOTP build is not verified.");
    Console.Error.WriteLine("This prevents silent memory-offset mismatches after an OOTP patch.");
    if (info.Ok)
    {
        Console.Error.WriteLine($"Detected build: timestamp=0x{info.Timestamp:X8}, size_of_image=0x{info.SizeOfImage:X8}");
    }
    else
    {
        Console.Error.WriteLine($"Could not read OOTP build info: {info.Error}");
    }
    Console.Error.WriteLine("Update the launcher/native supported-build list after verifying this OOTP version.");
}

static RosterMarkerInfo CheckRosterMarkerForInjectionTarget(int pid, string logPath)
{
    var info = KboRosterMarkerGuard.CheckCurrentSave(pid, message => Log(logPath, message));
    WriteCurrentSavePathCache(pid, info.Ok ? info.SavePath : null, logPath);
    Console.WriteLine(KboRosterMarkerGuard.FormatConsoleStatus(info));
    Log(logPath, KboRosterMarkerGuard.FormatLogStatus(info));
    WriteRosterMarkerGuardStatus(info);
    return info;
}

static void WriteCurrentSavePathCache(int pid, string? savePath, string logPath)
{
    var path = GetKboFlagPath($"current_save_path_{pid}.txt");
    try
    {
        if (string.IsNullOrWhiteSpace(savePath))
        {
            if (File.Exists(path))
            {
                File.Delete(path);
            }
            return;
        }

        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, Path.GetFullPath(savePath));
    }
    catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
    {
        Log(logPath, $"current_save_cache_write_failed path=\"{path}\" error=\"{ex.Message}\"");
    }
}

static void WriteRosterMarkerGuardStatus(RosterMarkerInfo info)
{
    var path = GetKboFlagPath("launcher_roster_marker_guard_status.txt");
    Directory.CreateDirectory(Path.GetDirectoryName(path)!);

    var lines = new List<string>
    {
        $"checked_at={DateTimeOffset.Now:O}",
        $"status={info.Status}",
        $"kbo_injection={(info.Ok ? "allowed" : "disabled")}",
        $"required_marker={KboRosterMarkerGuard.RequiredMarkerUrl}",
    };

    if (!string.IsNullOrWhiteSpace(info.SavePath))
    {
        lines.Add($"save_path={info.SavePath}");
    }
    if (!string.IsNullOrWhiteSpace(info.DescriptionPath))
    {
        lines.Add($"description_path={info.DescriptionPath}");
    }
    if (!string.IsNullOrWhiteSpace(info.Error))
    {
        lines.Add($"error={info.Error}");
    }

    File.WriteAllLines(path, lines);
}

static void PrintMissingRosterMarkerWarning(RosterMarkerInfo info)
{
    Console.Error.WriteLine("KBOFix injection disabled: the currently opened OOTP save is not marked for this KBO roster.");
    Console.Error.WriteLine($"Required marker in description.txt: {KboRosterMarkerGuard.RequiredMarkerUrl}");
    if (!string.IsNullOrWhiteSpace(info.SavePath))
    {
        Console.Error.WriteLine($"Current save checked: {info.SavePath}");
    }
    if (!string.IsNullOrWhiteSpace(info.DescriptionPath))
    {
        Console.Error.WriteLine($"Description checked: {info.DescriptionPath}");
    }
    if (!string.IsNullOrWhiteSpace(info.Error))
    {
        Console.Error.WriteLine($"Reason: {info.Error}");
    }
}

static void EnsureKboLeagueIdConfig()
{
    var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    var localDir = Path.Combine(local, "OOTP-KBO");
    var localPath = Path.Combine(localDir, "kbo_league_id.txt");

    if (File.Exists(localPath)) {
        return;
    }

    Directory.CreateDirectory(localDir);

    var defaultCandidates = new[]
    {
        Path.Combine(AppContext.BaseDirectory, "kbo_league_id.txt"),
        Path.Combine(Environment.CurrentDirectory, "kbo_league_id.txt"),
        Path.Combine(AppContext.BaseDirectory, "native", "kbo_league_id.txt")
    };

    foreach (var candidate in defaultCandidates)
    {
        if (!File.Exists(candidate))
        {
            continue;
        }

        try
        {
            File.Copy(candidate, localPath, overwrite: true);
            return;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to seed kbo_league_id.txt from {candidate}: {ex.Message}");
            return;
        }
    }

    Console.WriteLine("kbo_league_id.txt not found in launcher directory. Set it manually at:");
    Console.WriteLine(localPath);
}

static void EnsureBundledKboDataFile(string fileName, string label)
{
    var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    var localDir = Path.Combine(local, "OOTP-KBO");
    var localPath = Path.Combine(localDir, fileName);

    Directory.CreateDirectory(localDir);

    var candidates = new[]
    {
        Path.Combine(AppContext.BaseDirectory, "data", "seeds", fileName),
        Path.Combine(Environment.CurrentDirectory, "data", "seeds", fileName),
        Path.Combine(AppContext.BaseDirectory, fileName),
        Path.Combine(Environment.CurrentDirectory, fileName),
        Path.Combine(AppContext.BaseDirectory, "native", fileName)
    };

    foreach (var candidate in candidates)
    {
        if (!File.Exists(candidate))
        {
            continue;
        }

        try
        {
            var shouldCopy = !File.Exists(localPath)
                || File.GetLastWriteTimeUtc(candidate) > File.GetLastWriteTimeUtc(localPath)
                || new FileInfo(candidate).Length != new FileInfo(localPath).Length;
            if (shouldCopy)
            {
                File.Copy(candidate, localPath, overwrite: true);
                Console.WriteLine($"{label}: seeded {localPath}");
            }
            return;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to seed {fileName} from {candidate}: {ex.Message}");
            return;
        }
    }
}

static void EnsureBundledTesseractConfig(string logPath)
{
    var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    var localDir = Path.Combine(local, "OOTP-KBO");
    var configPath = Path.Combine(localDir, "tesseract_path.txt");

    if (File.Exists(configPath))
    {
        var configuredPath = File.ReadAllText(configPath).Trim();
        if (!string.IsNullOrWhiteSpace(configuredPath) && File.Exists(configuredPath))
        {
            Log(logPath, $"tesseract_path_config keep_existing path={configuredPath}");
            return;
        }
    }

    var bundledPath = ResolveBundledTesseractPath();
    if (bundledPath is null)
    {
        Log(logPath, "tesseract_path_config skipped reason=no_bundled_tesseract");
        return;
    }

    Directory.CreateDirectory(localDir);
    File.WriteAllText(configPath, bundledPath + Environment.NewLine);
    Console.WriteLine($"Bundled OCR: {bundledPath}");
    Log(logPath, $"tesseract_path_config source=bundled path={bundledPath}");
}

static string? ResolveBundledTesseractPath()
{
    var baseDir = AppContext.BaseDirectory;
    var candidates = new[]
    {
        Path.Combine(baseDir, "ocr", "tesseract.exe"),
        Path.Combine(Environment.CurrentDirectory, "ocr", "tesseract.exe"),
        Path.Combine(baseDir, "tesseract", "tesseract.exe"),
        Path.GetFullPath(Path.Combine(baseDir, "..", "..", "..", "ocr", "tesseract.exe")),
    };

    return candidates
        .Where(File.Exists)
        .Select(Path.GetFullPath)
        .FirstOrDefault();
}

static void Log(string path, string message)
{
    File.AppendAllText(path, $"[{DateTimeOffset.Now:O}] {message}{Environment.NewLine}");
}

static int? ResolveSingleExistingProcess(IReadOnlyList<ProcessInfo> processes)
{
    return processes.Count == 1 ? processes[0].Id : null;
}

static string PrepareInjectableDllCopy(string dllPath, string logPath)
{
    var fullDllPath = Path.GetFullPath(dllPath);
    if (!File.Exists(fullDllPath))
    {
        throw new FileNotFoundException("Native DLL was not found.", fullDllPath);
    }

    var runDllDir = GetKboFlagPath("run_dlls");
    Directory.CreateDirectory(runDllDir);

    var extension = Path.GetExtension(fullDllPath);
    var stem = Path.GetFileNameWithoutExtension(fullDllPath);
    var copyPath = Path.Combine(
        runDllDir,
        $"{stem}-{DateTimeOffset.Now:yyyyMMddHHmmssfff}-{Environment.ProcessId}{extension}");

    File.Copy(fullDllPath, copyPath, overwrite: false);

    var loaderSource = Path.Combine(Path.GetDirectoryName(fullDllPath) ?? string.Empty, "WebView2Loader.dll");
    if (File.Exists(loaderSource))
    {
        var loaderTarget = Path.Combine(runDllDir, "WebView2Loader.dll");
        File.Copy(loaderSource, loaderTarget, overwrite: true);
        Log(logPath, $"webview2_loader_copy source={loaderSource} target={loaderTarget}");
    }

    Log(logPath, $"dll_copy source={fullDllPath} target={copyPath}");
    return copyPath;
}

static bool IsKboFixAlreadyLoaded(int pid, string logPath)
{
    try
    {
        using var process = Process.GetProcessById(pid);
        foreach (ProcessModule module in process.Modules)
        {
            var moduleName = Path.GetFileName(module.FileName);
            if (moduleName.Equals("KBOFix.dll", StringComparison.OrdinalIgnoreCase)
                || (moduleName.StartsWith("KBOFix-", StringComparison.OrdinalIgnoreCase)
                    && moduleName.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)))
            {
                Log(logPath, $"already_loaded pid={pid} module={module.FileName}");
                return true;
            }
        }
    }
    catch (Exception ex) when (ex is Win32Exception or InvalidOperationException)
    {
        Log(logPath, $"already_loaded_check_failed pid={pid} error={ex.GetType().Name}:{ex.Message}");
    }

    return false;
}

static Process WaitForStableOotpProcess(
    Process launched,
    string exePath,
    DateTimeOffset launchStarted,
    TimeSpan timeout,
    string logPath)
{
    var deadline = DateTimeOffset.Now + timeout;
    Process? lastCandidate = null;

    while (DateTimeOffset.Now < deadline)
    {
        var candidates = Process.GetProcesses()
            .Where(p => p.ProcessName.Contains("ootp", StringComparison.OrdinalIgnoreCase))
            .Select(TryProcessForInjection)
            .Where(p => p is not null)
            .Cast<Process>()
            .Where(p => PathEquals(TryGetProcessPath(p), exePath))
            .OrderByDescending(TryGetStartTime)
            .ToList();

        lastCandidate = candidates.FirstOrDefault(p => p.Id == launched.Id)
            ?? candidates.FirstOrDefault(p => TryGetStartTime(p) >= launchStarted.AddSeconds(-5))
            ?? candidates.FirstOrDefault();

        if (lastCandidate is not null)
        {
            Log(logPath, $"candidate_for_injection pid={lastCandidate.Id} path={TryGetProcessPath(lastCandidate)}");
            Thread.Sleep(1500);

            try
            {
                lastCandidate.Refresh();
                if (!lastCandidate.HasExited && PathEquals(TryGetProcessPath(lastCandidate), exePath))
                {
                    Console.WriteLine($"Stable OOTP injection target pid={lastCandidate.Id}");
                    Log(logPath, $"stable_injection_target pid={lastCandidate.Id}");
                    return lastCandidate;
                }
            }
            catch
            {
                // short-lived launcher handoff process
            }
        }

        Thread.Sleep(500);
    }

    throw new TimeoutException(
        lastCandidate is null
            ? "Could not find a stable OOTP process for injection."
            : $"Could not find a stable OOTP process for injection. Last candidate pid={lastCandidate.Id}");
}

static Process? TryProcessForInjection(Process process)
{
    try
    {
        _ = process.Id;
        _ = process.ProcessName;
        _ = process.MainModule?.FileName;
        return process;
    }
    catch
    {
        process.Dispose();
        return null;
    }
}

static string TryGetProcessPath(Process process)
{
    try
    {
        return process.MainModule?.FileName ?? "";
    }
    catch
    {
        return "";
    }
}

static DateTimeOffset TryGetStartTime(Process process)
{
    try
    {
        return new DateTimeOffset(process.StartTime);
    }
    catch
    {
        return DateTimeOffset.MinValue;
    }
}

static bool PathEquals(string left, string right)
{
    if (string.IsNullOrWhiteSpace(left) || string.IsNullOrWhiteSpace(right))
    {
        return false;
    }

    return string.Equals(
        Path.GetFullPath(left),
        Path.GetFullPath(right),
        StringComparison.OrdinalIgnoreCase);
}

static void InjectDll(int pid, string dllPath, string logPath)
{
    var fullDllPath = Path.GetFullPath(dllPath);
    if (!File.Exists(fullDllPath))
    {
        throw new FileNotFoundException("DLL not found", fullDllPath);
    }

    Console.WriteLine($"Injecting DLL into pid={pid}: {fullDllPath}");
    Log(logPath, $"inject_start pid={pid} dll={fullDllPath}");

    var processHandle = NativeMethods.OpenProcess(
        NativeMethods.ProcessAccessFlags.CreateThread
        | NativeMethods.ProcessAccessFlags.QueryInformation
        | NativeMethods.ProcessAccessFlags.VirtualMemoryOperation
        | NativeMethods.ProcessAccessFlags.VirtualMemoryWrite
        | NativeMethods.ProcessAccessFlags.VirtualMemoryRead,
        false,
        pid);
    if (processHandle == IntPtr.Zero)
    {
        ThrowWin32("OpenProcess");
    }

    try
    {
        var dllBytes = System.Text.Encoding.Unicode.GetBytes(fullDllPath + "\0");
        var remotePath = NativeMethods.VirtualAllocEx(
            processHandle,
            IntPtr.Zero,
            (UIntPtr)dllBytes.Length,
            NativeMethods.AllocationType.Commit | NativeMethods.AllocationType.Reserve,
            NativeMethods.MemoryProtection.ReadWrite);
        if (remotePath == IntPtr.Zero)
        {
            ThrowWin32("VirtualAllocEx");
        }

        try
        {
            if (!NativeMethods.WriteProcessMemory(
                    processHandle,
                    remotePath,
                    dllBytes,
                    dllBytes.Length,
                    out var bytesWritten)
                || bytesWritten.ToInt64() != dllBytes.Length)
            {
                ThrowWin32("WriteProcessMemory");
            }

            var kernel32 = NativeMethods.GetModuleHandle("kernel32.dll");
            if (kernel32 == IntPtr.Zero)
            {
                ThrowWin32("GetModuleHandle(kernel32.dll)");
            }

            var loadLibrary = NativeMethods.GetProcAddress(kernel32, "LoadLibraryW");
            if (loadLibrary == IntPtr.Zero)
            {
                ThrowWin32("GetProcAddress(LoadLibraryW)");
            }

            var threadHandle = NativeMethods.CreateRemoteThread(
                processHandle,
                IntPtr.Zero,
                0,
                loadLibrary,
                remotePath,
                0,
                out var threadId);
            if (threadHandle == IntPtr.Zero)
            {
                ThrowWin32("CreateRemoteThread");
            }

            try
            {
                var wait = NativeMethods.WaitForSingleObject(threadHandle, 10000);
                if (wait != NativeMethods.WaitObject0)
                {
                    throw new TimeoutException($"Remote LoadLibraryW thread did not finish. wait=0x{wait:X}");
                }

                if (!NativeMethods.GetExitCodeThread(threadHandle, out var exitCode))
                {
                    ThrowWin32("GetExitCodeThread");
                }

                Console.WriteLine($"DLL injection thread={threadId} exit=0x{exitCode:X}");
                Log(logPath, $"inject_done pid={pid} thread={threadId} exit=0x{exitCode:X}");
                if (exitCode == 0)
                {
                    throw new InvalidOperationException("Remote LoadLibraryW returned 0.");
                }
            }
            finally
            {
                NativeMethods.CloseHandle(threadHandle);
            }
        }
        finally
        {
            NativeMethods.VirtualFreeEx(processHandle, remotePath, UIntPtr.Zero, NativeMethods.FreeType.Release);
        }
    }
    finally
    {
        NativeMethods.CloseHandle(processHandle);
    }
}

static void ThrowWin32(string operation)
{
    throw new Win32Exception(Marshal.GetLastWin32Error(), operation);
}

static class OotpBuildGuard
{
    private static readonly OotpSupportedBuild[] SupportedBuilds =
    [
        new(0x69F75E6Bu, 0x03919000u, "2026-05-04 Steam"),
    ];

    public static OotpBuildInfo Read(string exePath)
    {
        try
        {
            using var stream = File.Open(exePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            using var reader = new BinaryReader(stream);

            if (stream.Length < 0x100)
            {
                return OotpBuildInfo.Fail("file too small");
            }

            stream.Position = 0;
            if (reader.ReadUInt16() != 0x5A4D)
            {
                return OotpBuildInfo.Fail("invalid DOS signature");
            }

            stream.Position = 0x3C;
            var peHeaderOffset = reader.ReadInt32();
            if (peHeaderOffset <= 0 || peHeaderOffset + 0x58 > stream.Length)
            {
                return OotpBuildInfo.Fail("invalid PE header offset");
            }

            stream.Position = peHeaderOffset;
            if (reader.ReadUInt32() != 0x00004550)
            {
                return OotpBuildInfo.Fail("invalid PE signature");
            }

            stream.Position = peHeaderOffset + 8;
            var timestamp = reader.ReadUInt32();

            stream.Position = peHeaderOffset + 24 + 0x38;
            var sizeOfImage = reader.ReadUInt32();

            return new OotpBuildInfo(true, timestamp, sizeOfImage, null);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
        {
            return OotpBuildInfo.Fail($"{ex.GetType().Name}: {ex.Message}");
        }
    }

    public static OotpSupportedBuild? FindSupportedBuild(OotpBuildInfo info)
    {
        if (!info.Ok)
        {
            return null;
        }

        return SupportedBuilds.FirstOrDefault(build =>
            build.Timestamp == info.Timestamp && build.SizeOfImage == info.SizeOfImage);
    }

    public static string FormatConsoleStatus(OotpBuildInfo info, OotpSupportedBuild? supportedBuild)
    {
        if (!info.Ok)
        {
            return $"OOTP build: unreadable ({info.Error})";
        }

        var suffix = supportedBuild is null
            ? "unsupported"
            : $"supported ({supportedBuild.Label})";
        return $"OOTP build: timestamp=0x{info.Timestamp:X8}, size_of_image=0x{info.SizeOfImage:X8} [{suffix}]";
    }

    public static string FormatLogStatus(OotpBuildInfo info, OotpSupportedBuild? supportedBuild)
    {
        if (!info.Ok)
        {
            return $"ootp_build status=unreadable error=\"{info.Error}\"";
        }

        if (supportedBuild is null)
        {
            return $"ootp_build status=unsupported timestamp=0x{info.Timestamp:X8} size_of_image=0x{info.SizeOfImage:X8}";
        }

        return $"ootp_build status=supported label=\"{supportedBuild.Label}\" timestamp=0x{info.Timestamp:X8} size_of_image=0x{info.SizeOfImage:X8}";
    }

    public static IEnumerable<string> SupportedBuildDescriptions()
    {
        return SupportedBuilds.Select(build => $"{build.Label}:0x{build.Timestamp:X8}/0x{build.SizeOfImage:X8}");
    }
}

record OotpBuildInfo(bool Ok, uint Timestamp, uint SizeOfImage, string? Error)
{
    public static OotpBuildInfo Fail(string error)
    {
        return new OotpBuildInfo(false, 0, 0, error);
    }
}

record OotpSupportedBuild(uint Timestamp, uint SizeOfImage, string Label);

static class OotpCurrentSavePathReader
{
    private const int SystemExtendedHandleInformation = 64;

    private const uint ImageScnMemExecute = 0x20000000u;
    private const uint ImageScnMemRead = 0x40000000u;
    private const uint ImageScnMemWrite = 0x80000000u;

    private const ulong OotpMessageSavePathOffset = 0x1A58u;
    private const ulong OotpStringObjectTextOffset = 0x8u;
    private const ulong OotpTeamVectorOffset = 0x90u;
    private const ulong OotpTeamCountOffset = 0x9Cu;
    private const ulong OotpTeamLeagueIdOffset = 0x120u;
    private const ulong OotpTeamIdOffset = 0x4450u;

    public static CurrentSavePathInfo TryRead(int pid, Action<string>? log = null)
    {
        using var process = Process.GetProcessById(pid);
        ProcessModule? mainModule;
        try
        {
            mainModule = process.MainModule;
        }
        catch (Exception ex) when (ex is Win32Exception or InvalidOperationException)
        {
            return CurrentSavePathInfo.Fail("process_module_unreadable", null, $"{ex.GetType().Name}: {ex.Message}");
        }

        if (mainModule is null || string.IsNullOrWhiteSpace(mainModule.FileName))
        {
            return CurrentSavePathInfo.Fail("process_module_missing", null, "main module was not available");
        }

        var processHandle = NativeMethods.OpenProcess(
            NativeMethods.ProcessAccessFlags.QueryInformation | NativeMethods.ProcessAccessFlags.VirtualMemoryRead,
            false,
            pid);
        if (processHandle == IntPtr.Zero)
        {
            return CurrentSavePathInfo.Fail("open_process_failed", null, new Win32Exception(Marshal.GetLastWin32Error()).Message);
        }

        try
        {
            var sections = ReadWritableDataSections(mainModule.FileName);
            if (sections.Count == 0)
            {
                return CurrentSavePathInfo.Fail("pe_sections_missing", null, "no writable PE sections found");
            }

            var baseAddress = unchecked((ulong)mainModule.BaseAddress.ToInt64());
            foreach (var section in sections)
            {
                var sectionAddress = baseAddress + section.VirtualAddress;
                var sectionBytes = ReadMemory(processHandle, sectionAddress, checked((int)section.VirtualSize));
                if (sectionBytes is null)
                {
                    continue;
                }

                for (var offset = 0; offset + 8 <= sectionBytes.Length; offset += 8)
                {
                    var candidate = BitConverter.ToUInt64(sectionBytes, offset);
                    if (candidate < 0x10000u || candidate > 0x7FFFFFFFFFFFFFFFul)
                    {
                        continue;
                    }
                    if (!LooksLikeGlobalDatabase(processHandle, candidate))
                    {
                        continue;
                    }

                    var savePath = ReadOotpString(processHandle, candidate + OotpMessageSavePathOffset, 512);
                        log?.Invoke($"current_save_probe pid={pid} global=0x{candidate:X} save=\"{savePath ?? ""}\"");
                        if (LooksLikeAbsoluteLgSavePath(savePath))
                        {
                            return new CurrentSavePathInfo(true, "current_save_found", Path.GetFullPath(savePath!), null);
                        }
                }
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException or OverflowException)
        {
            return CurrentSavePathInfo.Fail("current_save_probe_failed", null, $"{ex.GetType().Name}: {ex.Message}");
        }
        finally
        {
            NativeMethods.CloseHandle(processHandle);
        }

        var handleSavePath = TryReadFromOpenFileHandles(pid, log);
        if (LooksLikeAbsoluteLgSavePath(handleSavePath))
        {
            return new CurrentSavePathInfo(true, "current_save_found_from_handles", Path.GetFullPath(handleSavePath!), null);
        }

        return CurrentSavePathInfo.Fail("current_save_unavailable", null, "could not read an opened .lg save path from the target process");
    }

    private static string? TryReadFromOpenFileHandles(int pid, Action<string>? log)
    {
        var queryLength = 0x10000;
        byte[] buffer;
        int status;
        int returnLength;
        do
        {
            buffer = new byte[queryLength];
            status = NativeMethods.NtQuerySystemInformation(
                SystemExtendedHandleInformation,
                buffer,
                buffer.Length,
                out returnLength);
            if (status == unchecked((int)0xC0000004) && returnLength > queryLength)
            {
                queryLength = returnLength + 0x10000;
            }
            else if (status == unchecked((int)0xC0000004))
            {
                queryLength *= 2;
            }
        } while (status == unchecked((int)0xC0000004) && queryLength <= 0x4000000);

        if (status != 0)
        {
            log?.Invoke($"current_save_handle_probe_failed pid={pid} status=0x{status:X8}");
            return null;
        }

        var sourceProcess = NativeMethods.OpenProcess(
            NativeMethods.ProcessAccessFlags.DuplicateHandle | NativeMethods.ProcessAccessFlags.QueryInformation,
            false,
            pid);
        if (sourceProcess == IntPtr.Zero)
        {
            log?.Invoke($"current_save_handle_probe_open_failed pid={pid} error=\"{new Win32Exception(Marshal.GetLastWin32Error()).Message}\"");
            return null;
        }

        try
        {
            var currentProcess = NativeMethods.GetCurrentProcess();
            var handleCount = checked((long)(IntPtr.Size == 8
                ? BitConverter.ToUInt64(buffer, 0)
                : BitConverter.ToUInt32(buffer, 0)));
            var offset = IntPtr.Size == 8 ? 16 : 8;
            var entrySize = IntPtr.Size == 8 ? 40 : 28;
            for (var i = 0L; i < handleCount; i++)
            {
                var entryOffset = offset + checked((int)(i * entrySize));
                if (entryOffset + entrySize > buffer.Length)
                {
                    break;
                }

                var ownerPid = IntPtr.Size == 8
                    ? BitConverter.ToUInt64(buffer, entryOffset + 8)
                    : BitConverter.ToUInt32(buffer, entryOffset + 8);
                if (ownerPid != (uint)pid)
                {
                    continue;
                }

                var rawHandle = IntPtr.Size == 8
                    ? unchecked((IntPtr)(long)BitConverter.ToUInt64(buffer, entryOffset + 16))
                    : unchecked((IntPtr)(int)BitConverter.ToUInt32(buffer, entryOffset + 16));
                if (!NativeMethods.DuplicateHandle(
                        sourceProcess,
                        rawHandle,
                        currentProcess,
                        out var duplicatedHandle,
                        0,
                        false,
                        NativeMethods.DuplicateOptions.SameAccess))
                {
                    continue;
                }

                try
                {
                    if (NativeMethods.GetFileType(duplicatedHandle) != NativeMethods.FileTypeDisk)
                    {
                        continue;
                    }

                    var path = GetPathFromHandle(duplicatedHandle);
                    var savePath = ExtractLgSavePath(path);
                    if (LooksLikeAbsoluteLgSavePath(savePath))
                    {
                        log?.Invoke($"current_save_handle_probe pid={pid} save=\"{savePath}\" source=\"{path}\"");
                        return savePath;
                    }
                }
                finally
                {
                    NativeMethods.CloseHandle(duplicatedHandle);
                }
            }
        }
        finally
        {
            NativeMethods.CloseHandle(sourceProcess);
        }

        log?.Invoke($"current_save_handle_probe_unavailable pid={pid}");
        return null;
    }

    private static string? GetPathFromHandle(IntPtr handle)
    {
        var buffer = new char[1024];
        var length = NativeMethods.GetFinalPathNameByHandle(handle, buffer, buffer.Length, 0);
        if (length == 0)
        {
            return null;
        }
        if (length >= buffer.Length)
        {
            buffer = new char[length + 1];
            length = NativeMethods.GetFinalPathNameByHandle(handle, buffer, buffer.Length, 0);
            if (length == 0 || length >= buffer.Length)
            {
                return null;
            }
        }
        return new string(buffer, 0, checked((int)length));
    }

    private static string? ExtractLgSavePath(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return null;
        }

        var normalized = path.StartsWith(@"\\?\", StringComparison.Ordinal)
            ? path[4..]
            : path;
        var marker = ".lg";
        var index = normalized.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return null;
        }

        var end = index + marker.Length;
        if (end < normalized.Length && normalized[end] != '\\' && normalized[end] != '/')
        {
            return null;
        }

        return normalized[..end];
    }

    private static List<PeSection> ReadWritableDataSections(string exePath)
    {
        using var stream = File.Open(exePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        using var reader = new BinaryReader(stream);

        if (stream.Length < 0x100)
        {
            return [];
        }

        stream.Position = 0x3C;
        var peHeaderOffset = reader.ReadInt32();
        if (peHeaderOffset <= 0 || peHeaderOffset + 0x18 > stream.Length)
        {
            return [];
        }

        stream.Position = peHeaderOffset;
        if (reader.ReadUInt32() != 0x00004550)
        {
            return [];
        }

        stream.Position = peHeaderOffset + 6;
        var sectionCount = reader.ReadUInt16();
        stream.Position = peHeaderOffset + 20;
        var optionalHeaderSize = reader.ReadUInt16();

        var sectionOffset = peHeaderOffset + 24 + optionalHeaderSize;
        var sections = new List<PeSection>();
        for (var i = 0; i < sectionCount; i++)
        {
            var current = sectionOffset + i * 40;
            if (current + 40 > stream.Length)
            {
                break;
            }

            stream.Position = current + 8;
            var virtualSize = reader.ReadUInt32();
            var virtualAddress = reader.ReadUInt32();
            stream.Position = current + 36;
            var characteristics = reader.ReadUInt32();

            var writableData = (characteristics & ImageScnMemWrite) != 0
                && (characteristics & ImageScnMemRead) != 0
                && (characteristics & ImageScnMemExecute) == 0;
            if (writableData && virtualSize >= 8)
            {
                sections.Add(new PeSection(virtualAddress, virtualSize));
            }
        }

        return sections;
    }

    private static bool LooksLikeGlobalDatabase(IntPtr processHandle, ulong candidate)
    {
        var header = ReadMemory(processHandle, candidate, 0xB0);
        if (header is null)
        {
            return false;
        }

        var teamCount = BitConverter.ToInt32(header, (int)OotpTeamCountOffset);
        if (teamCount < 2 || teamCount > 500)
        {
            return false;
        }

        var teamVector = BitConverter.ToUInt64(header, (int)OotpTeamVectorOffset);
        if (teamVector == 0)
        {
            return false;
        }

        var teamPointers = ReadMemory(processHandle, teamVector, teamCount * 8);
        if (teamPointers is null)
        {
            return false;
        }

        var checks = Math.Min(teamCount, 5);
        for (var i = 0; i < checks; i++)
        {
            var team = BitConverter.ToUInt64(teamPointers, i * 8);
            if (team == 0)
            {
                return false;
            }

            var teamIdBytes = ReadMemory(processHandle, team + OotpTeamIdOffset, 4);
            var leagueIdBytes = ReadMemory(processHandle, team + OotpTeamLeagueIdOffset, 4);
            if (teamIdBytes is null || leagueIdBytes is null)
            {
                return false;
            }

            var teamId = BitConverter.ToUInt32(teamIdBytes, 0);
            var leagueId = BitConverter.ToUInt32(leagueIdBytes, 0);
            if (teamId == 0 || teamId > 100000 || leagueId == 0 || leagueId > 100000)
            {
                return false;
            }
        }

        return true;
    }

    private static string? ReadOotpString(IntPtr processHandle, ulong stringObjectAddress, int maxBytes)
    {
        var pointerBytes = ReadMemory(processHandle, stringObjectAddress + OotpStringObjectTextOffset, 8);
        if (pointerBytes is null)
        {
            return null;
        }

        var textPointer = BitConverter.ToUInt64(pointerBytes, 0);
        if (textPointer < 0x10000u)
        {
            return null;
        }

        var bytes = ReadMemory(processHandle, textPointer, maxBytes);
        if (bytes is null)
        {
            return null;
        }

        var used = 0;
        for (; used < bytes.Length; used++)
        {
            var value = bytes[used];
            if (value == 0)
            {
                break;
            }
            if (value < 0x20 || value > 0x7E)
            {
                return null;
            }
        }

        return used == 0 ? null : System.Text.Encoding.ASCII.GetString(bytes, 0, used);
    }

    private static byte[]? ReadMemory(IntPtr processHandle, ulong address, int size)
    {
        if (size <= 0 || address < 0x10000u)
        {
            return null;
        }

        var buffer = new byte[size];
        if (!NativeMethods.ReadProcessMemory(
                processHandle,
                unchecked((IntPtr)(long)address),
                buffer,
                size,
                out var bytesRead)
            || bytesRead.ToInt64() != size)
        {
            return null;
        }

        return buffer;
    }

    private static bool LooksLikeAbsoluteLgSavePath(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return false;
        }

        var absolute = Path.IsPathFullyQualified(path);
        return absolute
            && path.EndsWith(".lg", StringComparison.OrdinalIgnoreCase)
            && Directory.Exists(path);
    }

    private readonly record struct PeSection(uint VirtualAddress, uint VirtualSize);
}

record CurrentSavePathInfo(bool Ok, string Status, string? SavePath, string? Error)
{
    public static CurrentSavePathInfo Fail(string status, string? savePath, string error)
    {
        return new CurrentSavePathInfo(false, status, savePath, error);
    }
}

static class KboRosterMarkerGuard
{
    public const string RequiredMarkerUrl = "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO";

    public static RosterMarkerInfo CheckCurrentSave(int pid, Action<string>? log = null)
    {
        var savePathInfo = OotpCurrentSavePathReader.TryRead(pid, log);
        if (!savePathInfo.Ok)
        {
            return RosterMarkerInfo.Fail(savePathInfo.Status, savePathInfo.SavePath, null, savePathInfo.Error ?? "current save path unavailable");
        }

        return CheckSavePath(savePathInfo.SavePath!);
    }

    private static RosterMarkerInfo CheckSavePath(string savePath)
    {
        var normalizedSavePath = Path.GetFullPath(savePath);
        if (!Directory.Exists(normalizedSavePath))
        {
            return RosterMarkerInfo.Fail("current_save_missing", normalizedSavePath, null, "current save directory was not found");
        }
        if (!normalizedSavePath.EndsWith(".lg", StringComparison.OrdinalIgnoreCase))
        {
            return RosterMarkerInfo.Fail("current_save_not_lg", normalizedSavePath, null, "current save path is not an .lg directory");
        }

        var descriptionPath = Path.Combine(normalizedSavePath, "description.txt");
        if (!File.Exists(descriptionPath))
        {
            return RosterMarkerInfo.Fail("description_missing", normalizedSavePath, descriptionPath, "description.txt not found");
        }

        string text;
        try
        {
            text = File.ReadAllText(descriptionPath);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return RosterMarkerInfo.Fail("description_unreadable", normalizedSavePath, descriptionPath, $"{ex.GetType().Name}: {ex.Message}");
        }

        if (text.IndexOf(RequiredMarkerUrl, StringComparison.OrdinalIgnoreCase) < 0)
        {
            return RosterMarkerInfo.Fail("marker_missing", normalizedSavePath, descriptionPath, "required GitHub marker was not found");
        }

        return new RosterMarkerInfo(true, "marked", normalizedSavePath, descriptionPath, null);
    }

    public static string FormatConsoleStatus(RosterMarkerInfo info)
    {
        if (info.Ok)
        {
            return $"KBO roster marker: current save marked in {info.DescriptionPath}";
        }

        return $"KBO roster marker: blocked ({info.Status})";
    }

    public static string FormatLogStatus(RosterMarkerInfo info)
    {
        var save = string.IsNullOrWhiteSpace(info.SavePath) ? "" : info.SavePath;
        var description = string.IsNullOrWhiteSpace(info.DescriptionPath) ? "" : info.DescriptionPath;
        var error = string.IsNullOrWhiteSpace(info.Error) ? "" : info.Error;
        return $"roster_marker status={info.Status} ok={(info.Ok ? "1" : "0")} save=\"{save}\" description=\"{description}\" error=\"{error}\"";
    }
}

record RosterMarkerInfo(bool Ok, string Status, string? SavePath, string? DescriptionPath, string? Error)
{
    public static RosterMarkerInfo Fail(string status, string? savePath, string? descriptionPath, string error)
    {
        return new RosterMarkerInfo(false, status, savePath, descriptionPath, error);
    }
}

record ProcessInfo(int Id, string Name, string Path);

record LauncherOptions(
    string? OotpPath,
    string? DllPath,
    int? AttachPid,
    bool AttachExisting,
    bool? EnableMilitaryDraftPool,
    bool? EnableForeignWaiverAi,
    bool? EnableSingleDivisionAllstarEvents,
    bool DryRun,
    bool AllowSecondInstance,
    bool ShowHelp,
    IReadOnlyList<string> OotpArgs)
{
    public static LauncherOptions Parse(string[] args)
    {
        string? ootpPath = null;
        string? dllPath = null;
        int? attachPid = null;
        var attachExisting = false;
        bool? enableMilitaryDraftPool = null;
        bool? enableForeignWaiverAi = null;
        bool? enableSingleDivisionAllstarEvents = null;
        var dryRun = false;
        var allowSecondInstance = false;
        var showHelp = false;
        var ootpArgs = new List<string>();

        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];
            switch (arg)
            {
                case "--ootp":
                    if (++i >= args.Length)
                        throw new ArgumentException("--ootp requires a path");
                    ootpPath = args[i];
                    break;
                case "--dll":
                    if (++i >= args.Length)
                        throw new ArgumentException("--dll requires a path");
                    dllPath = args[i];
                    break;
                case "--attach-pid":
                    if (++i >= args.Length || !int.TryParse(args[i], out var parsedPid))
                        throw new ArgumentException("--attach-pid requires a numeric process id");
                    attachPid = parsedPid;
                    break;
                case "--attach-existing":
                    attachExisting = true;
                    break;
                case "--enable-military-draft-pool":
                    enableMilitaryDraftPool = true;
                    break;
                case "--disable-military-draft-pool":
                    enableMilitaryDraftPool = false;
                    break;
                case "--enable-foreign-waiver-ai":
                    enableForeignWaiverAi = true;
                    break;
                case "--disable-foreign-waiver-ai":
                    enableForeignWaiverAi = false;
                    break;
                case "--enable-single-division-allstar-events":
                    enableSingleDivisionAllstarEvents = true;
                    break;
                case "--disable-single-division-allstar-events":
                    enableSingleDivisionAllstarEvents = false;
                    break;
                case "--dry-run":
                    dryRun = true;
                    break;
                case "--allow-second-instance":
                    allowSecondInstance = true;
                    break;
                case "-h":
                case "--help":
                    showHelp = true;
                    break;
                case "--":
                    ootpArgs.AddRange(args.Skip(i + 1));
                    i = args.Length;
                    break;
                default:
                    throw new ArgumentException($"Unknown argument: {arg}");
            }
        }

        return new LauncherOptions(
            ootpPath,
            dllPath,
            attachPid,
            attachExisting,
            enableMilitaryDraftPool,
            enableForeignWaiverAi,
            enableSingleDivisionAllstarEvents,
            dryRun,
            allowSecondInstance,
            showHelp,
            ootpArgs);
    }

    public static void PrintHelp()
    {
        Console.WriteLine("""
        KBOLauncher

        Usage:
          KBOLauncher
          KBOLauncher [--ootp path] [--allow-second-instance] [--] [ootp args...]
          KBOLauncher --attach-existing --dll path
          KBOLauncher --attach-pid pid --dll path

        Options:
          --ootp PATH                  Explicit ootp27.exe path.
          --dll PATH                   Native DLL to load into OOTP.
          --attach-existing            Attach to the single running OOTP process.
          --attach-pid PID             Attach to a specific OOTP process id.
          --enable-military-draft-pool Enable the military draft pool flag (default when run with no args).
          --disable-military-draft-pool
                                       Disable the military draft pool flag.
          --enable-foreign-waiver-ai    Enable foreign waiver assistant log.
          --disable-foreign-waiver-ai   Disable foreign waiver assistant log.
          --enable-single-division-allstar-events
                                       Enable single-division all-star event creation.
          --disable-single-division-allstar-events
                                       Disable single-division all-star event creation.
          --dry-run                    Resolve paths and detect existing process without launching.
          --allow-second-instance      Launch even if an OOTP process is already running.
          --                           Pass remaining arguments to OOTP.

        Safety:
          KBOFix injection is disabled unless ootp27.exe matches a verified build.
          KBOFix injection also requires the currently opened OOTP .lg save description.txt
          to contain the official roster marker URL.
        """);
    }
}

static class NativeMethods
{
    public const uint WaitObject0 = 0x00000000;

    [Flags]
    public enum ProcessAccessFlags : uint
    {
        CreateThread = 0x0002,
        QueryInformation = 0x0400,
        DuplicateHandle = 0x0040,
        VirtualMemoryOperation = 0x0008,
        VirtualMemoryRead = 0x0010,
        VirtualMemoryWrite = 0x0020,
    }

    [Flags]
    public enum DuplicateOptions : uint
    {
        SameAccess = 0x00000002,
    }

    [Flags]
    public enum AllocationType : uint
    {
        Commit = 0x1000,
        Reserve = 0x2000,
    }

    public enum MemoryProtection : uint
    {
        ReadWrite = 0x04,
    }

    public enum FreeType : uint
    {
        Release = 0x8000,
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(
        ProcessAccessFlags processAccess,
        [MarshalAs(UnmanagedType.Bool)] bool inheritHandle,
        int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr VirtualAllocEx(
        IntPtr hProcess,
        IntPtr lpAddress,
        UIntPtr dwSize,
        AllocationType flAllocationType,
        MemoryProtection flProtect);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool VirtualFreeEx(
        IntPtr hProcess,
        IntPtr lpAddress,
        UIntPtr dwSize,
        FreeType dwFreeType);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool WriteProcessMemory(
        IntPtr hProcess,
        IntPtr lpBaseAddress,
        byte[] lpBuffer,
        int nSize,
        out IntPtr lpNumberOfBytesWritten);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ReadProcessMemory(
        IntPtr hProcess,
        IntPtr lpBaseAddress,
        byte[] lpBuffer,
        int nSize,
        out IntPtr lpNumberOfBytesRead);

    [DllImport("kernel32.dll")]
    public static extern IntPtr GetCurrentProcess();

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool DuplicateHandle(
        IntPtr hSourceProcessHandle,
        IntPtr hSourceHandle,
        IntPtr hTargetProcessHandle,
        out IntPtr lpTargetHandle,
        uint dwDesiredAccess,
        [MarshalAs(UnmanagedType.Bool)] bool bInheritHandle,
        DuplicateOptions dwOptions);

    [DllImport("kernel32.dll", EntryPoint = "GetFinalPathNameByHandleW", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern uint GetFinalPathNameByHandle(
        IntPtr hFile,
        [Out] char[] lpszFilePath,
        int cchFilePath,
        uint dwFlags);

    public const uint FileTypeDisk = 0x0001;

    [DllImport("kernel32.dll")]
    public static extern uint GetFileType(IntPtr hFile);

    [DllImport("ntdll.dll")]
    public static extern int NtQuerySystemInformation(
        int systemInformationClass,
        byte[] systemInformation,
        int systemInformationLength,
        out int returnLength);

    [DllImport("kernel32.dll", EntryPoint = "GetModuleHandleW", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern IntPtr GetModuleHandle(string lpModuleName);

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr CreateRemoteThread(
        IntPtr hProcess,
        IntPtr lpThreadAttributes,
        uint dwStackSize,
        IntPtr lpStartAddress,
        IntPtr lpParameter,
        uint dwCreationFlags,
        out uint lpThreadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetExitCodeThread(IntPtr hThread, out uint lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool CloseHandle(IntPtr hObject);
}
