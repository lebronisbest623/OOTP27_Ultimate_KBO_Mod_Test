
record OotpBuildInfo(bool Ok, uint Timestamp, uint SizeOfImage, string? Error)
{
    public static OotpBuildInfo Fail(string error)
    {
        return new OotpBuildInfo(false, 0, 0, error);
    }
}

record OotpSupportedBuild(uint Timestamp, uint SizeOfImage, string Label, bool ExperimentalSignature = false)
{
    public string SupportStatus => ExperimentalSignature ? "experimental_signature_supported" : "supported";
}

