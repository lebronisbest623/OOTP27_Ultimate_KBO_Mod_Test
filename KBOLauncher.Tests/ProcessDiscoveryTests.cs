namespace KBOLauncher.Tests;

using Xunit;

public sealed class ProcessDiscoveryTests
{
    [Fact]
    public void ResolveSingleExistingProcess_ReturnsOnlyWhenExactlyOneCandidateExists()
    {
        Assert.Null(global::ProcessDiscovery.ResolveSingleExistingProcess([]));
        Assert.Equal(42, global::ProcessDiscovery.ResolveSingleExistingProcess([new(42, "ootp27", @"C:\ootp27.exe")]));
        Assert.Null(global::ProcessDiscovery.ResolveSingleExistingProcess([
            new(42, "ootp27", @"C:\ootp27.exe"),
            new(43, "ootp27", @"D:\ootp27.exe")
        ]));
    }

    [Fact]
    public void PathEquals_NormalizesAndComparesCaseInsensitively()
    {
        var path = Path.Combine(Path.GetTempPath(), "OOTP27", "ootp27.exe");

        Assert.True(global::InjectionTargetResolver.PathEquals(path, path.ToUpperInvariant()));
        Assert.False(global::InjectionTargetResolver.PathEquals("", path));
    }
}
