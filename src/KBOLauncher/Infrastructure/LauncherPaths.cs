
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
        candidates.AddRange(ResolveSteamLibraryOotpCandidates());
    
        return ResolveExistingNewestPath(candidates);
    }

    internal static string? ResolveExistingNewestPath(IEnumerable<string> candidates)
    {
        return candidates
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .Where(File.Exists)
            .OrderByDescending(File.GetLastWriteTimeUtc)
            .FirstOrDefault();
    }

    private static IEnumerable<string> ResolveSteamLibraryOotpCandidates()
    {
        foreach (var steamRoot in ResolveSteamRoots())
        {
            yield return Path.Combine(steamRoot, "steamapps", "common", "Out of the Park Baseball 27", "ootp27.exe");

            var libraryFolders = Path.Combine(steamRoot, "steamapps", "libraryfolders.vdf");
            foreach (var libraryRoot in ReadSteamLibraryFolders(libraryFolders))
            {
                yield return Path.Combine(libraryRoot, "steamapps", "common", "Out of the Park Baseball 27", "ootp27.exe");
            }
        }
    }

    private static IEnumerable<string> ResolveSteamRoots()
    {
        var candidates = new List<string?>
        {
            Environment.GetEnvironmentVariable("STEAM_DIR"),
            @"C:\Program Files (x86)\Steam",
            @"C:\Program Files\Steam",
        };

        foreach (var drive in DriveInfo.GetDrives())
        {
            if (drive.DriveType != DriveType.Fixed && drive.DriveType != DriveType.Removable)
            {
                continue;
            }

            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Steam"));
            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files (x86)", "Steam"));
            candidates.Add(Path.Combine(drive.RootDirectory.FullName, "Program Files", "Steam"));
        }

        foreach (var candidate in candidates)
        {
            if (!string.IsNullOrWhiteSpace(candidate) && Directory.Exists(candidate))
            {
                yield return candidate;
            }
        }
    }

    internal static IEnumerable<string> ReadSteamLibraryFolders(string libraryFoldersPath)
    {
        if (!File.Exists(libraryFoldersPath))
        {
            yield break;
        }

        string[] lines;
        try
        {
            lines = File.ReadAllLines(libraryFoldersPath);
        }
        catch
        {
            yield break;
        }

        foreach (var rawLine in lines)
        {
            var line = rawLine.Trim();
            if (!line.StartsWith("\"path\"", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var parts = line.Split('"', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
            if (parts.Length < 2)
            {
                continue;
            }

            var path = parts[^1].Replace(@"\\", @"\");
            if (Directory.Exists(path))
            {
                yield return path;
            }
        }
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
