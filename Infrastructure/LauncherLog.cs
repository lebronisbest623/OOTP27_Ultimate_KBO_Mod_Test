
internal static class LauncherLog
{
    public static void Log(string path, string message)
    {
        File.AppendAllText(path, $"[{DateTimeOffset.Now:O}] {message}{Environment.NewLine}");
    }
}
