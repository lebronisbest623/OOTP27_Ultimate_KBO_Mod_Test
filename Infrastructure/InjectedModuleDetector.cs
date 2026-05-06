using System.ComponentModel;
using System.Diagnostics;
using static LauncherLog;

internal static class InjectedModuleDetector
{
    public static bool IsKboFixAlreadyLoaded(int pid, string logPath)
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
}
