namespace KBOLauncher.Tests;

using Xunit;

public sealed class LauncherPathsTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launcher-path-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void ResolveOotpPath_UsesExplicitPathEvenWhenAnotherCandidateIsNewer()
    {
        var explicitPath = Path.Combine(tempDir, "explicit", "ootp27.exe");
        var newest = Path.Combine(tempDir, "newest", "ootp27.exe");
        var oldEnvExe = Environment.GetEnvironmentVariable("OOTP27_EXE");
        Directory.CreateDirectory(Path.GetDirectoryName(explicitPath)!);
        Directory.CreateDirectory(Path.GetDirectoryName(newest)!);
        File.WriteAllText(explicitPath, "explicit");
        File.WriteAllText(newest, "new");
        File.SetLastWriteTimeUtc(explicitPath, new DateTime(2026, 5, 1, 0, 0, 0, DateTimeKind.Utc));
        File.SetLastWriteTimeUtc(newest, new DateTime(2026, 5, 2, 0, 0, 0, DateTimeKind.Utc));

        try
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", newest);

            var resolved = global::LauncherPaths.ResolveOotpPath(explicitPath);

            Assert.Equal(explicitPath, resolved);
        }
        finally
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", oldEnvExe);
        }
    }

    [Fact]
    public void ResolveOotpPath_UsesAutoDiscoveryWhenExplicitPathDoesNotExist()
    {
        var envExe = Path.Combine(tempDir, "env", "ootp27.exe");
        var oldEnvExe = Environment.GetEnvironmentVariable("OOTP27_EXE");
        Directory.CreateDirectory(Path.GetDirectoryName(envExe)!);
        File.WriteAllText(envExe, "env");

        try
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", envExe);

            var resolved = global::LauncherPaths.ResolveOotpPath(Path.Combine(tempDir, "missing", "ootp27.exe"));

            Assert.Equal(envExe, resolved);
        }
        finally
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", oldEnvExe);
        }
    }

    [Fact]
    public void ResolveOotpPath_ReturnsNullWhenExplicitPathIsDirectory()
    {
        var explicitDirectory = Path.Combine(tempDir, "ootp27.exe");
        var oldEnvExe = Environment.GetEnvironmentVariable("OOTP27_EXE");
        Directory.CreateDirectory(explicitDirectory);

        try
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", null);

            var resolved = global::LauncherPaths.ResolveOotpPath(explicitDirectory);

            Assert.Null(resolved);
        }
        finally
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", oldEnvExe);
        }
    }

    [Fact]
    public void ResolveOotpPath_ReturnsNullWhenExplicitPathIsNotOotpExe()
    {
        var explicitPath = Path.Combine(tempDir, "not-ootp.exe");
        var oldEnvExe = Environment.GetEnvironmentVariable("OOTP27_EXE");
        Directory.CreateDirectory(Path.GetDirectoryName(explicitPath)!);
        File.WriteAllText(explicitPath, "not ootp");

        try
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", null);

            var resolved = global::LauncherPaths.ResolveOotpPath(explicitPath);

            Assert.Null(resolved);
        }
        finally
        {
            Environment.SetEnvironmentVariable("OOTP27_EXE", oldEnvExe);
        }
    }

    [Fact]
    public void ResolveExistingNewestPath_DeduplicatesAutoCandidatesAndChoosesNewestExistingFile()
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
    public void GetOotpPathCandidates_IncludesOfficialWebsiteInstallLocations()
    {
        var candidates = global::LauncherPaths.GetOotpPathCandidates(null).ToList();

        Assert.Contains(candidates, path =>
            path.EndsWith(
                @"Out of the Park Developments\OOTP Baseball 27\ootp27.exe",
                StringComparison.OrdinalIgnoreCase));
        Assert.Contains(candidates, path =>
            path.EndsWith(
                @"Out of the Park Developments\Out of the Park Baseball 27\ootp27.exe",
                StringComparison.OrdinalIgnoreCase));
        Assert.Contains(candidates, path =>
            path.EndsWith(
                @"OOTP 27\ootp27.exe",
                StringComparison.OrdinalIgnoreCase));
    }

    [Fact]
    public void GetOotpPathCandidates_UsesEnvironmentDirectoryAndExeOverrides()
    {
        var envDir = Path.Combine(tempDir, "OfficialInstall");
        var envExe = Path.Combine(tempDir, "Portable", "ootp27.exe");
        var oldDir = Environment.GetEnvironmentVariable("OOTP27_DIR");
        var oldExe = Environment.GetEnvironmentVariable("OOTP27_EXE");
        try
        {
            Environment.SetEnvironmentVariable("OOTP27_DIR", envDir);
            Environment.SetEnvironmentVariable("OOTP27_EXE", envExe);

            var candidates = global::LauncherPaths.GetOotpPathCandidates(null).ToList();

            Assert.Contains(Path.Combine(envDir, "ootp27.exe"), candidates);
            Assert.Contains(envExe, candidates);
        }
        finally
        {
            Environment.SetEnvironmentVariable("OOTP27_DIR", oldDir);
            Environment.SetEnvironmentVariable("OOTP27_EXE", oldExe);
        }
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
