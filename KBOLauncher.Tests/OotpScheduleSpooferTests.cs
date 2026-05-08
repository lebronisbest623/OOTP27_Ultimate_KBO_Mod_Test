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
    public void EnsureAllKboScheduleSpoofFiles_WritesMajorLeagueTargetsAndBacksUpExistingFiles()
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

        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 2, 0, 0), result);
        Assert.Contains("type=\"4\"", File.ReadAllText(Path.Combine(schedulesDir, "major_league_ml_c_2098.lsdl")));
        Assert.Contains("type=\"4\"", File.ReadAllText(Path.Combine(schedulesDir, "major_league_ml_c_2098_ap.lsdl")));
        Assert.Equal("original regular", File.ReadAllText(Path.Combine(backupDir, "major_league_ml_c_2098.lsdl")));
        Assert.Equal("original ap", File.ReadAllText(Path.Combine(backupDir, "major_league_ml_c_2098_ap.lsdl")));
    }

    [Fact]
    public void EnsureAllKboScheduleSpoofFiles_IsIdempotentAfterFirstWrite()
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

        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 2, 0, 0), first);
        Assert.Equal(new global::OotpScheduleSpoofer.Result(1, 0, 2, 0), second);
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
