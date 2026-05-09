using System.ComponentModel;
using System.Diagnostics;
using static LauncherLog;

internal static class InjectedModuleDetector
{
    public static bool IsKboFixAlreadyLoaded(int pid, string logPath)
        => TryGetLoadedKboFixModulePath(pid, logPath, out _);

    public static bool TryGetLoadedKboFixModulePath(int pid, string logPath, out string modulePath)
    {
        modulePath = string.Empty;
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
                    modulePath = module.FileName;
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

    public static void WarnIfLoadedKboFixLooksStale(int pid, string requestedDllPath, string logPath)
    {
        if (!TryGetLoadedKboFixModulePath(pid, logPath, out var loadedPath)) {
            return;
        }

        try
        {
            var requestedWriteTime = File.GetLastWriteTimeUtc(requestedDllPath);
            var loadedWriteTime = File.GetLastWriteTimeUtc(loadedPath);
            if (loadedWriteTime >= requestedWriteTime) {
                return;
            }

            var message =
                $"KBOFix already loaded in pid={pid}, but it is older than the requested DLL. Restart OOTP to load the new patch.";
            Console.Error.WriteLine(message);
            Log(
                logPath,
                $"already_loaded_stale pid={pid} loaded=\"{loadedPath}\" loaded_write=\"{loadedWriteTime:O}\" requested=\"{requestedDllPath}\" requested_write=\"{requestedWriteTime:O}\" action=restart_required");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or NotSupportedException)
        {
            Log(logPath, $"already_loaded_stale_check_failed pid={pid} error={ex.GetType().Name}:{ex.Message}");
        }
    }
}
