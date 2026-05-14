using System.Text.Json;
using System.Text.RegularExpressions;

using static LauncherPaths;

internal static class KboSeedFiles
{
    private const string SeedManifestFileName = "seed_manifest.json";

    private static readonly JsonSerializerOptions SeedManifestJsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

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
    
    public static void EnsureBundledKboDataManifest()
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var localDir = Path.Combine(local, "OOTP-KBO");
        var manifestPath = ResolveBundledKboDataFileCandidates(SeedManifestFileName).FirstOrDefault(File.Exists);
        if (manifestPath is null)
        {
            Console.WriteLine($"{SeedManifestFileName}: bundled seed manifest not found");
            return;
        }

        EnsureBundledKboDataManifest(localDir, manifestPath, Path.GetDirectoryName(manifestPath)!);
    }

    internal static void EnsureBundledKboDataManifest(string localDir, string manifestPath, string dataRoot)
    {
        Directory.CreateDirectory(localDir);

        KboSeedManifest? manifest;
        try
        {
            manifest = JsonSerializer.Deserialize<KboSeedManifest>(
                File.ReadAllText(manifestPath),
                SeedManifestJsonOptions);
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to read {SeedManifestFileName}: {ex.Message}");
            return;
        }

        if (manifest?.Groups is null)
        {
            Console.WriteLine($"{SeedManifestFileName}: no seed groups found");
            return;
        }

        var seeded = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var file in manifest.Groups.SelectMany(group => group.Files ?? []))
        {
            if (string.IsNullOrWhiteSpace(file.Path))
            {
                continue;
            }

            string targetRelativePath;
            try
            {
                targetRelativePath = NormalizeSeedManifestRelativePath(file.Path);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{SeedManifestFileName}: ignored invalid seed path '{file.Path}': {ex.Message}");
                continue;
            }

            var source = string.IsNullOrWhiteSpace(file.Source) ? file.Path : file.Source;
            string sourceRelativePath;
            try
            {
                sourceRelativePath = NormalizeSeedManifestRelativePath(source);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{SeedManifestFileName}: ignored invalid seed source '{source}': {ex.Message}");
                continue;
            }

            if (!seeded.Add(targetRelativePath))
            {
                continue;
            }

            var candidates = new List<string> { Path.Combine(dataRoot, sourceRelativePath) };
            candidates.AddRange(ResolveBundledKboDataFileCandidates(sourceRelativePath));
            if (!string.Equals(sourceRelativePath, targetRelativePath, StringComparison.OrdinalIgnoreCase))
            {
                candidates.Add(Path.Combine(dataRoot, targetRelativePath));
                candidates.AddRange(ResolveBundledKboDataFileCandidates(targetRelativePath));
            }

            EnsureBundledKboDataFile(localDir, targetRelativePath, ManifestLabel(file), candidates);
        }

        foreach (var retiredFile in manifest.RetiredFiles ?? [])
        {
            if (string.IsNullOrWhiteSpace(retiredFile.Path))
            {
                continue;
            }

            string relativePath;
            try
            {
                relativePath = NormalizeSeedManifestRelativePath(retiredFile.Path);
            }
            catch
            {
                continue;
            }

            RemoveRetiredBundledKboDataFileIfUnchanged(localDir, relativePath, ManifestLabel(retiredFile));
        }
    }

    public static void EnsureBundledKboDataFile(string fileName, string label)
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var localDir = Path.Combine(local, "OOTP-KBO");
        EnsureBundledKboDataFile(localDir, fileName, label, ResolveBundledKboDataFileCandidates(fileName));
    }

    public static void EnsureBundledKboDataDirectory(string directoryName, string label)
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var localDir = Path.Combine(local, "OOTP-KBO");
        EnsureBundledKboDataDirectory(localDir, directoryName, label,
        [
            Path.Combine(AppContext.BaseDirectory, "data", "seeds", directoryName),
            Path.Combine(Environment.CurrentDirectory, "data", "seeds", directoryName),
            Path.Combine(AppContext.BaseDirectory, directoryName),
            Path.Combine(Environment.CurrentDirectory, directoryName)
        ]);
    }

    public static void RemoveRetiredBundledKboDataFileIfUnchanged(string fileName, string label)
    {
        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var localDir = Path.Combine(local, "OOTP-KBO");
        RemoveRetiredBundledKboDataFileIfUnchanged(localDir, fileName, label);
    }

    internal static void RemoveRetiredBundledKboDataFileIfUnchanged(string localDir, string fileName, string label)
    {
        var localPath = Path.Combine(localDir, fileName);
        if (!File.Exists(localPath))
        {
            return;
        }

        try
        {
            var meaningfulLines = File.ReadAllLines(localPath)
                .Select(line => line.Trim())
                .Where(line => line.Length > 0 && !line.StartsWith("#", StringComparison.Ordinal) && !line.StartsWith(";", StringComparison.Ordinal))
                .ToArray();

            var isRetiredForeignReplacementSeed = string.Equals(fileName, "foreign_replacement_players_seed.csv", StringComparison.OrdinalIgnoreCase)
                && meaningfulLines.All(line =>
                    line.StartsWith("verhadr01,", StringComparison.OrdinalIgnoreCase)
                    || line.StartsWith("olougja01,", StringComparison.OrdinalIgnoreCase));

            if (!isRetiredForeignReplacementSeed)
            {
                return;
            }

            File.Delete(localPath);
            Console.WriteLine($"{label}: removed retired bundled seed {localPath}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Failed to remove retired {fileName}: {ex.Message}");
        }
    }

    internal static void EnsureBundledKboDataFile(
        string localDir,
        string fileName,
        string label,
        IReadOnlyList<string> candidates)
    {
        var localPath = Path.Combine(localDir, fileName);

        Directory.CreateDirectory(Path.GetDirectoryName(localPath)!);

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

    private static IReadOnlyList<string> ResolveBundledKboDataFileCandidates(string relativePath)
    {
        return
        [
            Path.Combine(AppContext.BaseDirectory, "data", "seeds", relativePath),
            Path.Combine(Environment.CurrentDirectory, "data", "seeds", relativePath),
            Path.Combine(AppContext.BaseDirectory, relativePath),
            Path.Combine(Environment.CurrentDirectory, relativePath),
            Path.Combine(AppContext.BaseDirectory, "native", relativePath)
        ];
    }

    private static string NormalizeSeedManifestRelativePath(string path)
    {
        var normalized = path.Trim()
            .Replace('/', Path.DirectorySeparatorChar)
            .Replace('\\', Path.DirectorySeparatorChar);

        if (Path.IsPathFullyQualified(normalized))
        {
            throw new InvalidOperationException("absolute paths are not allowed");
        }

        var segments = normalized.Split(Path.DirectorySeparatorChar, StringSplitOptions.RemoveEmptyEntries);
        if (segments.Length == 0 || segments.Any(segment => segment == ".."))
        {
            throw new InvalidOperationException("empty paths and parent traversal are not allowed");
        }

        return Path.Combine(segments);
    }

    private static string ManifestLabel(KboSeedManifestFile file)
    {
        return string.IsNullOrWhiteSpace(file.Label) ? file.Path : file.Label;
    }

    private sealed class KboSeedManifest
    {
        public List<KboSeedManifestGroup>? Groups { get; set; }

        public List<KboSeedManifestFile>? RetiredFiles { get; set; }
    }

    private sealed class KboSeedManifestGroup
    {
        public List<KboSeedManifestFile>? Files { get; set; }
    }

    private sealed class KboSeedManifestFile
    {
        public string Path { get; set; } = "";

        public string Source { get; set; } = "";

        public string Label { get; set; } = "";
    }

    internal static void EnsureBundledKboDataDirectory(
        string localDir,
        string directoryName,
        string label,
        IReadOnlyList<string> candidates)
    {
        var localPath = Path.Combine(localDir, directoryName);

        Directory.CreateDirectory(localDir);

        foreach (var candidate in candidates)
        {
            if (!Directory.Exists(candidate))
            {
                continue;
            }

            try
            {
                var copied = 0;
                foreach (var sourcePath in Directory.EnumerateFiles(candidate, "*", SearchOption.AllDirectories))
                {
                    var relative = Path.GetRelativePath(candidate, sourcePath);
                    var targetPath = Path.Combine(localPath, relative);
                    Directory.CreateDirectory(Path.GetDirectoryName(targetPath)!);

                    var shouldCopy = !File.Exists(targetPath)
                        || File.GetLastWriteTimeUtc(sourcePath) > File.GetLastWriteTimeUtc(targetPath)
                        || new FileInfo(sourcePath).Length != new FileInfo(targetPath).Length;
                    if (!shouldCopy)
                    {
                        continue;
                    }

                    File.Copy(sourcePath, targetPath, overwrite: true);
                    copied++;
                }

                if (copied > 0)
                {
                    Console.WriteLine($"{label}: seeded {copied} file(s) under {localPath}");
                }
                return;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Failed to seed {directoryName} from {candidate}: {ex.Message}");
                return;
            }
        }

        Console.WriteLine($"{label}: bundled seed directory not found for {directoryName}");
    }

    public static void EnsureKboScheduleAllstarGameLines(string? ootpExePath)
    {
        foreach (var directory in ResolveKboScheduleDirectories(ootpExePath).Distinct(StringComparer.OrdinalIgnoreCase))
        {
            EnsureKboScheduleAllstarGameLinesInDirectory(directory);
        }
    }

    internal static void EnsureKboScheduleAllstarGameLinesInDirectory(string scheduleDirectory)
    {
        if (string.IsNullOrWhiteSpace(scheduleDirectory) || !Directory.Exists(scheduleDirectory))
        {
            return;
        }

        foreach (var path in Directory.EnumerateFiles(scheduleDirectory, "korean_baseball_organization_int_c_*.lsdl"))
        {
            try
            {
                if (EnsureKboScheduleAllstarGameLine(path))
                {
                    Console.WriteLine($"KBO schedule all-star game: repaired {path}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"KBO schedule all-star game: failed to repair {path}: {ex.Message}");
            }
        }
    }

    internal static bool EnsureKboScheduleAllstarGameLine(string path)
    {
        var text = File.ReadAllText(path);
        var gamesMatch = Regex.Match(text, "<GAMES>\\s*", RegexOptions.IgnoreCase);
        var gamesEndMatch = Regex.Match(text, "</GAMES>", RegexOptions.IgnoreCase);
        var hasTypeFourInsideGames = false;
        if (gamesMatch.Success && gamesEndMatch.Success && gamesEndMatch.Index > gamesMatch.Index)
        {
            var gamesBlock = text.Substring(gamesMatch.Index, gamesEndMatch.Index - gamesMatch.Index);
            hasTypeFourInsideGames = gamesBlock.Contains("type=\"4\"", StringComparison.OrdinalIgnoreCase);
        }
        else if (text.Contains("type=\"4\"", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        var changed = false;
        if (!hasTypeFourInsideGames)
        {
            var dayMatch = Regex.Match(text, "allstar_game_day\\s*=\\s*\"(?<day>\\d+)\"", RegexOptions.IgnoreCase);
            if (!dayMatch.Success || !int.TryParse(dayMatch.Groups["day"].Value, out var day) || day <= 0)
            {
                return false;
            }

            var newline = text.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
            var allstarLine = $"<Game day=\"{day}\" time=\"1830\" away=\"0\" home=\"0\" type=\"4\" />{newline}";
            if (gamesMatch.Success)
            {
                var insertAt = gamesMatch.Index + gamesMatch.Length;
                text = text.Insert(insertAt, allstarLine);
            }
            else
            {
                var scheduleHeaderEnd = text.IndexOf('>');
                if (scheduleHeaderEnd < 0)
                {
                    return false;
                }
                text = text.Insert(scheduleHeaderEnd + 1, newline + allstarLine);
            }
            changed = true;
        }

        var cleaned = RemoveKboAllstarGameLinesOutsideGamesBlock(text);
        if (!string.Equals(cleaned, text, StringComparison.Ordinal))
        {
            text = cleaned;
            changed = true;
        }

        if (changed)
        {
            File.WriteAllText(path, text);
        }
        return changed;
    }

    private static string RemoveKboAllstarGameLinesOutsideGamesBlock(string text)
    {
        var gamesStart = Regex.Match(text, "<GAMES>\\s*", RegexOptions.IgnoreCase);
        var gamesEnd = Regex.Match(text, "</GAMES>", RegexOptions.IgnoreCase);
        if (!gamesStart.Success || !gamesEnd.Success || gamesEnd.Index <= gamesStart.Index)
        {
            return text;
        }

        var before = text[..gamesStart.Index];
        var games = text[gamesStart.Index..gamesEnd.Index];
        var after = text[gamesEnd.Index..];
        const string strayAllstarGameLine = "^[ \\t]*<Game\\b[^\\r\\n>]*type\\s*=\\s*\"4\"[^\\r\\n>]*>\\s*(?:\\r?\\n)?";
        before = Regex.Replace(before, strayAllstarGameLine, "", RegexOptions.IgnoreCase | RegexOptions.Multiline);
        after = Regex.Replace(after, strayAllstarGameLine, "", RegexOptions.IgnoreCase | RegexOptions.Multiline);
        return before + games + after;
    }

    private static IEnumerable<string> ResolveKboScheduleDirectories(string? ootpExePath)
    {
        if (!string.IsNullOrWhiteSpace(ootpExePath))
        {
            var ootpDir = Path.GetDirectoryName(ootpExePath);
            if (!string.IsNullOrWhiteSpace(ootpDir))
            {
                yield return Path.Combine(ootpDir, "data", "schedules");
            }
        }

        var documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
        if (!string.IsNullOrWhiteSpace(documents))
        {
            yield return Path.Combine(documents, "Out of the Park Developments", "OOTP Baseball 27", "schedules");
        }
    }
}
