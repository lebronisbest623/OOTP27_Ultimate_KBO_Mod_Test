
internal static class LauncherPaths
{
    public static string? ResolveOotpPath(string? explicitPath)
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
    
    public static string? ResolveDefaultKboFixDllPath()
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
    
    public static string GetLogPath()
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(local, "OOTP-KBO", "launcher.log");
    }
    
    public static string GetKboLocalDataPath(string fileName)
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(local, "OOTP-KBO", fileName);
    }

    public static string GetKboFlagConfigPath()
    {
        return GetKboLocalDataPath("kbo_flags.json");
    }
}
