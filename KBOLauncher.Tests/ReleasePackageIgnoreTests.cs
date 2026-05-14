namespace KBOLauncher.Tests;

using Xunit;

public sealed class ReleasePackageIgnoreTests
{
    [Theory]
    [InlineData("dist/")]
    [InlineData("release_artifacts/")]
    [InlineData("native/bin/")]
    [InlineData("*.dll")]
    [InlineData("*.exe")]
    [InlineData("*.pdb")]
    [InlineData("TestResults/")]
    public void GitIgnore_ExcludesReleaseNativeAndTestOutputs(string requiredPattern)
    {
        var gitignore = File.ReadAllLines(Path.Combine(FindRepoRoot(), ".gitignore"));

        Assert.Contains(requiredPattern, gitignore);
    }

    [Theory]
    [InlineData("\"KBOLauncher.exe\"")]
    [InlineData("\"KBOFix.dll\"")]
    [InlineData("\"WebView2Loader.dll\"")]
    [InlineData("\"kbo_league_id.txt\"")]
    [InlineData("\"assets\\fonts\\JejuGothic-Regular.ttf\"")]
    [InlineData("\"assets\\icons\\github-mark.png\"")]
    [InlineData("\"data\\seeds\\allstar_teams.csv\"")]
    [InlineData("\"data\\seeds\\amateur_player_quality_policy.json\"")]
    [InlineData("\"data\\seeds\\asian_games_projected_hosts.csv\"")]
    [InlineData("\"data\\seeds\\asian_games_projected_policy.json\"")]
    [InlineData("\"data\\seeds\\asian_games_roster_policy.json\"")]
    [InlineData("\"data\\seeds\\asian_games_schedule_seed.csv\"")]
    [InlineData("\"data\\seeds\\captain_selection_policy.json\"")]
    [InlineData("\"data\\seeds\\captain_seed.csv\"")]
    [InlineData("\"data\\seeds\\cbt_player_team_seasons_seed.csv\"")]
    [InlineData("\"data\\seeds\\cbt_rules.json\"")]
    [InlineData("\"data\\seeds\\college_reputation_seed.csv\"")]
    [InlineData("\"data\\seeds\\economic_defaults.json\"")]
    [InlineData("\"data\\seeds\\fa_compensation_policy.json\"")]
    [InlineData("\"data\\seeds\\fa_requalification_policy.json\"")]
    [InlineData("\"data\\seeds\\fa_market_policy.json\"")]
    [InlineData("\"data\\seeds\\fa_rules.json\"")]
    [InlineData("\"data\\seeds\\foreign_injury_replacements_seed.csv\"")]
    [InlineData("\"data\\seeds\\foreign_player_policy.json\"")]
    [InlineData("\"data\\seeds\\high_school_reputation_seed.csv\"")]
    [InlineData("\"data\\seeds\\intl_established_fa_policy.json\"")]
    [InlineData("\"data\\seeds\\kbo_team_policy.json\"")]
    [InlineData("\"data\\seeds\\military_service_policy.json\"")]
    [InlineData("\"data\\seeds\\military_service_seed.csv\"")]
    [InlineData("\"data\\seeds\\runtime_tuning_policy.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\asian_games.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\captain.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\competitive_balance_tax.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\custom_events.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\fa_compensation.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\foreign_injury.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\foreign_waiver.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\en\\military_service.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\asian_games.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\captain.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\competitive_balance_tax.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\custom_events.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\fa_compensation.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\foreign_injury.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\foreign_waiver.json\"")]
    [InlineData("\"data\\seeds\\news_templates\\ko\\military_service.json\"")]
    [InlineData("\"data\\seeds\\ui_text\\en\\hotkey_window.json\"")]
    [InlineData("\"data\\seeds\\ui_text\\ko\\hotkey_window.json\"")]
    public void ReleaseScript_ValidatesRequiredPayloadFiles(string requiredFileLiteral)
    {
        var script = File.ReadAllText(Path.Combine(FindRepoRoot(), "scripts", "release.ps1"));

        Assert.Contains(requiredFileLiteral, script);
    }

    private static string FindRepoRoot()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            if (File.Exists(Path.Combine(dir.FullName, "OOTP27-KBO-Launcher.sln")))
            {
                return dir.FullName;
            }

            dir = dir.Parent;
        }

        throw new InvalidOperationException("Could not find repository root.");
    }
}
