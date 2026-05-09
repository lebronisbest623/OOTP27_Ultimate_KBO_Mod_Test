#include "no_minor_patch_helpers.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../patch_helpers/patch_helpers.h"

int patch_kbo_no_minor_contract_write_site_by_masked_context(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t size,
    const uint8_t* context,
    const uint8_t* context_mask,
    size_t context_size,
    size_t target_offset)
{
    uint8_t* target = resolve_patch_target_by_rva_or_masked_context_pattern(
        exe,
        rva,
        expected,
        size,
        context,
        context_mask,
        context_size,
        target_offset,
        label);
    if (target == NULL) {
        return 0;
    }
    return patch_static_bytes(label, target, expected, patch, size);
}

int patch_kbo_no_minor_contract_write_site_by_pattern(
    HMODULE exe,
    const char* label,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t size)
{
    uint8_t* target = resolve_patch_target_by_rva_or_pattern(exe, rva, expected, size, label);
    if (target == NULL) {
        return 0;
    }
    return patch_static_bytes(label, target, expected, patch, size);
}

int patch_kbo_no_minor_contract_write_site_by_pattern_ordinal(
    HMODULE exe,
    const char* label,
    const uint8_t* expected,
    const uint8_t* patch,
    size_t size,
    int desired_index,
    int expected_hits)
{
    (void)exe;
    uint8_t* target = find_ootp_executable_pattern_nth(expected, size, desired_index, expected_hits);
    if (target == NULL) {
        append_logf("%s ordinal pattern unresolved desired=%d expected_hits=%d", label, desired_index, expected_hits);
        return 0;
    }
    return patch_static_bytes(label, target, expected, patch, size);
}
