#include "foreign_signability_offer_attach_probe_utils.h"
#include "../../../../../build_verify/build_verify.h"

uint32_t kbo_foreign_ai_offer_attach_caller_rva(uintptr_t caller_return_ptr)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL || caller_return_ptr < (uintptr_t)exe) {
        return 0u;
    }
    return (uint32_t)(caller_return_ptr - (uintptr_t)exe);
}

uint8_t kbo_offer_read_u8(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0 || offset + sizeof(uint8_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(uint8_t))) {
        return 0u;
    }
    return *(uint8_t*)(offer_ptr + offset);
}

uint16_t kbo_offer_read_u16(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0 || offset + sizeof(uint16_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(offer_ptr + offset);
}

int32_t kbo_offer_read_i32(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0 || offset + sizeof(int32_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(offer_ptr + offset);
}

uint32_t kbo_offer_probe_team_id_from_ptr(uintptr_t team_ptr)
{
    if (team_ptr == 0
            || !memory_range_readable((void*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET), sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
}

void* kbo_offer_probe_resolve_rva(uint32_t rva)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }
    return kbo_resolve_build_specific_rva_ptr(exe, rva);
}

