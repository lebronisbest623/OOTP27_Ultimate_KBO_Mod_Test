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
    public void Read_RejectsInvalidPeSignaturesAndHeaderOffsets()
    {
        var invalidDos = Path.Combine(tempDir, "invalid-dos.exe");
        Directory.CreateDirectory(tempDir);
        File.WriteAllBytes(invalidDos, new byte[0x200]);
        Assert.Equal("invalid DOS signature", global::OotpBuildGuard.Read(invalidDos).Error);

        var invalidOffset = Path.Combine(tempDir, "invalid-offset.exe");
        var offsetBytes = new byte[0x200];
        offsetBytes[0] = 0x4D;
        offsetBytes[1] = 0x5A;
        BitConverter.GetBytes(0x1FF).CopyTo(offsetBytes, 0x3C);
        File.WriteAllBytes(invalidOffset, offsetBytes);
        Assert.Equal("invalid PE header offset", global::OotpBuildGuard.Read(invalidOffset).Error);

        var invalidPe = Path.Combine(tempDir, "invalid-pe.exe");
        WriteMinimalPe(invalidPe, 0x12345678u, 0x00100000u);
        using (var stream = File.OpenWrite(invalidPe))
        {
            stream.Position = 0x80;
            stream.WriteByte(0x00);
        }

        Assert.Equal("invalid PE signature", global::OotpBuildGuard.Read(invalidPe).Error);
    }

    [Fact]
    public void FindSupportedBuild_MatchesNativePatchSupportedBuilds()
    {
        foreach (var expected in global::OotpSupportedBuilds.All.Where(build => build.NativePatchesSupported))
        {
            var info = new global::OotpBuildInfo(true, expected.Timestamp, expected.SizeOfImage, null);

            var supported = global::OotpBuildGuard.FindSupportedBuild(info);

            Assert.NotNull(supported);
            Assert.Equal(expected.Label, supported.Label);
            Assert.Contains($"supported ({expected.Label})", global::OotpBuildGuard.FormatConsoleStatus(info, supported));
            Assert.Contains("status=supported", global::OotpBuildGuard.FormatLogStatus(info, supported));
            Assert.Contains(
                $"{expected.Label}:0x{expected.Timestamp:X8}/0x{expected.SizeOfImage:X8}" +
                (expected.ExperimentalSignature ? "[experimental_signature]" : "") +
                "[native_patches]",
                global::OotpBuildGuard.SupportedBuildDescriptions());
        }
    }

    [Fact]
    public void FindSupportedBuild_RejectsMetadataOnlyExperimentalSignatureBuild()
    {
        var expected = global::OotpSupportedBuilds.All.First(build => build.ExperimentalSignature);
        var info = new global::OotpBuildInfo(true, expected.Timestamp, expected.SizeOfImage, null);

        var supported = global::OotpBuildGuard.FindSupportedBuild(info);
        var known = global::OotpBuildGuard.FindKnownBuild(info);

        Assert.Null(supported);
        Assert.NotNull(known);
        Assert.True(known.ExperimentalSignature);
        Assert.False(known.NativePatchesSupported);
        Assert.Contains("unsupported", global::OotpBuildGuard.FormatConsoleStatus(info, supported));
        Assert.Contains(
            $"{expected.Label}:0x{expected.Timestamp:X8}/0x{expected.SizeOfImage:X8}[experimental_signature][metadata_only]",
            global::OotpBuildGuard.SupportedBuildDescriptions());
    }

    [Fact]
    public void FormatStatuses_IncludeUnreadableAndUnsupportedDetails()
    {
        var unreadable = global::OotpBuildInfo.Fail("boom");
        var unsupported = new global::OotpBuildInfo(true, 0x11111111u, 0x22222222u, null);

        Assert.Equal("OOTP build: unreadable (boom)", global::OotpBuildGuard.FormatConsoleStatus(unreadable, null));
        Assert.Equal("ootp_build status=unreadable error=\"boom\"", global::OotpBuildGuard.FormatLogStatus(unreadable, null));
        Assert.Contains("timestamp=0x11111111", global::OotpBuildGuard.FormatConsoleStatus(unsupported, null));
        Assert.Equal(
            "ootp_build status=unsupported timestamp=0x11111111 size_of_image=0x22222222",
            global::OotpBuildGuard.FormatLogStatus(unsupported, null));
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
