using static LauncherPaths;

internal static class KboSeedFiles
{
    public static void EnsureKboLeagueIdConfig()
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var localDir = Path.Combine(local, "OOTP-KBO");
        EnsureKboLeagueIdConfig(localDir,
        [
            Path.Combine(AppContext.BaseDirectory, "kbo_league_id.txt"),
            Path.Combine(Environment.CurrentDirectory, "kbo_league_id.txt"),
            Path.Combine(AppContext.BaseDirectory, "native", "kbo_league_id.txt")
        ]);
    }

    internal static void EnsureKboLeagueIdConfig(string localDir, IReadOnlyList<string> candidates)
    {
        var localPath = Path.Combine(localDir, "kbo_league_id.txt");

        Directory.CreateDirectory(localDir);

        foreach (var candidate in candidates)
        {
            if (!File.Exists(candidate))
            {
                continue;
            }
    
            try
            {
                var shouldCopy = !File.Exists(localPath)
                    || !File.ReadAllText(localPath).Trim().Equals(File.ReadAllText(candidate).Trim(), StringComparison.OrdinalIgnoreCase);
                if (shouldCopy)
                {
                    File.Copy(candidate, localPath, overwrite: true);
                    Console.WriteLine($"KBO league id: seeded {localPath}");
                }
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
    
    public static void EnsureBundledKboDataFile(string fileName, string label)
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var localDir = Path.Combine(local, "OOTP-KBO");
        EnsureBundledKboDataFile(localDir, fileName, label,
        [
            Path.Combine(AppContext.BaseDirectory, "data", "seeds", fileName),
            Path.Combine(Environment.CurrentDirectory, "data", "seeds", fileName),
            Path.Combine(AppContext.BaseDirectory, fileName),
            Path.Combine(Environment.CurrentDirectory, fileName),
            Path.Combine(AppContext.BaseDirectory, "native", fileName)
        ]);
    }

    internal static void EnsureBundledKboDataFile(
        string localDir,
        string fileName,
        string label,
        IReadOnlyList<string> candidates)
    {
        var localPath = Path.Combine(localDir, fileName);

        Directory.CreateDirectory(localDir);

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

        Console.WriteLine($"{label}: bundled seed not found for {fileName}");
    }
}
