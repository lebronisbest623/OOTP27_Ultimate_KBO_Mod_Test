#ifndef KBO_BUILD_VERIFY_H
#define KBO_BUILD_VERIFY_H

#include <stddef.h>
#include <stdint.h>

typedef struct OotpSupportedBuild {
    uint32_t timestamp;
    uint32_t size_of_image;
    const char* label;
} OotpSupportedBuild;

typedef struct OotpBuildInfo {
    int      ok;
    uint32_t timestamp;
    uint32_t size_of_image;
} OotpBuildInfo;

OotpBuildInfo read_ootp_build_info(void);
int verify_ootp_build(void);
size_t kbo_supported_ootp_build_count(void);
const OotpSupportedBuild* kbo_supported_ootp_build_at(size_t index);
int kbo_ootp_build_is_steam_2026_05_04(OotpBuildInfo info);

#endif
