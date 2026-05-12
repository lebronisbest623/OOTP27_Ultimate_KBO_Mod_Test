using System.Text.RegularExpressions;

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
