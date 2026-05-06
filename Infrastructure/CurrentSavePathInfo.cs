
record CurrentSavePathInfo(bool Ok, string Status, string? SavePath, string? Error)
{
    public static CurrentSavePathInfo Fail(string status, string? savePath, string error)
    {
        return new CurrentSavePathInfo(false, status, savePath, error);
    }
}
