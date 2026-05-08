using System.Diagnostics;

internal static class ProcessDiscovery
{
    public static ProcessInfo? TryDescribe(Process process)
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

    public static int? ResolveSingleExistingProcess(IReadOnlyList<ProcessInfo> processes)
    {
        return processes.Count == 1 ? processes[0].Id : null;
    }
}
