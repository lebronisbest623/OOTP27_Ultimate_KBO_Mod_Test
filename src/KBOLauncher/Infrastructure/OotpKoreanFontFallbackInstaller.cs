using static LauncherLog;
using static LauncherPaths;

internal static class OotpKoreanFontFallbackInstaller
{
    private const string EnableFlagFileName = "enable_kbo_korean_font_fallback.txt";

    private static readonly string[] TargetFontFolders =
    [
        "font0",
        "font1",
        "font2",
        "font3",
        "font4",
        "font5",
    ];

    private static readonly FontCopyMap[] FontFiles =
    [
        new("regular.ttf", "regular.ttf"),
        new("medium.ttf", "medium.ttf"),
        new("bold.ttf", "bold.ttf"),
        new("light.ttf", "light.ttf"),
        new("italic.ttf", "italic.ttf"),
        new("digit.ttf", "digit.ttf"),
        new("bb_regular.ttf", "regular.ttf"),
        new("bb_medium.ttf", "medium.ttf"),
        new("bb_bold.ttf", "bb_bold.ttf"),
        new("bb_light.ttf", "light.ttf"),
        new("bb_italic.ttf", "italic.ttf"),
        new("bb_condensed.ttf", "medium.ttf"),
    ];

    public static void EnsureOotpKoreanFontFallback(string exePath, string logPath, bool dryRun)
    {
        if (!KboFlags.ReadKboFlagDefaultEnabled(EnableFlagFileName))
        {
            Log(logPath, "korean_font_fallback=disabled");
            return;
        }

        var ootpDir = Path.GetDirectoryName(exePath);
        if (string.IsNullOrWhiteSpace(ootpDir))
        {
            Log(logPath, "korean_font_fallback=skipped reason=no_ootp_dir");
            return;
        }

        var fontsRoot = Path.Combine(ootpDir, "data", "fonts");
        var sourceDir = Path.Combine(fontsRoot, "font20");
        if (!Directory.Exists(sourceDir))
        {
            Log(logPath, $"korean_font_fallback=skipped reason=no_font20 path=\"{sourceDir}\"");
            return;
        }

        try
        {
            var pending = GetPendingCopies(fontsRoot, sourceDir).ToArray();
            if (pending.Length == 0)
            {
                Log(logPath, "korean_font_fallback=ready copied=0");
                return;
            }

            if (dryRun)
            {
                Console.WriteLine($"Dry-run: Korean font fallback would update {pending.Length} OOTP font file(s).");
                Log(logPath, $"korean_font_fallback=dry_run pending={pending.Length}");
                return;
            }

            var backupRoot = Path.Combine(fontsRoot, ".kbo_original_font_backup");
            var copied = 0;
            foreach (var op in pending)
            {
                var backupPath = Path.Combine(backupRoot, op.TargetFolder, op.TargetFileName);
                Directory.CreateDirectory(Path.GetDirectoryName(backupPath)!);
                if (!File.Exists(backupPath))
                {
                    File.Copy(op.TargetPath, backupPath, overwrite: false);
                }

                File.Copy(op.SourcePath, op.TargetPath, overwrite: true);
                copied++;
            }

            WriteStatusFile(copied, fontsRoot, backupRoot, null);
            Console.WriteLine($"Installed Korean font fallback for OOTP English UI ({copied} file(s)).");
            Log(logPath, $"korean_font_fallback=installed copied={copied} backup=\"{backupRoot}\"");
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            WriteStatusFile(0, fontsRoot, "", ex.Message);
            Console.Error.WriteLine("Could not install Korean font fallback. Run the launcher as administrator or select JejuGothic in OOTP settings.");
            Log(logPath, $"korean_font_fallback=failed error=\"{ex.Message.Replace("\"", "'")}\"");
        }
    }

    private static IEnumerable<FontCopyOperation> GetPendingCopies(string fontsRoot, string sourceDir)
    {
        foreach (var folder in TargetFontFolders)
        {
            var targetDir = Path.Combine(fontsRoot, folder);
            if (!Directory.Exists(targetDir))
            {
                continue;
            }

            foreach (var map in FontFiles)
            {
                var sourcePath = Path.Combine(sourceDir, map.SourceFileName);
                var targetPath = Path.Combine(targetDir, map.TargetFileName);
                if (!File.Exists(sourcePath) || !File.Exists(targetPath))
                {
                    continue;
                }

                if (FilesEqual(sourcePath, targetPath))
                {
                    continue;
                }

                yield return new FontCopyOperation(folder, map.TargetFileName, sourcePath, targetPath);
            }
        }
    }

    private static bool FilesEqual(string left, string right)
    {
        var leftInfo = new FileInfo(left);
        var rightInfo = new FileInfo(right);
        if (leftInfo.Length != rightInfo.Length)
        {
            return false;
        }

        using var leftStream = File.OpenRead(left);
        using var rightStream = File.OpenRead(right);
        Span<byte> leftBuffer = stackalloc byte[8192];
        Span<byte> rightBuffer = stackalloc byte[8192];
        while (true)
        {
            var leftRead = leftStream.Read(leftBuffer);
            var rightRead = rightStream.Read(rightBuffer);
            if (leftRead != rightRead)
            {
                return false;
            }
            if (leftRead == 0)
            {
                return true;
            }
            if (!leftBuffer[..leftRead].SequenceEqual(rightBuffer[..rightRead]))
            {
                return false;
            }
        }
    }

    private static void WriteStatusFile(int copied, string fontsRoot, string backupRoot, string? error)
    {
        var statusPath = GetKboLocalDataPath("korean_font_fallback_status.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(statusPath)!);
        File.WriteAllLines(
            statusPath,
            [
                $"checked_at={DateTimeOffset.Now:O}",
                $"copied={copied}",
                $"fonts_root={fontsRoot}",
                $"backup_root={backupRoot}",
                $"error={error ?? ""}",
            ]);
    }

    private sealed record FontCopyMap(string TargetFileName, string SourceFileName);

    private sealed record FontCopyOperation(
        string TargetFolder,
        string TargetFileName,
        string SourcePath,
        string TargetPath);
}
