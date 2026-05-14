namespace KBOLauncher.Tests;

using FluentAssertions;
using Xunit;

public sealed class ProcessDiscoveryTests
{
    [Fact]
    public void ResolveSingleExistingProcess_ReturnsOnlyWhenExactlyOneCandidateExists()
    {
        global::ProcessDiscovery.ResolveSingleExistingProcess([]).Should().BeNull();
        global::ProcessDiscovery.ResolveSingleExistingProcess([new(42, "ootp27", @"C:\ootp27.exe")]).Should().Be(42);
        global::ProcessDiscovery.ResolveSingleExistingProcess([
            new(42, "ootp27", @"C:\ootp27.exe"),
            new(43, "ootp27", @"D:\ootp27.exe")
        ]).Should().BeNull();
    }

    [Fact]
    public void PathEquals_NormalizesAndComparesCaseInsensitively()
    {
        var path = Path.Combine(Path.GetTempPath(), "OOTP27", "ootp27.exe");

        global::InjectionTargetResolver.PathEquals(path, path.ToUpperInvariant()).Should().BeTrue();
        global::InjectionTargetResolver.PathEquals("", path).Should().BeFalse();
    }
}
