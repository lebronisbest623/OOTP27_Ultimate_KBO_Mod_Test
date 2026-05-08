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

        Directory.CreateDirectory(backupDir);

        var scanned = 0;
        var written = 0;
        var unchanged = 0;
        var failed = 0;

        foreach (var sourcePath in Directory.EnumerateFiles(schedulesDir, "korean_baseball_organization_int_c_*.lsdl"))
        {
            var match = KboScheduleFileName.Match(Path.GetFileName(sourcePath));
            if (!match.Success || !int.TryParse(match.Groups["year"].Value, out var year))
            {
                continue;
            }

            scanned++;
            foreach (var suffix in new[] { "", "_ap" })
            {
                var targetName = $"major_league_ml_c_{year}{suffix}.lsdl";
                var targetPath = Path.Combine(schedulesDir, targetName);
                try
                {
                    BackupScheduleIfNeeded(targetPath, Path.Combine(backupDir, targetName), log);
                    var sourceText = File.ReadAllText(sourcePath);
                    var spoofText = BuildSpoofScheduleText(sourceText, year, log);
                    if (File.Exists(targetPath) && File.ReadAllText(targetPath) == spoofText)
                    {
                        unchanged++;
                        continue;
                    }

                    File.WriteAllText(targetPath, spoofText);
                    written++;
                    log?.Invoke($"schedule_spoof wrote year={year} source=\"{sourcePath}\" target=\"{targetPath}\"");
                }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
                {
                    failed++;
                    log?.Invoke($"schedule_spoof failed year={year} target=\"{targetPath}\" error=\"{ex.Message}\"");
                }
            }
        }

        log?.Invoke($"schedule_spoof complete scanned={scanned} written={written} unchanged={unchanged} failed={failed}");
        return new Result(scanned, written, unchanged, failed);
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

    [GeneratedRegex(@"^korean_baseball_organization_int_c_(?<year>\d{4})\.lsdl$", RegexOptions.IgnoreCase)]
    private static partial Regex KboScheduleFileNameRegex();
}
