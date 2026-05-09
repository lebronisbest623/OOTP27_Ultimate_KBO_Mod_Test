namespace KBOLauncher.Tests;

using Xunit;

public sealed class RosterMarkerGuardTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-roster-marker-tests", Guid.NewGuid().ToString("N"));
    private const string SaveCompletedText = "2026-05-09 23:44:08\tFinished save_database, closing flag file now";

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
    public void CheckSavePath_FailsClosedWhenMarkedSaveWasNotCompleted()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl.ToUpperInvariant());

        var info = global::KboRosterMarkerGuard.CheckSavePath(savePath);

        Assert.False(info.Ok);
        Assert.Equal("save_not_completed", info.Status);
        Assert.EndsWith("flag_save_completed.dat", info.SaveCompletedPath);
    }

    [Fact]
    public void CheckSavePath_FailsClosedWhenCompletedSaveIsOlderThanRequired()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl.ToUpperInvariant());
        var completedPath = Path.Combine(savePath, "flag_save_completed.dat");
        File.WriteAllText(completedPath, SaveCompletedText);
        File.SetLastWriteTimeUtc(completedPath, new DateTime(2026, 5, 9, 14, 44, 8, DateTimeKind.Utc));

        var info = global::KboRosterMarkerGuard.CheckSavePath(
            savePath,
            new DateTimeOffset(2026, 5, 9, 14, 45, 0, TimeSpan.Zero));

        Assert.False(info.Ok);
        Assert.Equal("save_completion_stale", info.Status);
    }

    [Fact]
    public void CheckSavePath_AllowsMarkedCompletedLgSaveCaseInsensitively()
    {
        var savePath = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(savePath);
        File.WriteAllText(Path.Combine(savePath, "description.txt"), global::KboRosterMarkerGuard.RequiredMarkerUrl.ToUpperInvariant());
        File.WriteAllText(Path.Combine(savePath, "flag_save_completed.dat"), SaveCompletedText);

        var info = global::KboRosterMarkerGuard.CheckSavePath(savePath);

        Assert.True(info.Ok);
        Assert.Equal("marked_save_completed", info.Status);
        Assert.Equal(Path.GetFullPath(savePath), info.SavePath);
        Assert.Equal(Path.Combine(Path.GetFullPath(savePath), "description.txt"), info.DescriptionPath);
        Assert.Equal(Path.Combine(Path.GetFullPath(savePath), "flag_save_completed.dat"), info.SaveCompletedPath);
        Assert.Contains("ok=1", global::KboRosterMarkerGuard.FormatLogStatus(info));
        Assert.Contains("current save marked and saved", global::KboRosterMarkerGuard.FormatConsoleStatus(info));
    }

    [Fact]
    public void FormatStatuses_IncludeFailureContextForInjectionLogs()
    {
        var savePath = Path.Combine(tempDir, "Plain.lg");
        var descriptionPath = Path.Combine(savePath, "description.txt");
        var info = global::RosterMarkerInfo.Fail("marker_missing", savePath, descriptionPath, "required marker missing");

        var log = global::KboRosterMarkerGuard.FormatLogStatus(info);

        Assert.Equal("KBO roster marker: blocked (marker_missing)", global::KboRosterMarkerGuard.FormatConsoleStatus(info));
        Assert.Contains("status=marker_missing", log);
        Assert.Contains("ok=0", log);
        Assert.Contains($"save=\"{savePath}\"", log);
        Assert.Contains($"description=\"{descriptionPath}\"", log);
        Assert.Contains("required marker missing", log);
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
