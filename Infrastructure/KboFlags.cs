using System.Text.Json;
using System.Text.Json.Nodes;
using static LauncherPaths;

internal static class KboFlags
{
    private static readonly string[] SingleDivisionAllstarFlagFiles =
    [
        "enable_single_division_allstar_runtime_patches.txt",
        "enable_single_division_allstar_settings_patch.txt",
        "enable_single_division_allstar_voting_hook.txt",
        "enable_single_division_allstar_events.txt",
    ];

    private static readonly HashSet<string> LegacyImportFlagKeys = new(StringComparer.OrdinalIgnoreCase)
    {
        "disable_foreign_injury_replacement",
        "disable_foreign_waiver_legacy_auto_detector",
        "disable_kbo_ai_fa_status_candidate_insert_hook",
        "disable_kbo_custom_foreign_policy",
        "disable_kbo_foreign_signing_branch_patch",
        "disable_kbo_foreign_trade_check_patch",
        "disable_kbo_sangmu_fa_block_core",
        "disable_kbo_submit_offer_probe_patch",
        "enable_experimental_runtime_hooks",
        "enable_fa_requalification",
        "enable_foreign_waiver_ai",
        "enable_foreign_waiver_background_scanner",
        "enable_foreign_waiver_event_probe",
        "enable_kbo_ai_fa_fallback_patch",
        "enable_kbo_asian_quota_probe_logs",
        "enable_kbo_callup_foreign_limit_patch",
        "enable_kbo_custom_foreign_offer_logs",
        "enable_kbo_diagnostic_minimal_runtime",
        "enable_kbo_fa_signability_hooks",
        "enable_kbo_foreign_trade_check_patch",
        "enable_kbo_fix",
        "enable_kbo_offer_eligibility_patch",
        "enable_kbo_player_team_signability_patch",
        "enable_kbo_sangmu_offer_only",
        "enable_kbo_sangmu_signability_only",
        "enable_kbo_season_phase_monitor",
        "enable_kbo_submit_offer_probe_patch",
        "enable_launcher_injection",
        "enable_single_division_allstar_events",
        "enable_single_division_allstar_runtime_patches",
        "enable_single_division_allstar_settings_patch",
        "enable_single_division_allstar_voting_hook",
    };

    public static void WriteKboFlag(string fileName, string label, bool enabled)
    {
        var key = NormalizeKboFlagKey(fileName);
        var path = GetKboFlagConfigPath();
        WriteKboFlagValue(path, key, enabled);
        Console.WriteLine($"{label}: {path} {key} = {(enabled ? "enabled" : "disabled")}");
    }

    internal static void WriteKboFlagValue(string configPath, string fileName, bool enabled)
    {
        WriteKboFlagValues(configPath, [fileName], enabled);
    }

