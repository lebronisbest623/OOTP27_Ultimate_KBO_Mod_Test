using static LauncherPaths;

internal static class KboSeedFiles
{
    public static void EnsureKboLeagueIdConfig()
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
    
    public static void EnsureBundledKboDataFile(string fileName, string label)
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var localDir = Path.Combine(local, "OOTP-KBO");
        var localPath = Path.Combine(localDir, fileName);
    
        Directory.CreateDirectory(localDir);
    
        var candidates = new[]
        {
            Path.Combine(AppContext.BaseDirectory, "data", "seeds", fileName),
            Path.Combine(Environment.CurrentDirectory, "data", "seeds", fileName),
            Path.Combine(AppContext.BaseDirectory, fileName),
            Path.Combine(Environment.CurrentDirectory, fileName),
            Path.Combine(AppContext.BaseDirectory, "native", fileName)
        };
    
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
    }
    
}
