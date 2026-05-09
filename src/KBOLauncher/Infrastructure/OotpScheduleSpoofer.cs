using System.Text.RegularExpressions;

internal static partial class OotpScheduleSpoofer
{
    private static readonly Regex KboScheduleFileName = KboScheduleFileNameRegex();

    public sealed record Result(int ScannedYears, int WrittenFiles, int UnchangedFiles, int FailedFiles);

    public static Result EnsureAllKboScheduleSpoofFiles(string ootpExePath, Action<string>? log = null)
    {
        var backupDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "OOTP-KBO",
            "schedule_backups");
        return EnsureAllKboScheduleSpoofFiles(ootpExePath, backupDir, log);
    }

    internal static Result EnsureAllKboScheduleSpoofFiles(string ootpExePath, string backupDir, Action<string>? log = null)
    {
        var installDir = Path.GetDirectoryName(Path.GetFullPath(ootpExePath));
        if (string.IsNullOrWhiteSpace(installDir))
        {
            log?.Invoke("schedule_spoof skipped reason=install_dir_unavailable");
            return new Result(0, 0, 0, 1);
        }

        var schedulesDir = Path.Combine(installDir, "data", "schedules");
        if (!Directory.Exists(schedulesDir))
        {
            log?.Invoke($"schedule_spoof skipped reason=schedules_dir_missing dir=\"{schedulesDir}\"");
            return new Result(0, 0, 0, 1);
        }

        var scanned = 0;
        var restored = 0;
        var unchanged = 0;
        var failed = 0;
        foreach (var kboPath in Directory.EnumerateFiles(schedulesDir, "korean_baseball_organization_int_c_*.lsdl"))
        {
            var match = KboScheduleFileName.Match(Path.GetFileName(kboPath));
            if (!match.Success)
            {
                continue;
            }

            scanned++;
            var year = match.Groups["year"].Value;
            var repair = RepairMajorLeagueScheduleIfKboCopy(schedulesDir, backupDir, kboPath, year, log);
            restored += repair.WrittenFiles;
            unchanged += repair.UnchangedFiles;
            failed += repair.FailedFiles;
        }

        log?.Invoke($"schedule_spoof retired scanned={scanned} written={restored} unchanged={unchanged} failed={failed} reason=global_major_league_schedule_mutation_disabled");
        return new Result(scanned, restored, unchanged, failed);
    }

    internal static Result RestoreAllKboScheduleSpoofFiles(string ootpExePath, string backupDir, Action<string>? log = null)
    {
        var installDir = Path.GetDirectoryName(Path.GetFullPath(ootpExePath));
        if (string.IsNullOrWhiteSpace(installDir))
        {
            log?.Invoke("schedule_restore skipped reason=install_dir_unavailable");
            return new Result(0, 0, 0, 1);
        }

        var schedulesDir = Path.Combine(installDir, "data", "schedules");
        if (!Directory.Exists(schedulesDir) || !Directory.Exists(backupDir))
        {
            log?.Invoke("schedule_restore skipped reason=restore_dir_missing");
            return new Result(0, 0, 0, 1);
        }

        var scanned = 0;
        var restored = 0;
        var unchanged = 0;
        var failed = 0;
        foreach (var backupPath in Directory.EnumerateFiles(backupDir, "major_league_ml_c_*.lsdl"))
        {
            scanned++;
            var targetPath = Path.Combine(schedulesDir, Path.GetFileName(backupPath));
            try
            {
                var backupText = File.ReadAllText(backupPath);
                if (File.Exists(targetPath) && File.ReadAllText(targetPath) == backupText)
                {
                    unchanged++;
                    continue;
                }

                File.WriteAllText(targetPath, backupText);
                restored++;
                log?.Invoke($"schedule_restore restored backup=\"{backupPath}\" target=\"{targetPath}\"");
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                failed++;
                log?.Invoke($"schedule_restore failed backup=\"{backupPath}\" error=\"{ex.Message}\"");
            }
        }

        log?.Invoke($"schedule_restore complete scanned={scanned} restored={restored} unchanged={unchanged} failed={failed}");
        return new Result(scanned, restored, unchanged, failed);
    }

    internal static string BuildSpoofScheduleText(string sourceText, int year, Action<string>? log = null)
    {
        if (ContainsExplicitAllstarGame(sourceText) || !TryExtractAllstarGameDay(sourceText, out var allstarDay))
        {
            return sourceText;
        }

        var line = $"    <GAME day=\"{allstarDay}\" time=\"1830\" away=\"0\" home=\"0\" type=\"4\" />{Environment.NewLine}";
        var closeIndex = sourceText.IndexOf("</SCHEDULE>", StringComparison.OrdinalIgnoreCase);
        if (closeIndex < 0)
        {
            log?.Invoke($"schedule_spoof patched_allstar_game year={year} day={allstarDay} insertion=end");
            return sourceText + Environment.NewLine + line;
        }

        log?.Invoke($"schedule_spoof patched_allstar_game year={year} day={allstarDay}");
        return sourceText[..closeIndex] + line + sourceText[closeIndex..];
    }

    internal static bool TryExtractAllstarGameDay(string text, out int day)
    {
        day = 0;
        const string marker = "allstar_game_day=\"";
        var index = text.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (index < 0)
        {
            return false;
        }

        index += marker.Length;
        var value = 0;
        var digits = 0;
        while (index < text.Length && char.IsAsciiDigit(text[index]))
        {
            value = value * 10 + (text[index] - '0');
            index++;
            digits++;
        }

        if (digits == 0 || value <= 0)
        {
            return false;
        }

        day = value;
        return true;
    }

    private static bool ContainsExplicitAllstarGame(string text)
    {
        return text.Contains("type=\"4\"", StringComparison.OrdinalIgnoreCase)
            || text.Contains("type='4'", StringComparison.OrdinalIgnoreCase);
    }

    private static void BackupScheduleIfNeeded(string targetPath, string backupPath, Action<string>? log)
    {
        if (!File.Exists(targetPath) || File.Exists(backupPath))
        {
            return;
        }

        File.Copy(targetPath, backupPath, overwrite: false);
        log?.Invoke($"schedule_spoof backup target=\"{targetPath}\" backup=\"{backupPath}\"");
    }

    private static Result RepairMajorLeagueScheduleIfKboCopy(
        string schedulesDir,
        string backupDir,
        string kboPath,
        string year,
        Action<string>? log)
    {
        var restored = 0;
        var unchanged = 0;
        var failed = 0;
        foreach (var suffix in new[] { ".lsdl", "_ap.lsdl" })
        {
            var targetPath = Path.Combine(schedulesDir, $"major_league_ml_c_{year}{suffix}");
            var backupPath = Path.Combine(backupDir, $"major_league_ml_c_{year}{suffix}");
            try
            {
                if (!File.Exists(targetPath))
                {
                    unchanged++;
                    continue;
                }

                var targetText = File.ReadAllText(targetPath);
                var kboText = File.ReadAllText(kboPath);
                var targetIsExactKboCopy = string.Equals(targetText, kboText, StringComparison.Ordinal);
                var targetIsLegacySpoofedKboCopy = IsLegacySpoofedKboCopy(targetText, kboText, year);
                if (!targetIsExactKboCopy && !targetIsLegacySpoofedKboCopy)
                {
                    unchanged++;
                    continue;
                }

                if (TryReadUsableBackup(backupPath, kboText, year, out var backupText, log))
                {
                    File.WriteAllText(targetPath, backupText);
                    restored++;
                    log?.Invoke($"schedule_restore repaired_kbo_copy backup=\"{backupPath}\" target=\"{targetPath}\" kbo=\"{kboPath}\"");
                    continue;
                }

                if (targetIsLegacySpoofedKboCopy)
                {
                    File.WriteAllText(targetPath, kboText);
                    restored++;
                    log?.Invoke($"schedule_restore removed_legacy_allstar_spoof target=\"{targetPath}\" kbo=\"{kboPath}\"");
                    continue;
                }

                unchanged++;
                log?.Invoke($"schedule_restore skipped_kbo_copy_no_usable_backup target=\"{targetPath}\" backup=\"{backupPath}\"");
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                failed++;
                log?.Invoke($"schedule_restore repair_failed target=\"{targetPath}\" error=\"{ex.Message}\"");
            }
        }

        return new Result(0, restored, unchanged, failed);
    }

    private static bool TryReadUsableBackup(
        string backupPath,
        string kboText,
        string year,
        out string backupText,
        Action<string>? log)
    {
        backupText = "";
        if (!File.Exists(backupPath))
        {
            return false;
        }

        backupText = File.ReadAllText(backupPath);
        if (string.Equals(backupText, kboText, StringComparison.Ordinal)
            || IsLegacySpoofedKboCopy(backupText, kboText, year))
        {
            log?.Invoke($"schedule_restore ignored_contaminated_backup backup=\"{backupPath}\"");
            return false;
        }

        return true;
    }

    private static bool IsLegacySpoofedKboCopy(string targetText, string kboText, string year)
    {
        var numericYear = int.TryParse(year, out var parsedYear) ? parsedYear : 0;
        var legacySpoofText = BuildSpoofScheduleText(kboText, numericYear);
        return !string.Equals(legacySpoofText, kboText, StringComparison.Ordinal)
            && string.Equals(targetText, legacySpoofText, StringComparison.Ordinal);
    }

    [GeneratedRegex(@"^korean_baseball_organization_int_c_(?<year>\d{4})\.lsdl$", RegexOptions.IgnoreCase)]
    private static partial Regex KboScheduleFileNameRegex();
}
