record RosterMarkerInfo(
    bool Ok,
    string Status,
    string? SavePath,
    string? DescriptionPath,
    string? SaveCompletedPath,
    DateTimeOffset? SaveCompletedAt,
    string? Error)
{
    public static RosterMarkerInfo Fail(string status, string? savePath, string? descriptionPath, string error)
    {
        return new RosterMarkerInfo(false, status, savePath, descriptionPath, null, null, error);
    }

    public static RosterMarkerInfo Fail(string status, string? savePath, string? descriptionPath, string? saveCompletedPath, string error)
    {
        return new RosterMarkerInfo(false, status, savePath, descriptionPath, saveCompletedPath, null, error);
    }
}
