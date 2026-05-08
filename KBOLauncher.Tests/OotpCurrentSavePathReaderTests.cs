namespace KBOLauncher.Tests;

using Xunit;

public sealed class OotpCurrentSavePathReaderTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-current-save-tests", Guid.NewGuid().ToString("N"));

    [Theory]
    [InlineData(@"C:\Users\me\Documents\OOTP Baseball 27\saved_games\KBO.lg\messages\1.txt", @"C:\Users\me\Documents\OOTP Baseball 27\saved_games\KBO.lg")]
    [InlineData(@"\\?\C:\Users\me\saved_games\KBO.lg\players.dat", @"C:\Users\me\saved_games\KBO.lg")]
    [InlineData(@"D:/OOTP/saved_games/KBO.lg/world.dat", @"D:/OOTP/saved_games/KBO.lg")]
    [InlineData(@"D:/OOTP/saved_games/KBO.lg", @"D:/OOTP/saved_games/KBO.lg")]
    public void ExtractLgSavePath_ReturnsContainingLgFolder(string sourcePath, string expected)
    {
        Assert.Equal(expected, global::OotpCurrentSavePathReader.ExtractLgSavePath(sourcePath));
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData(@"C:\OOTP\saved_games\KBO.lgbackup\players.dat")]
    [InlineData(@"C:\OOTP\saved_games\KBO\players.dat")]
    [InlineData(@"C:\OOTP\saved_games\KBO.lgx")]
    public void ExtractLgSavePath_RejectsNonLgPaths(string? sourcePath)
    {
        Assert.Null(global::OotpCurrentSavePathReader.ExtractLgSavePath(sourcePath));
    }

    [Fact]
    public void LooksLikeAbsoluteLgSavePath_RequiresAbsoluteExistingLgDirectory()
    {
        var saveDir = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(saveDir);
        var relativeSave = Path.Combine("relative", "KBO.lg");

        Assert.True(global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(saveDir));
        Assert.False(global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(relativeSave));
        Assert.False(global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(Path.Combine(tempDir, "Missing.lg")));
        Assert.False(global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(tempDir));
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
