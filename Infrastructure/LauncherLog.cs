
internal static class LauncherLog
{
    public static void Log(string path, string message)
    {
        var line = $"[{DateTimeOffset.Now:O}] {message}{Environment.NewLine}";
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);

        for (var attempt = 0; attempt < 5; attempt++)
        {
            try
            {
                using var stream = new FileStream(path, FileMode.Append, FileAccess.Write, FileShare.ReadWrite);
                using var writer = new StreamWriter(stream);
                writer.Write(line);
                return;
            }
            catch (IOException) when (attempt < 4)
            {
                Thread.Sleep(25);
            }
        }
    }
}
