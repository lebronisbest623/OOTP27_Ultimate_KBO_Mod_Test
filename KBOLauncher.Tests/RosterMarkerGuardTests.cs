namespace KBOLauncher.Tests;

using Xunit;

public sealed class RosterMarkerGuardTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-roster-marker-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void CheckSavePath_FailsWhenSaveDirectoryIsMissing()
    {
        var missing = Path.Combine(tempDir, "Missing.lg");

        var info = global::KboRosterMarkerGuard.CheckSavePath(missing);

        Assert.False(info.Ok);
        Assert.Equal("current_save_missing", info.Status);
    }

    [Fact]
    public void CheckSavePath_FailsWhenDirectoryIsNotLgSave()
    {
        var savePath = Path.Combine(tempDir, "NotALeague");
        Directory.CreateDirectory(savePath);

        var info = global::KboRosterMarkerGuard.CheckSavePath(savePath);

        Assert.False(info.Ok);
        Assert.Equal("current_save_not_lg", info.Status);
    }

    [Fact]
    public void CheckSavePath_FailsClosedWhenDescriptionIsMissingOrUnmarked()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);

        var missingDescription = global::KboRosterMarkerGuard.CheckSavePath(savePath);
        Assert.False(missingDescription.Ok);
        Assert.Equal("description_missing", missingDescription.Status);

        File.WriteAllText(Path.Combine(savePath, "description.txt"), "plain roster");
        var missingMarker = global::KboRosterMarkerGuard.CheckSavePath(savePath);
        Assert.False(missingMarker.Ok);
        Assert.Equal("marker_missing", missingMarker.Status);
    }

    [Fact]
    public void CheckSavePath_AllowsMarkedLgSaveCaseInsensitively()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl.ToUpperInvariant());

        var info = global::KboRosterMarkerGuard.CheckSavePath(savePath);

        Assert.True(info.Ok);
        Assert.Equal("marked", info.Status);
        Assert.Contains("ok=1", global::KboRosterMarkerGuard.FormatLogStatus(info));
        Assert.Contains("current save marked", global::KboRosterMarkerGuard.FormatConsoleStatus(info));
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
