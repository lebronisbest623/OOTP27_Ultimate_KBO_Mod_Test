using System.Diagnostics;
using static LauncherLog;

internal static class InjectionTargetResolver
{
    public static Process WaitForStableOotpProcess(
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
    
    public static Process? TryProcessForInjection(Process process)
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
    
    public static string TryGetProcessPath(Process process)
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
    
    public static DateTimeOffset TryGetStartTime(Process process)
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
    
    public static bool PathEquals(string left, string right)
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
}
