namespace KBOLauncher.Tests;

using System.Text.Json;

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
    [InlineData("\"assets\\fonts\\JejuGothic-OFL.txt\"")]
    [InlineData("\"assets\\icons\\github-mark.png\"")]
    [InlineData("\"tools\\kbo_optimizer.exe\"")]
    [InlineData("\"tools\\kbo_optimizer.py\"")]
    public void ReleaseScript_ValidatesRequiredNonSeedPayloadFiles(string requiredFileLiteral)
    {
        var script = File.ReadAllText(Path.Combine(FindRepoRoot(), "scripts", "release.ps1"));

        Assert.Contains(requiredFileLiteral, script);
    }

    [Fact]
    public void ReleaseValidation_UsesSeedManifest()
    {
        var root = FindRepoRoot();
        var releaseScript = File.ReadAllText(Path.Combine(root, "scripts", "release.ps1"));
        var validationScript = File.ReadAllText(Path.Combine(root, "tests", "release", "verify-release-artifact.ps1"));

        Assert.Contains("seed_manifest.json", releaseScript);
        Assert.Contains("seed_manifest.json", validationScript);
        Assert.Contains("Get-SeedManifestPayloadFiles", releaseScript);
        Assert.Contains("Get-SeedManifestPayloadFiles", validationScript);
    }

    [Fact]
    public void SeedManifest_ListsEveryJsonAndCsvSourceFile()
    {
        var root = FindRepoRoot();
        var seedRoot = Path.Combine(root, "data", "seeds");
        var manifestPath = Path.Combine(seedRoot, "seed_manifest.json");

        using var document = JsonDocument.Parse(File.ReadAllText(manifestPath));
        var manifestFiles = document.RootElement
            .GetProperty("groups")
            .EnumerateArray()
            .SelectMany(group => group.GetProperty("files").EnumerateArray())
            .ToArray();
        var listedSources = manifestFiles
            .Select(file => GetManifestRelativePath(file, "source", fallbackPropertyName: "path"))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var listedTargets = manifestFiles
            .Select(file => GetManifestRelativePath(file, "path"))
            .ToArray();

        var actual = Directory.EnumerateFiles(seedRoot, "*", SearchOption.AllDirectories)
            .Where(path => path.EndsWith(".json", StringComparison.OrdinalIgnoreCase)
                || path.EndsWith(".csv", StringComparison.OrdinalIgnoreCase))
            .Select(path => Path.GetRelativePath(seedRoot, path))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        Assert.Empty(actual.Except(listedSources, StringComparer.OrdinalIgnoreCase));
        Assert.Empty(listedSources.Except(actual, StringComparer.OrdinalIgnoreCase));
        Assert.Equal(listedTargets.Length, listedTargets.Distinct(StringComparer.OrdinalIgnoreCase).Count());
    }

    private static string GetManifestRelativePath(
        JsonElement file,
        string propertyName,
        string? fallbackPropertyName = null)
    {
        if (!file.TryGetProperty(propertyName, out var property)
            || property.ValueKind is JsonValueKind.Null
            || string.IsNullOrWhiteSpace(property.GetString()))
        {
            if (fallbackPropertyName is null)
            {
                throw new InvalidOperationException($"Missing manifest file property: {propertyName}");
            }

            property = file.GetProperty(fallbackPropertyName);
        }

        return property.GetString()!.Replace('/', Path.DirectorySeparatorChar);
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
