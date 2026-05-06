internal static class KboRosterMarkerGuard
{
    public const string RequiredMarkerUrl = "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO";

    public static RosterMarkerInfo CheckCurrentSave(int pid, Action<string>? log = null)
    {
        var savePathInfo = OotpCurrentSavePathReader.TryRead(pid, log);
        if (!savePathInfo.Ok)
        {
            return RosterMarkerInfo.Fail(savePathInfo.Status, savePathInfo.SavePath, null, savePathInfo.Error ?? "current save path unavailable");
        }

        return CheckSavePath(savePathInfo.SavePath!);
    }

    internal static RosterMarkerInfo CheckSavePath(string savePath)
    {
        var normalizedSavePath = Path.GetFullPath(savePath);
        if (!Directory.Exists(normalizedSavePath))
        {
            return RosterMarkerInfo.Fail("current_save_missing", normalizedSavePath, null, "current save directory was not found");
        }
        if (!normalizedSavePath.EndsWith(".lg", StringComparison.OrdinalIgnoreCase))
        {
            return RosterMarkerInfo.Fail("current_save_not_lg", normalizedSavePath, null, "current save path is not an .lg directory");
        }

        var descriptionPath = Path.Combine(normalizedSavePath, "description.txt");
        if (!File.Exists(descriptionPath))
        {
            return RosterMarkerInfo.Fail("description_missing", normalizedSavePath, descriptionPath, "description.txt not found");
        }

        string text;
        try
        {
            text = File.ReadAllText(descriptionPath);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return RosterMarkerInfo.Fail("description_unreadable", normalizedSavePath, descriptionPath, $"{ex.GetType().Name}: {ex.Message}");
        }

        if (text.IndexOf(RequiredMarkerUrl, StringComparison.OrdinalIgnoreCase) < 0)
        {
            return RosterMarkerInfo.Fail("marker_missing", normalizedSavePath, descriptionPath, "required GitHub marker was not found");
        }

        return new RosterMarkerInfo(true, "marked", normalizedSavePath, descriptionPath, null);
    }

    public static string FormatConsoleStatus(RosterMarkerInfo info)
    {
        if (info.Ok)
        {
            return $"KBO roster marker: current save marked in {info.DescriptionPath}";
        }

        return $"KBO roster marker: blocked ({info.Status})";
    }

    public static string FormatLogStatus(RosterMarkerInfo info)
    {
        var save = string.IsNullOrWhiteSpace(info.SavePath) ? "" : info.SavePath;
        var description = string.IsNullOrWhiteSpace(info.DescriptionPath) ? "" : info.DescriptionPath;
        var error = string.IsNullOrWhiteSpace(info.Error) ? "" : info.Error;
        return $"roster_marker status={info.Status} ok={(info.Ok ? "1" : "0")} save=\"{save}\" description=\"{description}\" error=\"{error}\"";
    }
}

