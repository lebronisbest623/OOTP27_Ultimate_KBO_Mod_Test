namespace KBOLauncher.Tests;

using System.Text.Json;
using System.Text.RegularExpressions;
using Xunit;

public sealed class OotpSupportedBuildManifestTests
{
    [Fact]
    public void SupportedBuildManifest_MatchesGeneratedManagedAndNativeLists()
    {
        var manifestBuilds = ReadManifestBuilds();
        var managedBuilds = global::OotpSupportedBuilds.All
            .Select(build => new SupportedBuildRow(build.Timestamp, build.SizeOfImage, build.Label, build.ExperimentalSignature))
            .ToArray();
        var nativeBuilds = ReadNativeGeneratedBuilds();

        Assert.Equal(manifestBuilds, managedBuilds);
        Assert.Equal(manifestBuilds.Select(build => build.NativeRow).ToArray(), nativeBuilds);
    }

    private static SupportedBuildRow[] ReadManifestBuilds()
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(RepoPath("config", "ootp-supported-builds.json")));
        return doc.RootElement.GetProperty("builds")
            .EnumerateArray()
            .Select(build => new SupportedBuildRow(
                ParseHexUInt32(build.GetProperty("timestamp").GetString()!),
                ParseHexUInt32(build.GetProperty("sizeOfImage").GetString()!),
                build.GetProperty("label").GetString()!,
                build.TryGetProperty("experimentalSignature", out var experimentalSignature) && experimentalSignature.GetBoolean()))
            .ToArray();
    }

    private static NativeBuildRow[] ReadNativeGeneratedBuilds()
    {
        var text = File.ReadAllText(RepoPath("native", "src", "build_verify", "supported_builds.generated.inc"));
        return Regex.Matches(
                text,
                "\\{0x(?<timestamp>[0-9A-Fa-f]{8})u, 0x(?<size>[0-9A-Fa-f]{8})u, \"(?<label>[^\"]+)\"\\},")
            .Select(match => new NativeBuildRow(
                Convert.ToUInt32(match.Groups["timestamp"].Value, 16),
                Convert.ToUInt32(match.Groups["size"].Value, 16),
                match.Groups["label"].Value))
            .ToArray();
    }

    private static uint ParseHexUInt32(string value)
    {
        Assert.Matches("^0x[0-9A-Fa-f]{1,8}$", value);
        return Convert.ToUInt32(value[2..], 16);
    }

    private static string RepoPath(params string[] parts)
    {
        var dir = AppContext.BaseDirectory;
        while (dir is not null)
        {
            var candidate = Path.Combine(new[] { dir }.Concat(parts).ToArray());
            if (File.Exists(candidate))
            {
                return candidate;
            }
            dir = Directory.GetParent(dir)?.FullName;
        }

        throw new FileNotFoundException("Could not find repository file.", Path.Combine(parts));
    }

    private sealed record SupportedBuildRow(uint Timestamp, uint SizeOfImage, string Label, bool ExperimentalSignature)
    {
        public NativeBuildRow NativeRow => new(Timestamp, SizeOfImage, Label);
    }

    private sealed record NativeBuildRow(uint Timestamp, uint SizeOfImage, string Label);
}
