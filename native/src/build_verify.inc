/* Supported OOTP 27 Steam builds. Update when a new build is verified. */
#define OOTP27_EXPECTED_BUILD_TIMESTAMP     0x69CE3A29u
#define OOTP27_EXPECTED_BUILD_SIZE_OF_IMAGE 0x03923000u

typedef struct OotpSupportedBuild {
    uint32_t timestamp;
    uint32_t size_of_image;
    const char* label;
} OotpSupportedBuild;

static const OotpSupportedBuild KBO_SUPPORTED_OOTP_BUILDS[] = {
    {0x69CE3A29u, 0x03923000u, "2026-04-02 Steam"},
    {0x69F75E6Bu, 0x03919000u, "2026-05-04 Steam"},
};

typedef struct OotpBuildInfo {
    int      ok;
    uint32_t timestamp;
    uint32_t size_of_image;
} OotpBuildInfo;

static OotpBuildInfo read_ootp_build_info(void)
{
    OotpBuildInfo info = {0};

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        append_log_line("build verify: GetModuleHandleA(NULL) returned NULL");
        return info;
    }

    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)exe;
    if (IsBadReadPtr(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_log_line("build verify: invalid DOS header");
        return info;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((uint8_t*)exe + dos->e_lfanew);
    if (IsBadReadPtr(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) {
        append_log_line("build verify: invalid NT header");
        return info;
    }

    info.ok = 1;
    info.timestamp = nt->FileHeader.TimeDateStamp;
    info.size_of_image = nt->OptionalHeader.SizeOfImage;
    return info;
}

static int verify_ootp_build(void)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (!info.ok) {
        append_log_line("build verify: failed to read PE headers; skipping all KBO patches");
        return 0;
    }

    if (OOTP27_EXPECTED_BUILD_TIMESTAMP == 0u) {
        append_logf(
            "build verify: discovery mode; detected timestamp=0x%08X size_of_image=0x%08X",
            info.timestamp, info.size_of_image);
        return 1;
    }

    for (size_t i = 0; i < sizeof(KBO_SUPPORTED_OOTP_BUILDS) / sizeof(KBO_SUPPORTED_OOTP_BUILDS[0]); ++i) {
        const OotpSupportedBuild* build = &KBO_SUPPORTED_OOTP_BUILDS[i];
        if (info.timestamp == build->timestamp && info.size_of_image == build->size_of_image) {
            append_logf(
                "build verify: ok label=%s timestamp=0x%08X size_of_image=0x%08X",
                build->label, info.timestamp, info.size_of_image);
            return 1;
        }
    }

    append_logf(
        "build verify: MISMATCH expected timestamp=0x%08X size_of_image=0x%08X, "
        "detected timestamp=0x%08X size_of_image=0x%08X. "
        "All KBO patches DISABLED to prevent crashes or save corruption.",
        OOTP27_EXPECTED_BUILD_TIMESTAMP,
        OOTP27_EXPECTED_BUILD_SIZE_OF_IMAGE,
        info.timestamp,
        info.size_of_image);

    return 0;
}
