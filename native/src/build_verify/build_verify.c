#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "build_verify.h"
#include "../core/core_log.h"

#include "supported_builds.generated.inc"

OotpBuildInfo read_ootp_build_info(void)
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

size_t kbo_supported_ootp_build_count(void)
{
    return sizeof(KBO_SUPPORTED_OOTP_BUILDS) / sizeof(KBO_SUPPORTED_OOTP_BUILDS[0]);
}

const OotpSupportedBuild* kbo_supported_ootp_build_at(size_t index)
{
    if (index >= kbo_supported_ootp_build_count()) {
        return NULL;
    }
    return &KBO_SUPPORTED_OOTP_BUILDS[index];
}

int kbo_ootp_build_is_steam_2026_05_04(OotpBuildInfo info)
{
    return info.ok
        && info.timestamp == KBO_SUPPORTED_OOTP_BUILD_STEAM_2026_05_04_TIMESTAMP
        && info.size_of_image == KBO_SUPPORTED_OOTP_BUILD_STEAM_2026_05_04_SIZE_OF_IMAGE;
}

int verify_ootp_build(void)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (!info.ok) {
        append_log_line("build verify: failed to read PE headers; skipping all KBO patches");
        return 0;
    }

    if (KBO_SUPPORTED_OOTP_BUILD_COUNT == 0u) {
        append_logf(
            "build verify: discovery mode; detected timestamp=0x%08X size_of_image=0x%08X",
            info.timestamp, info.size_of_image);
        return 1;
    }

    for (size_t i = 0; i < kbo_supported_ootp_build_count(); ++i) {
        const OotpSupportedBuild* build = kbo_supported_ootp_build_at(i);
        if (build != NULL && info.timestamp == build->timestamp && info.size_of_image == build->size_of_image) {
            append_logf(
                "build verify: ok label=%s timestamp=0x%08X size_of_image=0x%08X",
                build->label, info.timestamp, info.size_of_image);
            return 1;
        }
    }

    append_logf(
        "build verify: MISMATCH expected one of %u supported builds, "
        "detected timestamp=0x%08X size_of_image=0x%08X. "
        "All KBO patches DISABLED to prevent crashes or save corruption.",
        (unsigned)kbo_supported_ootp_build_count(),
        info.timestamp,
        info.size_of_image);

    return 0;
}
