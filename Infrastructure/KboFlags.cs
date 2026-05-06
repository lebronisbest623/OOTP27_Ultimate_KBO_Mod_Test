using System.Text.Json;
using static LauncherPaths;

internal static class KboFlags
{
    public static void WriteKboFlag(string fileName, string label, bool enabled)
    {
        var key = NormalizeKboFlagKey(fileName);
        var path = GetKboFlagConfigPath();
        WriteKboFlagValue(path, key, enabled);
        Console.WriteLine($"{label}: {path} {key} = {(enabled ? "enabled" : "disabled")}");
    }

    internal static void WriteKboFlagValue(string configPath, string fileName, bool enabled)
    {
        var key = NormalizeKboFlagKey(fileName);
        var flags = ReadKboFlagConfig(configPath);
        flags[key] = enabled;
        Directory.CreateDirectory(Path.GetDirectoryName(configPath)!);
        var json = JsonSerializer.Serialize(flags, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(configPath, json + Environment.NewLine);
    }

    public static void ImportLegacyKboFlagFilesIfMissing()
    {
        ImportLegacyKboFlagFilesIfMissing(GetKboFlagConfigPath());
    }

    internal static void ImportLegacyKboFlagFilesIfMissing(string configPath)
    {
        var configDir = Path.GetDirectoryName(configPath);
        if (string.IsNullOrWhiteSpace(configDir) || !Directory.Exists(configDir))
        {
            return;
        }

        var flags = ReadKboFlagConfig(configPath);
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
            if (flags.ContainsKey(key))
            {
                continue;
            }

            if (!TryReadLegacyFlagFile(path, out var value))
            {
                continue;
            }

            flags[key] = value;
            changed = true;
        }

        if (!changed)
        {
            return;
        }

        Directory.CreateDirectory(configDir);
        var json = JsonSerializer.Serialize(flags, new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(configPath, json + Environment.NewLine);
    }
    
    public static bool ReadKboFlag(string fileName)
    {
        return ReadKboFlag(GetKboFlagConfigPath(), fileName);
    }

    internal static bool ReadKboFlag(string configPath, string fileName)
    {
        return ReadKboFlagConfig(configPath).TryGetValue(NormalizeKboFlagKey(fileName), out var enabled)
            && enabled;
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
    
    public static void WriteKboMilitaryDraftPoolFlag(bool enabled)
    {
        WriteKboFlag("enable_military_draft_pool.txt", "Military draft pool flag", enabled);
    }
    
    public static void WriteKboForeignWaiverAiFlag(bool enabled)
    {
        WriteKboFlag("enable_foreign_waiver_ai.txt", "Foreign waiver AI flag", enabled);
    }
    
    public static void WriteKboSingleDivisionAllstarEventsFlag(bool enabled)
    {
        WriteKboFlag("enable_single_division_allstar_events.txt", "Single-division all-star events flag", enabled);
    }
}
