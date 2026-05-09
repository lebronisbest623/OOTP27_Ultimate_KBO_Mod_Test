internal static class KboRosterMarkerGuard
{
    public const string RequiredMarkerUrl = "https://github.com/lebronisbest623/OOTP27_Ultimate_KBO";
    private const string SaveCompletedFileName = "flag_save_completed.dat";
    private const string SaveCompletedSentinel = "Finished save_database, closing flag file now";

    public static RosterMarkerInfo CheckCurrentSave(int pid, Action<string>? log = null)
    {
        return CheckCurrentSave(pid, null, log);
    }

    public static RosterMarkerInfo CheckCurrentSave(int pid, DateTimeOffset? minSaveCompletedAt, Action<string>? log = null)
    {
        var savePathInfo = OotpCurrentSavePathReader.TryRead(pid, log);
        if (!savePathInfo.Ok)
        {
            return RosterMarkerInfo.Fail(savePathInfo.Status, savePathInfo.SavePath, null, savePathInfo.Error ?? "current save path unavailable");
        }

        return CheckSavePath(savePathInfo.SavePath!, minSaveCompletedAt);
    }

    internal static RosterMarkerInfo CheckSavePath(string savePath)
    {
        return CheckSavePath(savePath, null);
    }

    internal static RosterMarkerInfo CheckSavePath(string savePath, DateTimeOffset? minSaveCompletedAt)
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

        var saveCompletedPath = Path.Combine(normalizedSavePath, SaveCompletedFileName);
        if (!File.Exists(saveCompletedPath))
        {
            return RosterMarkerInfo.Fail("save_not_completed", normalizedSavePath, descriptionPath, saveCompletedPath, $"{SaveCompletedFileName} not found");
        }

        string completedText;
        DateTimeOffset completedAt;
        try
        {
            completedText = File.ReadAllText(saveCompletedPath);
            completedAt = File.GetLastWriteTimeUtc(saveCompletedPath);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            return RosterMarkerInfo.Fail("save_completion_unreadable", normalizedSavePath, descriptionPath, saveCompletedPath, $"{ex.GetType().Name}: {ex.Message}");
        }

        if (completedText.IndexOf(SaveCompletedSentinel, StringComparison.OrdinalIgnoreCase) < 0)
        {
            return RosterMarkerInfo.Fail("save_not_completed", normalizedSavePath, descriptionPath, saveCompletedPath, $"{SaveCompletedFileName} does not contain the completed save sentinel");
        }

        if (minSaveCompletedAt is not null && completedAt < minSaveCompletedAt.Value.ToUniversalTime())
        {
            return RosterMarkerInfo.Fail(
                "save_completion_stale",
                normalizedSavePath,
                descriptionPath,
                saveCompletedPath,
                $"{SaveCompletedFileName} was last written before this launcher run");
        }

        return new RosterMarkerInfo(true, "marked_save_completed", normalizedSavePath, descriptionPath, saveCompletedPath, completedAt, null);
    }

    public static string FormatConsoleStatus(RosterMarkerInfo info)
    {
        if (info.Ok)
        {
            return $"KBO roster marker: current save marked and saved in {info.DescriptionPath}";
        }

        return $"KBO roster marker: blocked ({info.Status})";
    }

    public static string FormatLogStatus(RosterMarkerInfo info)
    {
        var save = string.IsNullOrWhiteSpace(info.SavePath) ? "" : info.SavePath;
        var description = string.IsNullOrWhiteSpace(info.DescriptionPath) ? "" : info.DescriptionPath;
        var saveCompleted = string.IsNullOrWhiteSpace(info.SaveCompletedPath) ? "" : info.SaveCompletedPath;
        var completedAt = info.SaveCompletedAt is null ? "" : info.SaveCompletedAt.Value.ToString("O");
        var error = string.IsNullOrWhiteSpace(info.Error) ? "" : info.Error;
        return $"roster_marker status={info.Status} ok={(info.Ok ? "1" : "0")} save=\"{save}\" description=\"{description}\" save_completed=\"{saveCompleted}\" save_completed_at=\"{completedAt}\" error=\"{error}\"";
    }
}

