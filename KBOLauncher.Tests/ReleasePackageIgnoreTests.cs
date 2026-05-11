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
    [InlineData("\"data\\seeds\\asian_games_projected_hosts.csv\"")]
    [InlineData("\"data\\seeds\\asian_games_schedule_seed.csv\"")]
    [InlineData("\"data\\seeds\\college_reputation_seed.csv\"")]
    [InlineData("\"data\\seeds\\fa_rules.json\"")]
    [InlineData("\"data\\seeds\\foreign_replacement_players_seed.csv\"")]
    [InlineData("\"data\\seeds\\high_school_reputation_seed.csv\"")]
    [InlineData("\"data\\seeds\\military_service_seed.csv\"")]
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
