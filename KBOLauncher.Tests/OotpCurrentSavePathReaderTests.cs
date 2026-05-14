namespace KBOLauncher.Tests;

using System.Text;
using FluentAssertions;
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
        global::OotpCurrentSavePathReader.ExtractLgSavePath(sourcePath).Should().Be(expected);
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData(@"C:\OOTP\saved_games\KBO.lgbackup\players.dat")]
    [InlineData(@"C:\OOTP\saved_games\KBO\players.dat")]
    [InlineData(@"C:\OOTP\saved_games\KBO.lgx")]
    public void ExtractLgSavePath_RejectsNonLgPaths(string? sourcePath)
    {
        global::OotpCurrentSavePathReader.ExtractLgSavePath(sourcePath).Should().BeNull();
    }

    [Fact]
    public void LooksLikeAbsoluteLgSavePath_RequiresAbsoluteExistingLgDirectory()
    {
        var saveDir = Path.Combine(tempDir, "KBO.lg");
        Directory.CreateDirectory(saveDir);
        var relativeSave = Path.Combine("relative", "KBO.lg");

        global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(saveDir).Should().BeTrue();
        global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(relativeSave).Should().BeFalse();
        global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(Path.Combine(tempDir, "Missing.lg")).Should().BeFalse();
        global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(tempDir).Should().BeFalse();
    }

    [Fact]
    public void ExtractLgSavePath_AllowsKoreanUserAndSaveNames()
    {
        var sourcePath = @"C:\Users\홍길동\Documents\OOTP Baseball 27\saved_games\한국시리즈 2026.lg\players.dat";

        var savePath = global::OotpCurrentSavePathReader.ExtractLgSavePath(sourcePath);

        savePath.Should().Be(@"C:\Users\홍길동\Documents\OOTP Baseball 27\saved_games\한국시리즈 2026.lg");
    }

    [Fact]
    public void LooksLikeAbsoluteLgSavePath_AllowsExistingNonAsciiLocalAppDataPath()
    {
        var saveDir = Path.Combine(tempDir, "Users", "홍길동", "AppData", "Local", "OOTP", "saved_games", "테스트 저장.lg");
        Directory.CreateDirectory(saveDir);

        global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(saveDir).Should().BeTrue();
    }

    [Fact]
    public void LooksLikeAbsoluteLgSavePath_AllowsExistingLongPathNearWindowsLegacyLimit()
    {
        var longSegment = new string('a', 90);
        var saveDir = Path.Combine(tempDir, longSegment, longSegment, "긴 저장 이름.lg");
        Directory.CreateDirectory(saveDir);

        global::OotpCurrentSavePathReader.LooksLikeAbsoluteLgSavePath(saveDir).Should().BeTrue();
        saveDir.Length.Should().BeGreaterThanOrEqualTo(240);
    }

    [Fact]
    public void DecodeNullTerminatedOotpPathString_AllowsUtf8KoreanSavePaths()
    {
        var path = "C:\\Users\\홍길동\\Documents\\Out of the Park Developments\\OOTP Baseball 27\\saved_games\\긴 저장 이름.lg";
        var bytes = Encoding.UTF8.GetBytes(path).Concat([byte.MinValue]).ToArray();

        var decoded = global::OotpCurrentSavePathReader.DecodeNullTerminatedOotpPathString(bytes);

        decoded.Should().Be(path);
    }

    [Fact]
    public void DecodeNullTerminatedOotpPathString_AllowsLongUtf8SavePaths()
    {
        var longName = new string('가', 260);
        var path = $"C:\\Users\\홍길동\\OneDrive\\문서\\OOTP\\saved_games\\{longName}.lg";
        var bytes = Encoding.UTF8.GetBytes(path).Concat([byte.MinValue]).ToArray();

        var decoded = global::OotpCurrentSavePathReader.DecodeNullTerminatedOotpPathString(bytes);

        decoded.Should().Be(path);
        decoded.Should().NotBeNull();
        decoded!.Length.Should().BeGreaterThan(260);
        bytes.Length.Should().BeGreaterThan(260);
    }

    [Fact]
    public void DecodeNullTerminatedOotpPathString_RejectsControlCharacters()
    {
        var bytes = Encoding.UTF8.GetBytes("C:\\OOTP\\badsave.lg").Concat([byte.MinValue]).ToArray();

        global::OotpCurrentSavePathReader.DecodeNullTerminatedOotpPathString(bytes).Should().BeNull();
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
