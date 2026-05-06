namespace KBOLauncher.Tests;

using Xunit;

public sealed class OotpBuildGuardTests : IDisposable
{
    private readonly string tempDir = Path.Combine(Path.GetTempPath(), "kbo-launcher-build-tests", Guid.NewGuid().ToString("N"));

    [Fact]
    public void Read_RejectsTooSmallFile()
    {
        var path = Path.Combine(tempDir, "tiny.exe");
        Directory.CreateDirectory(tempDir);
        File.WriteAllBytes(path, [0x4D, 0x5A]);

        var info = global::OotpBuildGuard.Read(path);

        Assert.False(info.Ok);
        Assert.Equal("file too small", info.Error);
        Assert.Null(global::OotpBuildGuard.FindSupportedBuild(info));
    }

    [Fact]
    public void Read_ParsesPeTimestampAndImageSize()
    {
        var path = Path.Combine(tempDir, "ootp27.exe");
        WriteMinimalPe(path, 0x12345678u, 0x00100000u);

        var info = global::OotpBuildGuard.Read(path);

        Assert.True(info.Ok);
        Assert.Equal(0x12345678u, info.Timestamp);
        Assert.Equal(0x00100000u, info.SizeOfImage);
        Assert.Null(global::OotpBuildGuard.FindSupportedBuild(info));
        Assert.Contains("unsupported", global::OotpBuildGuard.FormatConsoleStatus(info, null));
    }

    [Fact]
    public void FindSupportedBuild_MatchesVerifiedBuildOnly()
    {
        var info = new global::OotpBuildInfo(true, 0x69F75E6Bu, 0x03919000u, null);

        var supported = global::OotpBuildGuard.FindSupportedBuild(info);

        Assert.NotNull(supported);
        Assert.Equal("2026-05-04 Steam", supported.Label);
        Assert.Contains("status=supported", global::OotpBuildGuard.FormatLogStatus(info, supported));
    }

    private static void WriteMinimalPe(string path, uint timestamp, uint sizeOfImage)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        var bytes = new byte[0x200];
        bytes[0] = 0x4D;
        bytes[1] = 0x5A;
        BitConverter.GetBytes(0x80).CopyTo(bytes, 0x3C);
        bytes[0x80] = 0x50;
        bytes[0x81] = 0x45;
        bytes[0x82] = 0x00;
        bytes[0x83] = 0x00;
        BitConverter.GetBytes(timestamp).CopyTo(bytes, 0x80 + 8);
        BitConverter.GetBytes(sizeOfImage).CopyTo(bytes, 0x80 + 24 + 0x38);
        File.WriteAllBytes(path, bytes);
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
