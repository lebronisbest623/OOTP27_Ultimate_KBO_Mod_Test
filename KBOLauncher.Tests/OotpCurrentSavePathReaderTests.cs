namespace KBOLauncher.Tests;

using System.Text;
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

    [Fact]
    public void ExtractLgSavePath_AllowsKoreanUserAndSaveNames()
    {
        var sourcePath = @"C:\Users\홍길동\Documents\OOTP Baseball 27\saved_games\한국시리즈 2026.lg\players.dat";

        var savePath = global::OotpCurrentSavePathReader.ExtractLgSavePath(sourcePath);

        Assert.Equal(@"C:\Users\홍길동\Documents\OOTP Baseball 27\saved_games\한국시리즈 2026.lg", savePath);
    }

    [Fact]
    public void LooksLikeAbsoluteLgSavePath_AllowsExistingNonAsciiLocalAppDataPath()
    {
        var saveDir = Path.Combine(tempDir, "Users", "홍길동", "AppData", "Local", "OOTP", "saved_games", "테스트 저장.lg");
        Directory.CreateDirectory(saveDir);

        Assert.True(global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(saveDir));
    }

    [Fact]
    public void LooksLikeAbsoluteLgSavePath_AllowsExistingLongPathNearWindowsLegacyLimit()
    {
        var longSegment = new string('a', 90);
        var saveDir = Path.Combine(tempDir, longSegment, longSegment, "긴 저장 이름.lg");
        Directory.CreateDirectory(saveDir);

        Assert.True(global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(saveDir));
        Assert.True(saveDir.Length >= 240);
    }

    [Fact]
    public void DecodeNullTerminatedOotpPathString_AllowsUtf8KoreanSavePaths()
    {
        var path = "C:\\Users\\\uD64D\uAE38\uB3D9\\Documents\\Out of the Park Developments\\OOTP Baseball 27\\saved_games\\\uAE34 \uC800\uC7A5 \uC774\uB984.lg";
        var bytes = Encoding.UTF8.GetBytes(path).Concat([byte.MinValue]).ToArray();

        var decoded = global::OotpCurrentSavePathReader.DecodeNullTerminatedOotpPathString(bytes);

        Assert.Equal(path, decoded);
    }

    [Fact]
    public void DecodeNullTerminatedOotpPathString_AllowsLongUtf8SavePaths()
    {
        var longName = new string('\uAC00', 260);
        var path = $"C:\\Users\\\uD64D\uAE38\uB3D9\\OneDrive\\\uBB38\uC11C\\OOTP\\saved_games\\{longName}.lg";
        var bytes = Encoding.UTF8.GetBytes(path).Concat([byte.MinValue]).ToArray();

        var decoded = global::OotpCurrentSavePathReader.DecodeNullTerminatedOotpPathString(bytes);

        Assert.Equal(path, decoded);
        Assert.NotNull(decoded);
        Assert.True(decoded.Length > 260);
        Assert.True(bytes.Length > 260);
    }

    [Fact]
    public void DecodeNullTerminatedOotpPathString_RejectsControlCharacters()
    {
        var bytes = Encoding.UTF8.GetBytes("C:\\OOTP\\bad\u0001save.lg").Concat([byte.MinValue]).ToArray();

        Assert.Null(global::OotpCurrentSavePathReader.DecodeNullTerminatedOotpPathString(bytes));
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
