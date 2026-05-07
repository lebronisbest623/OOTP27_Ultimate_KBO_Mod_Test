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
            "enable_foreign_waiver_ai": "enabled",
            "enable_single_division_allstar_events.txt": false
          }
        }
        """);

        Assert.True(global::KboFlags.ReadKboFlag(ConfigPath, "enable_foreign_waiver_ai.txt"));
        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_single_division_allstar_events.txt"));
    }

    [Fact]
    public void WriteKboFlagValue_UpdatesOneJsonFileAndPreservesExistingFlags()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true,
          "intl_established_fa_multiplier": 10
        }
        """);

        global::KboFlags.WriteKboFlagValue(ConfigPath, "enable_foreign_waiver_ai.txt", true);
        global::KboFlags.WriteKboFlagValue(ConfigPath, "disable_foreign_injury_replacement.txt", false);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_launcher_injection"]);
        Assert.True(flags["enable_foreign_waiver_ai"]);
        Assert.False(flags["disable_foreign_injury_replacement"]);
        Assert.Equal(10, global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 1, 1, 20));
    }

    [Fact]
    public void WriteKboIntSetting_UpdatesOneJsonFileAndPreservesBooleanFlags()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        global::KboFlags.WriteKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 99, 1, 20);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_launcher_injection"]);
        Assert.Equal(20, global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 1, 1, 20));
    }

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void WriteKboSingleDivisionAllstarEventsFlag_UpdatesAllNativeGateFlags(bool enabled)
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        global::KboFlags.WriteKboSingleDivisionAllstarEventsFlag(ConfigPath, enabled);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_launcher_injection"]);
        Assert.Equal(enabled, flags["enable_single_division_allstar_runtime_patches"]);
        Assert.Equal(enabled, flags["enable_single_division_allstar_settings_patch"]);
        Assert.Equal(enabled, flags["enable_single_division_allstar_voting_hook"]);
        Assert.Equal(enabled, flags["enable_single_division_allstar_events"]);
    }

    [Fact]
    public void ImportLegacyKboFlagFilesIfMissing_CopiesKnownBooleanFlagsOnly()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(Path.Combine(tempDir, "enable_launcher_injection.txt"), "1");
        File.WriteAllText(Path.Combine(tempDir, "enable_experimental_runtime_hooks.txt"), "enabled");
        File.WriteAllText(Path.Combine(tempDir, "disable_foreign_injury_replacement.txt"), "off");
        File.WriteAllText(Path.Combine(tempDir, "enable_player_profile_ocr_click_v1.txt"), "1");
        File.WriteAllText(Path.Combine(tempDir, "kbo_league_id.txt"), "100");
        File.WriteAllText(Path.Combine(tempDir, "foreign_waiver_negotiation_window.txt"), "2026");

        global::KboFlags.ImportLegacyKboFlagFilesIfMissing(ConfigPath);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_launcher_injection"]);
        Assert.True(flags["enable_experimental_runtime_hooks"]);
        Assert.False(flags["disable_foreign_injury_replacement"]);
        Assert.False(flags.ContainsKey("enable_player_profile_ocr_click_v1"));
        Assert.False(flags.ContainsKey("kbo_league_id"));
        Assert.False(flags.ContainsKey("foreign_waiver_negotiation_window"));
    }

    [Fact]
    public void ImportLegacyKboFlagFilesIfMissing_DoesNotOverwriteJsonValues()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": false
        }
        """);
        File.WriteAllText(Path.Combine(tempDir, "enable_launcher_injection.txt"), "1");

        global::KboFlags.ImportLegacyKboFlagFilesIfMissing(ConfigPath);

        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_launcher_injection.txt"));
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
    [InlineData("10", 10)]
    [InlineData("\" 5 \"", 5)]
    [InlineData("0", 1)]
    [InlineData("99", 20)]
    public void ReadKboIntSetting_ReadsAndClampsSupportedValues(string jsonValue, int expected)
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, $$"""
        {
          "intl_established_fa_multiplier": {{jsonValue}}
        }
        """);

        Assert.Equal(expected, global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 1, 1, 20));
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
