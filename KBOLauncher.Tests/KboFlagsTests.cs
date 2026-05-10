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
    public async Task WriteKboFlagValue_ConcurrentWritesKeepJsonValidAndPreserveKeys()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        var tasks = Enumerable.Range(0, 32)
            .Select(index => Task.Run(() =>
                global::KboFlags.WriteKboFlagValue(ConfigPath, $"enable_concurrent_flag_{index}.txt", index % 2 == 0)))
            .ToArray();

        await Task.WhenAll(tasks);

        using var doc = JsonDocument.Parse(File.ReadAllText(ConfigPath));
        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_launcher_injection"]);
        for (var index = 0; index < 32; index++)
        {
            Assert.Equal(index % 2 == 0, flags[$"enable_concurrent_flag_{index}"]);
        }
    }

    [Fact]
    public void WriteKboFlagValue_MalformedJsonRecoversToSafeValidConfig()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, "{ nope");

        global::KboFlags.WriteKboFlagValue(ConfigPath, "enable_foreign_waiver_ai.txt", true);

        using var doc = JsonDocument.Parse(File.ReadAllText(ConfigPath));
        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_foreign_waiver_ai"]);
        Assert.False(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt"));
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
    public void EnsureDefaultKboRuntimeFlags_SeedsFreshInstallCoreFeatureFlags()
    {
        global::KboFlags.EnsureDefaultKboRuntimeFlags(ConfigPath);

        var flags = global::KboFlags.ReadKboFlagConfig(ConfigPath);
        Assert.True(flags["enable_experimental_runtime_hooks"]);
        Assert.True(flags["enable_foreign_waiver_ai"]);
        Assert.True(flags["enable_foreign_waiver_background_scanner"]);
        Assert.False(flags["enable_single_division_allstar_runtime_patches"]);
        Assert.False(flags["enable_single_division_allstar_settings_patch"]);
        Assert.False(flags["enable_single_division_allstar_voting_hook"]);
        Assert.False(flags["enable_single_division_allstar_events"]);
        Assert.True(flags["enable_kbo_foreign_trade_check_patch"]);
        Assert.True(flags["enable_kbo_ai_fa_fallback_patch"]);
        Assert.True(flags["enable_kbo_player_team_signability_patch"]);
        Assert.True(flags["enable_kbo_offer_eligibility_patch"]);
        Assert.True(flags["enable_kbo_callup_foreign_limit_patch"]);
        Assert.True(flags["enable_intl_established_fa_quality_probe_patch"]);
        Assert.False(flags["enable_kbo_season_phase_monitor"]);
        Assert.False(flags["disable_kbo_fa_salary_opening_day_snapshot"]);
        Assert.False(flags["disable_kbo_no_minor_contract_experimental_patch"]);
    }

    [Fact]
    public void EnsureDefaultKboRuntimeFlags_DoesNotOverwriteExistingOptOuts()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_kbo_foreign_trade_check_patch": false,
          "enable_experimental_runtime_hooks": false
        }
        """);

        global::KboFlags.EnsureDefaultKboRuntimeFlags(ConfigPath);

        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_kbo_foreign_trade_check_patch.txt"));
        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_experimental_runtime_hooks.txt"));
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
        Assert.False(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt"));

        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, "{ nope");

        Assert.Empty(global::KboFlags.ReadKboFlagConfig(ConfigPath));
        Assert.False(global::KboFlags.ReadKboFlag(ConfigPath, "enable_launcher_injection.txt"));
        Assert.False(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt"));
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_EnableLauncherInjectionMissing_ReturnsFalse()
    {
        Assert.False(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt"));
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_EnableLauncherInjectionMalformedConfig_ReturnsFalse()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, "{ nope");

        Assert.False(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt"));
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_EnableLauncherInjectionExplicitTrue_ReturnsTrue()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_launcher_injection": true
        }
        """);

        Assert.True(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_launcher_injection.txt"));
    }

    [Fact]
    public void ReadKboFlagDefaultEnabled_OnlyDisablesWhenConfigExplicitlySaysFalse()
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, """
        {
          "enable_foreign_waiver_ai": true
        }
        """);

        Assert.True(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "enable_foreign_waiver_ai.txt"));
        Assert.True(global::KboFlags.ReadKboFlagDefaultEnabled(ConfigPath, "missing_flag.txt"));
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
    [InlineData("\"nope\"")]
    [InlineData("true")]
    [InlineData("{}")]
    public void ReadKboIntSetting_IgnoresUnsupportedValues(string jsonValue)
    {
        Directory.CreateDirectory(tempDir);
        File.WriteAllText(ConfigPath, $$"""
        {
          "intl_established_fa_multiplier": {{jsonValue}}
        }
        """);

        Assert.Equal(7, global::KboFlags.ReadKboIntSetting(ConfigPath, "intl_established_fa_multiplier", 7, 1, 20));
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
