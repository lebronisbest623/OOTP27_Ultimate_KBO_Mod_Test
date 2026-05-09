
record OotpBuildInfo(bool Ok, uint Timestamp, uint SizeOfImage, string? Error)
{
    public static OotpBuildInfo Fail(string error)
    {
        return new OotpBuildInfo(false, 0, 0, error);
    }
}

record OotpSupportedBuild(
    uint Timestamp,
    uint SizeOfImage,
    string Label,
    bool ExperimentalSignature = false,
    bool NativePatchesSupported = false)
{
    public string SupportStatus => NativePatchesSupported ? "supported" : "metadata_only";
}

