record RosterMarkerInfo(bool Ok, string Status, string? SavePath, string? DescriptionPath, string? Error)
{
    public static RosterMarkerInfo Fail(string status, string? savePath, string? descriptionPath, string error)
    {
        return new RosterMarkerInfo(false, status, savePath, descriptionPath, error);
    }
}
