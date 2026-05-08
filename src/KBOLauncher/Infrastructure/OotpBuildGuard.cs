using System.Reflection.PortableExecutable;

internal static class OotpBuildGuard
{
    public static OotpBuildInfo Read(string exePath)
    {
        try
        {
            using var stream = File.Open(exePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            using var reader = new BinaryReader(stream);

            if (stream.Length < 0x100)
            {
                return OotpBuildInfo.Fail("file too small");
            }

            stream.Position = 0;
            if (reader.ReadUInt16() != 0x5A4D)
            {
                return OotpBuildInfo.Fail("invalid DOS signature");
            }

            stream.Position = 0x3C;
            var peHeaderOffset = reader.ReadInt32();
            if (peHeaderOffset <= 0 || peHeaderOffset + 0x58 > stream.Length)
            {
                return OotpBuildInfo.Fail("invalid PE header offset");
            }

            stream.Position = peHeaderOffset;
            if (reader.ReadUInt32() != 0x00004550)
            {
                return OotpBuildInfo.Fail("invalid PE signature");
            }

            stream.Position = peHeaderOffset + 8;
            var timestamp = reader.ReadUInt32();

            stream.Position = peHeaderOffset + 24 + 0x38;
            var sizeOfImage = reader.ReadUInt32();

            return new OotpBuildInfo(true, timestamp, sizeOfImage, null);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException)
        {
            return OotpBuildInfo.Fail($"{ex.GetType().Name}: {ex.Message}");
        }
    }

    public static OotpSupportedBuild? FindSupportedBuild(OotpBuildInfo info)
    {
        if (!info.Ok)
        {
            return null;
        }

        return OotpSupportedBuilds.All.FirstOrDefault(build =>
            build.Timestamp == info.Timestamp && build.SizeOfImage == info.SizeOfImage);
    }

    public static string FormatConsoleStatus(OotpBuildInfo info, OotpSupportedBuild? supportedBuild)
    {
        if (!info.Ok)
        {
            return $"OOTP build: unreadable ({info.Error})";
        }

        var suffix = supportedBuild is null
            ? "unsupported"
            : $"supported ({supportedBuild.Label})";
        return $"OOTP build: timestamp=0x{info.Timestamp:X8}, size_of_image=0x{info.SizeOfImage:X8} [{suffix}]";
    }

    public static string FormatLogStatus(OotpBuildInfo info, OotpSupportedBuild? supportedBuild)
    {
        if (!info.Ok)
        {
            return $"ootp_build status=unreadable error=\"{info.Error}\"";
        }

        if (supportedBuild is null)
        {
            return $"ootp_build status=unsupported timestamp=0x{info.Timestamp:X8} size_of_image=0x{info.SizeOfImage:X8}";
        }

        return $"ootp_build status=supported label=\"{supportedBuild.Label}\" timestamp=0x{info.Timestamp:X8} size_of_image=0x{info.SizeOfImage:X8}";
    }

    public static IEnumerable<string> SupportedBuildDescriptions()
    {
        return OotpSupportedBuilds.All.Select(build => $"{build.Label}:0x{build.Timestamp:X8}/0x{build.SizeOfImage:X8}");
    }
}