    internal static void WriteKboFlagValues(string configPath, IEnumerable<string> fileNames, bool enabled)
    {
        var flags = ReadKboRawConfig(configPath);
        foreach (var fileName in fileNames)
        {
            flags[NormalizeKboFlagKey(fileName)] = JsonValue.Create(enabled);
        }
        Directory.CreateDirectory(Path.GetDirectoryName(configPath)!);
        var json = JsonSerializer.Serialize(flags, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(configPath, json + Environment.NewLine);
    }

    public static void ImportLegacyKboFlagFilesIfMissing()
    {
        ImportLegacyKboFlagFilesIfMissing(GetKboFlagConfigPath());
    }

    public static void EnsureDefaultKboRuntimeFlags()
    {
        EnsureDefaultKboRuntimeFlags(GetKboFlagConfigPath());
    }

    internal static void EnsureDefaultKboRuntimeFlags(string configPath)
    {
        var raw = ReadKboRawConfig(configPath);
        var changed = false;

        changed |= EnsureMissingFlag(raw, "enable_experimental_runtime_hooks", true);
        changed |= EnsureMissingFlag(raw, "enable_foreign_waiver_ai", true);
        changed |= EnsureMissingFlag(raw, "enable_foreign_waiver_background_scanner", true);
        changed |= EnsureMissingFlag(raw, "enable_single_division_allstar_runtime_patches", true);
        changed |= EnsureMissingFlag(raw, "enable_single_division_allstar_settings_patch", true);
        changed |= EnsureMissingFlag(raw, "enable_single_division_allstar_voting_hook", true);
        changed |= EnsureMissingFlag(raw, "enable_single_division_allstar_events", true);
        changed |= EnsureMissingFlag(raw, "enable_kbo_foreign_trade_check_patch", true);
        changed |= EnsureMissingFlag(raw, "enable_kbo_ai_fa_fallback_patch", true);
        changed |= EnsureMissingFlag(raw, "enable_kbo_player_team_signability_patch", true);
        changed |= EnsureMissingFlag(raw, "enable_kbo_offer_eligibility_patch", true);
        changed |= EnsureMissingFlag(raw, "enable_kbo_callup_foreign_limit_patch", true);
        changed |= EnsureMissingFlag(raw, "enable_intl_established_fa_quality_probe_patch", true);
        changed |= EnsureMissingFlag(raw, "enable_kbo_season_phase_monitor", true);

        if (!changed)
        {
            return;
        }

        Directory.CreateDirectory(Path.GetDirectoryName(configPath)!);
        var json = JsonSerializer.Serialize(raw, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(configPath, json + Environment.NewLine);
    }

    private static bool EnsureMissingFlag(SortedDictionary<string, JsonNode?> flags, string key, bool value)
    {
        key = NormalizeKboFlagKey(key);
        if (flags.ContainsKey(key))
        {
            return false;
        }

        flags[key] = JsonValue.Create(value);
        return true;
    }

    internal static void ImportLegacyKboFlagFilesIfMissing(string configPath)
    {
        var configDir = Path.GetDirectoryName(configPath);
        if (string.IsNullOrWhiteSpace(configDir) || !Directory.Exists(configDir))
        {
            return;
        }

        var raw = ReadKboRawConfig(configPath);
        var changed = false;
        foreach (var path in Directory.EnumerateFiles(configDir, "*.txt", SearchOption.TopDirectoryOnly))
        {
            var fileName = Path.GetFileName(path);
            if (!fileName.StartsWith("enable_", StringComparison.OrdinalIgnoreCase)
                    && !fileName.StartsWith("disable_", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var key = NormalizeKboFlagKey(fileName);
            if (!LegacyImportFlagKeys.Contains(key))
            {
                continue;
            }
            if (raw.ContainsKey(key))
            {
                continue;
            }

            if (!TryReadLegacyFlagFile(path, out var value))
            {
                continue;
            }

            raw[key] = JsonValue.Create(value);
            changed = true;
        }

        if (!changed)
        {
            return;
        }

        Directory.CreateDirectory(configDir);
        var json = JsonSerializer.Serialize(raw, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(configPath, json + Environment.NewLine);
    }
    
    public static bool ReadKboFlag(string fileName)
    {
        return ReadKboFlag(GetKboFlagConfigPath(), fileName);
    }

    public static bool ReadKboFlagDefaultEnabled(string fileName)
    {
        return ReadKboFlagDefaultEnabled(GetKboFlagConfigPath(), fileName);
    }

    internal static bool ReadKboFlag(string configPath, string fileName)
    {
        return ReadKboFlagConfig(configPath).TryGetValue(NormalizeKboFlagKey(fileName), out var enabled)
            && enabled;
    }

    internal static bool ReadKboFlagDefaultEnabled(string configPath, string fileName)
    {
        return !ReadKboFlagConfig(configPath).TryGetValue(NormalizeKboFlagKey(fileName), out var enabled)
            || enabled;
    }

    public static SortedDictionary<string, bool> ReadKboFlagConfig()
    {
        return ReadKboFlagConfig(GetKboFlagConfigPath());
    }

    internal static SortedDictionary<string, bool> ReadKboFlagConfig(string configPath)
    {
        var flags = new SortedDictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(configPath))
        {
            return flags;
        }

        try
        {
            using var doc = JsonDocument.Parse(File.ReadAllText(configPath));
            var root = doc.RootElement;
            if (root.ValueKind == JsonValueKind.Object
                    && root.TryGetProperty("flags", out var nestedFlags)
                    && nestedFlags.ValueKind == JsonValueKind.Object)
            {
                root = nestedFlags;
            }
            if (root.ValueKind != JsonValueKind.Object)
            {
                return flags;
            }

            foreach (var property in root.EnumerateObject())
            {
                if (TryReadJsonBool(property.Value, out var value))
                {
                    flags[NormalizeKboFlagKey(property.Name)] = value;
                }
            }
        }
        catch
        {
            return flags;
        }

        return flags;
    }

    internal static SortedDictionary<string, JsonNode?> ReadKboRawConfig(string configPath)
    {
        var values = new SortedDictionary<string, JsonNode?>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(configPath))
        {
            return values;
        }

        try
        {
            var node = JsonNode.Parse(File.ReadAllText(configPath));
            var root = node as JsonObject;
            if (root is not null
                    && root.TryGetPropertyValue("flags", out var nestedFlags)
                    && nestedFlags is JsonObject nestedObject)
            {
                root = nestedObject;
            }
            if (root is null)
            {
                return values;
            }

            foreach (var property in root)
            {
                values[NormalizeKboFlagKey(property.Key)] = property.Value?.DeepClone();
            }
        }
        catch
        {
            return values;
        }

        return values;
    }

    public static bool TryReadJsonBool(JsonElement value, out bool result)
    {
        result = false;
        switch (value.ValueKind)
        {
            case JsonValueKind.True:
                result = true;
                return true;
            case JsonValueKind.False:
                return true;
            case JsonValueKind.Number:
                if (value.TryGetInt32(out var number))
                {
                    result = number != 0;
                    return true;
                }
                return false;
            case JsonValueKind.String:
                var text = value.GetString()?.Trim();
                return TryReadBooleanText(text, out result);
            default:
                return false;
        }
    }

    internal static bool TryReadBooleanText(string? text, out bool result)
    {
        result = false;
        text = text?.Trim();
        if (string.IsNullOrEmpty(text))
        {
            return false;
        }
        if (text.Equals("1", StringComparison.OrdinalIgnoreCase)
                || text.Equals("true", StringComparison.OrdinalIgnoreCase)
                || text.Equals("yes", StringComparison.OrdinalIgnoreCase)
                || text.Equals("on", StringComparison.OrdinalIgnoreCase)
                || text.Equals("enabled", StringComparison.OrdinalIgnoreCase))
        {
            result = true;
            return true;
        }
        if (text.Equals("0", StringComparison.OrdinalIgnoreCase)
                || text.Equals("false", StringComparison.OrdinalIgnoreCase)
                || text.Equals("no", StringComparison.OrdinalIgnoreCase)
                || text.Equals("off", StringComparison.OrdinalIgnoreCase)
                || text.Equals("disabled", StringComparison.OrdinalIgnoreCase))
        {
            result = false;
            return true;
        }
        return false;
    }

    public static int ReadKboIntlEstablishedFaMultiplier()
    {
        return ReadKboIntSetting(GetKboFlagConfigPath(), "intl_established_fa_multiplier", defaultValue: 20, minValue: 1, maxValue: 20);
    }

    internal static int ReadKboIntSetting(string configPath, string key, int defaultValue, int minValue, int maxValue)
    {
        if (!File.Exists(configPath))
        {
            return defaultValue;
        }

        try
        {
            using var doc = JsonDocument.Parse(File.ReadAllText(configPath));
            var root = doc.RootElement;
            if (root.ValueKind == JsonValueKind.Object
                    && root.TryGetProperty("flags", out var nestedFlags)
                    && nestedFlags.ValueKind == JsonValueKind.Object)
            {
                root = nestedFlags;
            }
            if (root.ValueKind != JsonValueKind.Object)
            {
                return defaultValue;
            }

            foreach (var property in root.EnumerateObject())
            {
                if (NormalizeKboFlagKey(property.Name).Equals(key, StringComparison.OrdinalIgnoreCase)
                        && TryReadJsonInt(property.Value, out var value))
                {
                    return Math.Clamp(value, minValue, maxValue);
                }
            }
        }
        catch
        {
            return defaultValue;
        }

        return defaultValue;
    }

    internal static void WriteKboIntSetting(string configPath, string key, int value, int minValue, int maxValue)
    {
        var values = ReadKboRawConfig(configPath);
        values[NormalizeKboFlagKey(key)] = JsonValue.Create(Math.Clamp(value, minValue, maxValue));
        Directory.CreateDirectory(Path.GetDirectoryName(configPath)!);
        var json = JsonSerializer.Serialize(values, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(configPath, json + Environment.NewLine);
    }

    public static bool TryReadJsonInt(JsonElement value, out int result)
    {
        result = 0;
        switch (value.ValueKind)
        {
            case JsonValueKind.Number:
                return value.TryGetInt32(out result);
            case JsonValueKind.String:
                return int.TryParse(value.GetString()?.Trim(), out result);
            default:
                return false;
        }
    }

    private static bool TryReadLegacyFlagFile(string path, out bool value)
    {
        value = false;
        try
        {
            return TryReadBooleanText(File.ReadAllText(path), out value);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return false;
        }
    }

    public static string NormalizeKboFlagKey(string fileName)
    {
        var key = Path.GetFileName(fileName);
        return key.EndsWith(".txt", StringComparison.OrdinalIgnoreCase)
            ? key[..^4]
            : key;
    }
    
    public static void WriteKboForeignWaiverAiFlag(bool enabled)
    {
        WriteKboFlag("enable_foreign_waiver_ai.txt", "Foreign waiver AI flag", enabled);
    }
    
    public static void WriteKboSingleDivisionAllstarEventsFlag(bool enabled)
    {
        var path = GetKboFlagConfigPath();
        WriteKboSingleDivisionAllstarEventsFlag(path, enabled);
        Console.WriteLine($"Single-division all-star flags: {path} = {(enabled ? "enabled" : "disabled")}");
    }

    internal static void WriteKboSingleDivisionAllstarEventsFlag(string configPath, bool enabled)
    {
        WriteKboFlagValues(configPath, SingleDivisionAllstarFlagFiles, enabled);
    }
}
