namespace KBOLauncher.Tests;

using Xunit;

public sealed class OotpScheduleSpooferTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-schedule-spoof-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void BuildSpoofScheduleText_AddsExplicitAllstarGameWhenOnlyHeaderDayExists()
    {
        var source = """
            <?xml version="1.0" encoding="ISO-8859-1"?>
            <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
            <GAMES>
            <GAME day="1" time="1400" away="1" home="2" />
            </GAMES>
            </SCHEDULE>
            """;

        var actual = global::OotpScheduleSpoofer.BuildSpoofScheduleText(source, 2026);

        Assert.Contains("""<GAME day="106" time="1830" away="0" home="0" type="4" />""", actual);
        Assert.Contains("</SCHEDULE>", actual);
    }

    [Fact]
    public void BuildSpoofScheduleText_DoesNotDuplicateExplicitAllstarGame()
    {
        var source = """
            <SCHEDULE allstar_game_day="106">
            <GAMES>
            <GAME day="106" time="1830" away="0" home="0" type="4" />
            </GAMES>
            </SCHEDULE>
            """;

        var actual = global::OotpScheduleSpoofer.BuildSpoofScheduleText(source, 2026);

        Assert.Equal(source, actual);
    }

    [Theory]
    [InlineData("""<SCHEDULE allstar_game_day="106">""", true, 106)]
    [InlineData("""<SCHEDULE allstar_game_day="0">""", false, 0)]
    [InlineData("""<SCHEDULE>""", false, 0)]
    public void TryExtractAllstarGameDay_ParsesPositiveHeaderDay(string text, bool expectedResult, int expectedDay)
    {
        var result = global::OotpScheduleSpoofer.TryExtractAllstarGameDay(text, out var day);

        Assert.Equal(expectedResult, result);
        Assert.Equal(expectedDay, day);
    }

    [Fact]
    public void EnsureAllKboScheduleSpoofFiles_DoesNotMutateMajorLeagueTargets()
    {
        var installDir = Path.Combine(tempDir, "ootp");
        var schedulesDir = Path.Combine(installDir, "data", "schedules");
        var backupDir = Path.Combine(tempDir, "backups");
        Directory.CreateDirectory(schedulesDir);
        var exePath = Path.Combine(installDir, "ootp27.exe");
        File.WriteAllText(exePath, "");
        File.WriteAllText(Path.Combine(schedulesDir, "korean_baseball_organization_int_c_2098.lsdl"), """
            <SCHEDULE allstar_game_day="106">
            <GAMES>
            <GAME day="1" time="1400" away="1" home="2" />
            </GAMES>
            </SCHEDULE>
            """);
        File.WriteAllText(Path.Combine(schedulesDir, "major_league_ml_c_2098.lsdl"), "original regular");
        File.WriteAllText(Path.Combine(schedulesDir, "major_league_ml_c_2098_ap.lsdl"), "original ap");

        var result = global::OotpScheduleSpoofer.EnsureAllKboScheduleSpoofFiles(exePath, backupDir);

        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 0, 0, 0), result);
        Assert.Equal("original regular", File.ReadAllText(Path.Combine(schedulesDir, "major_league_ml_c_2098.lsdl")));
        Assert.Equal("original ap", File.ReadAllText(Path.Combine(schedulesDir, "major_league_ml_c_2098_ap.lsdl")));
        Assert.False(File.Exists(Path.Combine(backupDir, "major_league_ml_c_2098.lsdl")));
        Assert.False(File.Exists(Path.Combine(backupDir, "major_league_ml_c_2098_ap.lsdl")));
    }

    [Fact]
    public void EnsureAllKboScheduleSpoofFiles_DoesNotRestoreFromKboSchedule()
    {
        var installDir = Path.Combine(tempDir, "ootp");
        var schedulesDir = Path.Combine(installDir, "data", "schedules");
        var backupDir = Path.Combine(tempDir, "backups");
        Directory.CreateDirectory(schedulesDir);
        var exePath = Path.Combine(installDir, "ootp27.exe");
        File.WriteAllText(exePath, "");
        var source = """
            <SCHEDULE allstar_game_day="106">
            <GAMES>
            <GAME day="1" time="1400" away="1" home="2" />
            </GAMES>
            </SCHEDULE>
            """;
        var spoofed = global::OotpScheduleSpoofer.BuildSpoofScheduleText(source, 2097);
        File.WriteAllText(Path.Combine(schedulesDir, "korean_baseball_organization_int_c_2097.lsdl"), source);
        File.WriteAllText(Path.Combine(schedulesDir, "major_league_ml_c_2097.lsdl"), spoofed);
        File.WriteAllText(Path.Combine(schedulesDir, "major_league_ml_c_2097_ap.lsdl"), "user modified");

        var result = global::OotpScheduleSpoofer.EnsureAllKboScheduleSpoofFiles(exePath, backupDir);

        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 0, 0, 0), result);
        Assert.Equal(spoofed, File.ReadAllText(Path.Combine(schedulesDir, "major_league_ml_c_2097.lsdl")));
        Assert.Equal("user modified", File.ReadAllText(Path.Combine(schedulesDir, "major_league_ml_c_2097_ap.lsdl")));
    }

    [Fact]
    public void EnsureAllKboScheduleSpoofFiles_IsIdempotentNoOp()
    {
        var installDir = Path.Combine(tempDir, "ootp");
        var schedulesDir = Path.Combine(installDir, "data", "schedules");
        var backupDir = Path.Combine(tempDir, "backups");
        Directory.CreateDirectory(schedulesDir);
        var exePath = Path.Combine(installDir, "ootp27.exe");
        File.WriteAllText(exePath, "");
        File.WriteAllText(Path.Combine(schedulesDir, "korean_baseball_organization_int_c_2099.lsdl"), """
            <SCHEDULE allstar_game_day="107">
            <GAMES>
            <GAME day="1" time="1400" away="1" home="2" />
            </GAMES>
            </SCHEDULE>
            """);

        var first = global::OotpScheduleSpoofer.EnsureAllKboScheduleSpoofFiles(exePath, backupDir);
        var second = global::OotpScheduleSpoofer.EnsureAllKboScheduleSpoofFiles(exePath, backupDir);

        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 0, 0, 0), first);
        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 0, 0, 0), second);
    }

    [Fact]
    public void EnsureAllKboScheduleSpoofFiles_DoesNotOverwriteExistingBackups()
    {
        var installDir = Path.Combine(tempDir, "ootp");
        var schedulesDir = Path.Combine(installDir, "data", "schedules");
        var backupDir = Path.Combine(tempDir, "backups");
        Directory.CreateDirectory(schedulesDir);
        Directory.CreateDirectory(backupDir);
        var exePath = Path.Combine(installDir, "ootp27.exe");
        File.WriteAllText(exePath, "");
        File.WriteAllText(Path.Combine(schedulesDir, "korean_baseball_organization_int_c_2100.lsdl"), """
            <SCHEDULE allstar_game_day="107">
            <GAMES>
            <GAME day="1" time="1400" away="1" home="2" />
            </GAMES>
            </SCHEDULE>
            """);
        File.WriteAllText(Path.Combine(schedulesDir, "major_league_ml_c_2100.lsdl"), "current original");
        File.WriteAllText(Path.Combine(backupDir, "major_league_ml_c_2100.lsdl"), "older backup");

        _ = global::OotpScheduleSpoofer.EnsureAllKboScheduleSpoofFiles(exePath, backupDir);

        Assert.Equal("older backup", File.ReadAllText(Path.Combine(backupDir, "major_league_ml_c_2100.lsdl")));
        Assert.Equal("current original", File.ReadAllText(Path.Combine(schedulesDir, "major_league_ml_c_2100.lsdl")));
    }

    [Fact]
    public void RestoreAllKboScheduleSpoofFiles_RestoresOriginalTargetsFromBackups()
    {
        var installDir = Path.Combine(tempDir, "ootp");
        var schedulesDir = Path.Combine(installDir, "data", "schedules");
        var backupDir = Path.Combine(tempDir, "backups");
        Directory.CreateDirectory(schedulesDir);
        Directory.CreateDirectory(backupDir);
        var exePath = Path.Combine(installDir, "ootp27.exe");
        var targetPath = Path.Combine(schedulesDir, "major_league_ml_c_2101.lsdl");
        var backupPath = Path.Combine(backupDir, "major_league_ml_c_2101.lsdl");
        File.WriteAllText(exePath, "");
        File.WriteAllText(targetPath, "spoofed");
        File.WriteAllText(backupPath, "original");

        var result = global::OotpScheduleSpoofer.RestoreAllKboScheduleSpoofFiles(exePath, backupDir);

        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 1, 0, 0), result);
        Assert.Equal("original", File.ReadAllText(targetPath));
    }

    [Fact]
    public void EnsureAllKboScheduleSpoofFiles_FailsClosedWhenSchedulesDirectoryIsMissing()
    {
        var installDir = Path.Combine(tempDir, "ootp");
        Directory.CreateDirectory(installDir);
        var exePath = Path.Combine(installDir, "ootp27.exe");
        File.WriteAllText(exePath, "");

        var result = global::OotpScheduleSpoofer.EnsureAllKboScheduleSpoofFiles(
            exePath,
            Path.Combine(tempDir, "backups"));

        Assert.Equal(new global::OotpScheduleSpoofer.Result(0, 0, 0, 1), result);
    }

    public void Dispose()
    {
        try
        {
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, recursive: true);
            }
        }
        catch (IOException)
        {
        }
    }
}
