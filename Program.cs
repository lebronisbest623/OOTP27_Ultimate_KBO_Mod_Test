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
        VirtualMemoryOperation = 0x0008,
        VirtualMemoryRead = 0x0010,
        VirtualMemoryWrite = 0x0020,
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
