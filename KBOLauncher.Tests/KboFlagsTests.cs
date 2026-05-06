namespace KBOLauncher.Tests;

using System.Text.Json;
using Xunit;

public sealed class KboFlagsTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launcher-tests", Guid.NewGuid().ToString("N"));

    private string ConfigPath => Path.Combine(tempDir, "kbo_flags.json");

    [Fact]
    public void NormalizeKboFlagKey_StripsDirectoryAndTxtExtension()
    {
        Assert.Equal("enable_foreign_waiver_ai", global::KboFlags.NormalizeKboFlagKey(@"C:\tmp\enable_foreign_waiver_ai.txt"));
        Assert.Equal("disable_foreign_injury_replacement", global::KboFlags.NormalizeKboFlagKey("disable_foreign_injury_replacement"));
    }

    [Fact]
    public void ReadKboFlagConfig_ReadsFlatJsonAndNormalizesLegacyTxtKeys()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection.txt": "yes",
          "enable_foreign_waiver_ai": true,
          "disable_foreign_injury_replacement": 0,
          "ignored": "maybe"
        }
        """);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);

        Assert.True(flags["enable_launcher_injection"]);
        Assert.True(flags["enable_foreign_waiver_ai"]);
        Assert.False(flags["disable_foreign_injury_replacement"]);
        Assert.False(flags.ContainsKey("enable_launcher_injection.txt"));
        Assert.False(flags.ContainsKey("ignored"));
    }

    [Fact]
    public void ReadKboFlagConfig_ReadsNestedFlagsObject()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "flags": {
            "enable_military_draft_pool": "enabled",
            "enable_single_division_allstar_events.txt": false
          }
        }
        """);

        Assert.True(global::KboFlags.ReadKboFlag(ConfigPath, "enable_military_draft_pool.txt"));
        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_single_division_allstar_events.txt"));
    }

    [Fact]
    public void WriteKboFlagValue_UpdatesOneJsonFileAndPreservesExistingFlags()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        global::KboFlags.WriteKboFlagValue(ConfigPath, "enable_foreign_waiver_ai.txt", true);
        global::KboFlags.WriteKboFlagValue(ConfigPath, "disable_foreign_injury_replacement.txt", false);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_launcher_injection"]);
        Assert.True(flags["enable_foreign_waiver_ai"]);
        Assert.False(flags["disable_foreign_injury_replacement"]);
    }

    [Fact]
    public void ReadKboFlagConfig_MissingOrMalformedFileFailsClosed()
    {
        Assert.Empty(global::KboFlags.ReadKboFlagConfig(ConfigPath));
        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_launcher_injection.txt"));

        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, "{ nope");

        Assert.Empty(global::KboFlags.ReadKboFlagConfig(ConfigPath));
        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_launcher_injection.txt"));
    }

    [Theory]
    [InlineData("true", true)]
    [InlineData("false", false)]
    [InlineData("1", true)]
    [InlineData("0", false)]
    [InlineData("-2", true)]
    [InlineData("\"YES\"", true)]
    [InlineData("\"off\"", false)]
    [InlineData("\" enabled \"", true)]
    public void TryReadJsonBool_AcceptsSupportedBooleanForms(string json, bool expected)
    {
        using var doc = JsonDocument.Parse(json);

        Assert.True(global::KboFlags.TryReadJsonBool(doc.RootElement, out var actual));
        Assert.Equal(expected, actual);
    }

    [Theory]
    [InlineData("{}")]
    [InlineData("[]")]
    [InlineData("\"maybe\"")]
    [InlineData("1.5")]
    public void TryReadJsonBool_RejectsUnsupportedValues(string json)
    {
        using var doc = JsonDocument.Parse(json);

        Assert.False(global::KboFlags.TryReadJsonBool(doc.RootElement, out _));
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
