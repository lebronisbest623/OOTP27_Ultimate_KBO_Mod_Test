namespace KBOLauncher.Tests;

using Xunit;

public sealed class LauncherPathsTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launcher-path-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void ResolveExistingNewestPath_DeduplicatesCandidatesAndChoosesNewestExistingFile()
    {
        var older = Path.Combine(tempDir, "older", "ootp27.exe");
        var newest = Path.Combine(tempDir, "newest", "ootp27.exe");
        Directory.CreateDirectory(Path.GetDirectoryName(older)!);
        Directory.CreateDirectory(Path.GetDirectoryName(newest)!);
        File.WriteAllText(older, "old");
        File.WriteAllText(newest, "new");
        File.SetLastWriteTimeUtc(older, new DateTime(2026, 5, 1, 0, 0, 0, DateTimeKind.Utc));
        File.SetLastWriteTimeUtc(newest, new DateTime(2026, 5, 2, 0, 0, 0, DateTimeKind.Utc));

        var resolved = global::LauncherPaths.ResolveExistingNewestPath([
            older,
            older.ToUpperInvariant(),
            Path.Combine(tempDir, "missing", "ootp27.exe"),
            newest
        ]);

        Assert.Equal(newest, resolved);
    }

    [Fact]
    public void ResolveExistingNewestPath_ReturnsNullWhenNoCandidateExists()
    {
        Assert.Null(global::LauncherPaths.ResolveExistingNewestPath([
            Path.Combine(tempDir, "missing", "ootp27.exe")
        ]));
    }

    [Fact]
    public void ReadSteamLibraryFolders_ParsesExistingPathEntriesAndSkipsMissingOnes()
    {
        var libraryA = Path.Combine(tempDir, "SteamLibraryA");
        var libraryB = Path.Combine(tempDir, "SteamLibraryB");
        Directory.CreateDirectory(libraryA);
        Directory.CreateDirectory(libraryB);
        var vdf = Path.Combine(tempDir, "libraryfolders.vdf");
        File.WriteAllText(vdf, $$"""
        "libraryfolders"
        {
            "0"
            {
                "path" "{{libraryA.Replace(@"\", @"\\")}}"
            }
            "1"
            {
                "path" "{{Path.Combine(tempDir, "MissingLibrary").Replace(@"\", @"\\")}}"
            }
            "2"
            {
                "path" "{{libraryB.Replace(@"\", @"\\")}}"
            }
        }
        """);

        var libraries = global::LauncherPaths.ReadSteamLibraryFolders(vdf).ToList();

        Assert.Equal([libraryA, libraryB], libraries);
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
