namespace KBOLauncher.Tests;

using Xunit;

public sealed class KboSeedFilesTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-seed-file-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void EnsureKboLeagueIdConfig_CopiesFirstExistingCandidate()
    {
        var localDir = Path.Combine(tempDir, "local");
        var missing = Path.Combine(tempDir, "missing", "kbo_league_id.txt");
        var candidate = Path.Combine(tempDir, "candidate", "kbo_league_id.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        File.WriteAllText(candidate, "100");

        global::KboSeedFiles.EnsureKboLeagueIdConfig(localDir, [missing, candidate]);

        Assert.Equal("100", File.ReadAllText(Path.Combine(localDir, "kbo_league_id.txt")));
    }

    [Fact]
    public void EnsureKboLeagueIdConfig_DoesNotRewriteMatchingTrimmedValue()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "kbo_league_id.txt");
        var localPath = Path.Combine(localDir, "kbo_league_id.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        Directory.CreateDirectory(localDir);
        File.WriteAllText(candidate, "100\r\n");
        File.WriteAllText(localPath, "100");
        var before = File.GetLastWriteTimeUtc(localPath);
        File.SetLastWriteTimeUtc(localPath, before.AddMinutes(-5));
        before = File.GetLastWriteTimeUtc(localPath);

        global::KboSeedFiles.EnsureKboLeagueIdConfig(localDir, [candidate]);

        Assert.Equal("100", File.ReadAllText(localPath));
        Assert.Equal(before, File.GetLastWriteTimeUtc(localPath));
    }

    [Fact]
    public void EnsureBundledKboDataFile_CopiesWhenMissingAndWhenCandidateIsNewer()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "allstar_teams.csv");
        var localPath = Path.Combine(localDir, "allstar_teams.csv");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        Directory.CreateDirectory(localDir);
        File.WriteAllText(candidate, "team_id,name\n1,A");

        global::KboSeedFiles.EnsureBundledKboDataFile(localDir, "allstar_teams.csv", "All-Star seed", [candidate]);

        Assert.Equal("team_id,name\n1,A", File.ReadAllText(localPath));

        File.WriteAllText(candidate, "team_id,name\n1,A\n2,B");
        File.SetLastWriteTimeUtc(candidate, File.GetLastWriteTimeUtc(localPath).AddMinutes(1));

        global::KboSeedFiles.EnsureBundledKboDataFile(localDir, "allstar_teams.csv", "All-Star seed", [candidate]);

        Assert.Equal("team_id,name\n1,A\n2,B", File.ReadAllText(localPath));
    }

    [Fact]
    public void EnsureBundledKboDataFile_DoesNotRewriteUnchangedLocalFile()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "fa_rules.json");
        var localPath = Path.Combine(localDir, "fa_rules.json");
        Directory.CreateDirectory(Path.GetDirectoryName(candidate)!);
        Directory.CreateDirectory(localDir);
        File.WriteAllText(candidate, "{}");
        File.WriteAllText(localPath, "{}");
        var stamp = new DateTime(2026, 5, 1, 0, 0, 0, DateTimeKind.Utc);
        File.SetLastWriteTimeUtc(candidate, stamp);
        File.SetLastWriteTimeUtc(localPath, stamp.AddMinutes(1));
        var before = File.GetLastWriteTimeUtc(localPath);

        global::KboSeedFiles.EnsureBundledKboDataFile(localDir, "fa_rules.json", "FA rules", [candidate]);

        Assert.Equal("{}", File.ReadAllText(localPath));
        Assert.Equal(before, File.GetLastWriteTimeUtc(localPath));
    }

    [Fact]
    public void EnsureBundledKboDataDirectory_CopiesNestedFilesWhenMissingOrChanged()
    {
        var localDir = Path.Combine(tempDir, "local");
        var candidate = Path.Combine(tempDir, "candidate", "news_templates");
        var sourcePath = Path.Combine(candidate, "ko", "captain.json");
        var localPath = Path.Combine(localDir, "news_templates", "ko", "captain.json");
        Directory.CreateDirectory(Path.GetDirectoryName(sourcePath)!);
        File.WriteAllText(sourcePath, "{\"captain.summary.title\":\"A\"}");

        global::KboSeedFiles.EnsureBundledKboDataDirectory(localDir, "news_templates", "News templates", [candidate]);

        Assert.Equal("{\"captain.summary.title\":\"A\"}", File.ReadAllText(localPath));

        File.WriteAllText(sourcePath, "{\"captain.summary.title\":\"B\"}");
        File.SetLastWriteTimeUtc(sourcePath, File.GetLastWriteTimeUtc(localPath).AddMinutes(1));

        global::KboSeedFiles.EnsureBundledKboDataDirectory(localDir, "news_templates", "News templates", [candidate]);

        Assert.Equal("{\"captain.summary.title\":\"B\"}", File.ReadAllText(localPath));
    }

    [Fact]
    public void RemoveRetiredBundledKboDataFileIfUnchanged_RemovesOnlyOldForeignReplacementSeed()
    {
        var localDir = Path.Combine(tempDir, "local");
        var retiredPath = Path.Combine(localDir, "foreign_replacement_players_seed.csv");
        Directory.CreateDirectory(localDir);
        File.WriteAllText(retiredPath, """
        # replacement_player_key,slot_type,comment
        verhadr01,regular,Drew VerHagen
        olougja01,regular,Jack O'Loughlin
        """);

        global::KboSeedFiles.RemoveRetiredBundledKboDataFileIfUnchanged(
            localDir,
            "foreign_replacement_players_seed.csv",
            "Foreign replacement player seed");

        Assert.False(File.Exists(retiredPath));

        File.WriteAllText(retiredPath, "custom01,regular,Keep me");

        global::KboSeedFiles.RemoveRetiredBundledKboDataFileIfUnchanged(
            localDir,
            "foreign_replacement_players_seed.csv",
            "Foreign replacement player seed");

        Assert.True(File.Exists(retiredPath));
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_AddsMissingTypeFourGame()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
        <GAMES>
        <GAME day="1" time="1400" away="1" home="2" />
        </GAMES>
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        var text = File.ReadAllText(path);
        Assert.True(repaired);
        Assert.Contains("<Game day=\"106\" time=\"1830\" away=\"0\" home=\"0\" type=\"4\" />", text);
        Assert.True(text.IndexOf("type=\"4\"", StringComparison.Ordinal) < text.IndexOf("day=\"1\"", StringComparison.Ordinal));
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_DoesNotDuplicateExistingTypeFourGame()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
        <GAMES>
        <Game day="106" time="1830" away="0" home="0" type="4" />
        </GAMES>
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        Assert.False(repaired);
        Assert.Equal(2, File.ReadAllText(path).Split("type=\"4\"").Length);
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_RepairsTypeFourGameOutsideGamesBlock()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="106">
        <GAMES>
        <GAME day="1" time="1400" away="1" home="2" />
        </GAMES>
        <Game day="106" time="1830" away="0" home="0" type="4" />
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        var text = File.ReadAllText(path);
        Assert.True(repaired);
        Assert.True(text.IndexOf("type=\"4\"", StringComparison.Ordinal) < text.IndexOf("day=\"1\"", StringComparison.Ordinal));
        Assert.Equal(2, text.Split("type=\"4\"").Length);
    }

    [Fact]
    public void EnsureKboScheduleAllstarGameLine_SkipsZeroAllstarDay()
    {
        Directory.CreateDirectory(tempDir);
        var path = Path.Combine(tempDir, "korean_baseball_organization_int_c_2026.lsdl");
        File.WriteAllText(path, """
        <SCHEDULE start_month="3" start_day="28" allstar_game_day="0">
        <GAMES>
        </GAMES>
        </SCHEDULE>
        """);

        var repaired = global::KboSeedFiles.EnsureKboScheduleAllstarGameLine(path);

        Assert.False(repaired);
        Assert.DoesNotContain("type=\"4\"", File.ReadAllText(path));
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
