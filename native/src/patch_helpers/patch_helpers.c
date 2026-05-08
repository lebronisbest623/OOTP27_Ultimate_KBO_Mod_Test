#include "patch_helpers.h"
#include <stdio.h>
#include <string.h>
#include "../bootstrap/ootp_offsets.h"
#include "../core/core_log.h"
#include "../core/core_current_date.h"
#include "../core/core_save_paths.h"
#include "../core/core_text_date.h"
#include "../core/core_flags/flags_api.h"
#include "../runtime_memory/runtime_memory.h"

void write_u64(uint8_t* dst, uint64_t value)
{
    for (int i = 0; i < 8; i++) {
        dst[i] = (uint8_t)((value >> (i * 8)) & 0xffu);
    }
}

void write_u32(uint8_t* dst, uint32_t value)
{
    for (int i = 0; i < 4; i++) {
        dst[i] = (uint8_t)((value >> (i * 8)) & 0xffu);
    }
}

int is_r11_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0x49
        && target[1] == 0xBB
        && target[10] == 0x41
        && target[11] == 0xFF
        && target[12] == 0xE3;
}

int is_rax_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0x48
        && target[1] == 0xB8
        && target[10] == 0xFF
        && target[11] == 0xE0;
}

int is_rip_absolute_jump_patch(const uint8_t* target)
{
    return target[0] == 0xFF
        && target[1] == 0x25
        && target[2] == 0x00
        && target[3] == 0x00
        && target[4] == 0x00
        && target[5] == 0x00;
}

void log_patch_bytes_mismatch(const char* label, const uint8_t* target, size_t size)
{
    char bytes[128] = {0};
    size_t offset = 0;
    for (size_t i = 0; i < size && offset + 4 < sizeof(bytes); i++) {
        offset += (size_t)snprintf(bytes + offset, sizeof(bytes) - offset, "%s%02X", i == 0 ? "" : " ", target[i]);
    }
    append_logf("%s bytes mismatch at %p: %s", label, target, bytes);
}

/* Dump pre_bytes before and (total_bytes - pre_bytes) after target, 16 bytes per log line.
 * Useful for diagnosing hook site mismatches after a binary update. */
void log_extended_context(const char* label, const uint8_t* target, int pre_bytes, int total_bytes)
{
    const uint8_t* start = (pre_bytes > 0) ? (target - pre_bytes) : target;
    if ((uintptr_t)start < 0x10000u) {
        start = (const uint8_t*)0x10000u;
    }
    if (!memory_range_readable(start, (SIZE_T)total_bytes)) {
        append_logf("%s extended ctx unreadable start=%p pre=%d total=%d", label, start, pre_bytes, total_bytes);
        return;
    }
    for (int row = 0; row < total_bytes; row += 16) {
        char hex[80] = {0};
        size_t hex_off = 0;
        for (int col = 0; col < 16 && row + col < total_bytes; col++) {
            hex_off += (size_t)snprintf(hex + hex_off, sizeof(hex) - hex_off,
                "%s%02X", col == 0 ? "" : " ", start[row + col]);
        }
        int rel = row - pre_bytes;
        append_logf("%s ctx%+d [%p]: %s", label, rel, start + row, hex);
    }
}

uint8_t* resolve_patch_target_by_rva_or_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const char* label)
{
    if (exe == NULL || expected == NULL || expected_size == 0) {
        return NULL;
    }

    uint8_t* target = NULL;

    uint8_t* found = find_ootp_executable_pattern(expected, expected_size);
    if (found != NULL) {
        append_logf(
            "%s resolved by signature target=%p rva=0x%llx original_rva=0x%08X",
            label != NULL ? label : "patch target",
            found,
            (unsigned long long)((uintptr_t)found - (uintptr_t)exe),
            rva);
        return found;
    }

    if (target != NULL && memory_range_readable(target, expected_size)) {
        log_patch_bytes_mismatch(label != NULL ? label : "patch target", target, expected_size);
    } else {
        append_logf("%s target unreadable target=%p", label != NULL ? label : "patch target", target);
    }
    return NULL;
}

uint8_t* find_ootp_executable_pattern_nth(const uint8_t* pattern, size_t pattern_size, int desired_index, int expected_hits)
{
    if (pattern == NULL || pattern_size == 0 || desired_index < 0 || expected_hits <= desired_index) {
        return NULL;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }
    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return NULL;
    }
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return NULL;
    }

    uint8_t* found = NULL;
    int hits = 0;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;
    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < pattern_size) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)pattern_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (memcmp(candidate, pattern, pattern_size) != 0) {
                continue;
            }
            if (hits == desired_index) {
                found = candidate;
            }
            hits++;
        }
    }

    if (hits != expected_hits) {
        return NULL;
    }
    return found;
}

uint8_t* resolve_patch_target_by_rva_or_context_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const uint8_t* context,
    size_t context_size,
    size_t target_offset,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_size == 0 || context == NULL || context_size == 0) {
        return NULL;
    }
    if (target_offset > context_size || expected_size > context_size - target_offset) {
        append_logf("%s invalid context target_offset=%llu context_size=%llu expected_size=%llu",
            patch_label,
            (unsigned long long)target_offset,
            (unsigned long long)context_size,
            (unsigned long long)expected_size);
        return NULL;
    }

    uint8_t* target = NULL;

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_logf("%s context scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        append_logf("%s context scan failed: invalid NT header", patch_label);
        return NULL;
    }

    uint8_t* found = NULL;
    int hits = 0;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;
    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < context_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)context_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (memcmp(candidate, context, context_size) != 0) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                append_logf("%s context pattern ambiguous hits=%d first=%p another=%p",
                    patch_label,
                    hits,
                    found,
                    candidate);
                return NULL;
            }
        }
    }

    if (hits != 1 || found == NULL) {
        if (target != NULL && memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        } else {
            append_logf("%s target unreadable target=%p", patch_label, target);
        }
        append_logf("%s context pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
        return NULL;
    }

    target = found + target_offset;
    if (!memory_range_readable(target, expected_size) || memcmp(target, expected, expected_size) != 0) {
        append_logf("%s context resolved target failed expected-byte check target=%p context=%p",
            patch_label,
            target,
            found);
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        }
        return NULL;
    }

    append_logf(
        "%s resolved by context signature target=%p rva=0x%llx original_rva=0x%08X context=%p",
        patch_label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        rva,
        found);
    return target;
}

int kbo_memory_matches_masked_pattern(const uint8_t* data, const uint8_t* pattern, const uint8_t* mask, size_t size)
{
    if (data == NULL || pattern == NULL || mask == NULL || size == 0) {
        return 0;
    }
    for (size_t i = 0; i < size; i++) {
        if (mask[i] != 0 && data[i] != pattern[i]) {
            return 0;
        }
    }
    return 1;
}

uint8_t* resolve_patch_target_by_rva_or_masked_context_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    size_t expected_size,
    const uint8_t* context,
    const uint8_t* context_mask,
    size_t context_size,
    size_t target_offset,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_size == 0 || context == NULL || context_mask == NULL || context_size == 0) {
        return NULL;
    }
    if (target_offset > context_size || expected_size > context_size - target_offset) {
        append_logf("%s invalid masked context target_offset=%llu context_size=%llu expected_size=%llu",
            patch_label,
            (unsigned long long)target_offset,
            (unsigned long long)context_size,
            (unsigned long long)expected_size);
        return NULL;
    }

    uint8_t* target = NULL;

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_logf("%s masked context scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        append_logf("%s masked context scan failed: invalid NT header", patch_label);
        return NULL;
    }

    uint8_t* found = NULL;
    int hits = 0;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;
    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < context_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)context_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (!kbo_memory_matches_masked_pattern(candidate, context, context_mask, context_size)) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                append_logf("%s masked context pattern ambiguous hits=%d first=%p another=%p",
                    patch_label,
                    hits,
                    found,
                    candidate);
                return NULL;
            }
        }
    }

    if (hits != 1 || found == NULL) {
        if (target != NULL && memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        } else {
            append_logf("%s target unreadable target=%p", patch_label, target);
        }
        append_logf("%s masked context pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
        return NULL;
    }

    target = found + target_offset;
    if (!memory_range_readable(target, expected_size) || memcmp(target, expected, expected_size) != 0) {
        append_logf("%s masked context resolved target failed expected-byte check target=%p context=%p",
            patch_label,
            target,
            found);
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        }
        return NULL;
    }

    append_logf(
        "%s resolved by masked context signature target=%p rva=0x%llx original_rva=0x%08X context=%p",
        patch_label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        rva,
        found);
    return target;
}

uint8_t* resolve_patch_target_by_rva_or_masked_context_and_expected_pattern(
    HMODULE exe,
    uint32_t rva,
    const uint8_t* expected,
    const uint8_t* expected_mask,
    size_t expected_size,
    const uint8_t* context,
    const uint8_t* context_mask,
    size_t context_size,
    size_t target_offset,
    const char* label)
{
    const char* patch_label = label != NULL ? label : "patch target";
    if (exe == NULL || expected == NULL || expected_mask == NULL || expected_size == 0 || context == NULL || context_mask == NULL || context_size == 0) {
        return NULL;
    }
    if (target_offset > context_size || expected_size > context_size - target_offset) {
        append_logf("%s invalid masked context/expected target_offset=%llu context_size=%llu expected_size=%llu",
            patch_label,
            (unsigned long long)target_offset,
            (unsigned long long)context_size,
            (unsigned long long)expected_size);
        return NULL;
    }

    uint8_t* target = NULL;

    uint8_t* base = (uint8_t*)exe;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        append_logf("%s masked context/expected scan failed: invalid DOS header", patch_label);
        return NULL;
    }

    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        append_logf("%s masked context/expected scan failed: invalid NT header", patch_label);
        return NULL;
    }

    uint8_t* found = NULL;
    int hits = 0;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
    WORD num_sections = nt->FileHeader.NumberOfSections;
    for (WORD s = 0; s < num_sections; s++) {
        DWORD chars = sections[s].Characteristics;
        if (!(chars & IMAGE_SCN_MEM_EXECUTE) || !(chars & IMAGE_SCN_MEM_READ) || !(chars & IMAGE_SCN_CNT_CODE)) {
            continue;
        }

        uint8_t* section = base + sections[s].VirtualAddress;
        DWORD vsz = sections[s].Misc.VirtualSize;
        if (vsz < context_size || vsz < 32) {
            continue;
        }

        DWORD scan_limit = vsz - (DWORD)context_size;
        for (DWORD off = 0; off <= scan_limit; off++) {
            uint8_t* candidate = section + off;
            if (!kbo_memory_matches_masked_pattern(candidate, context, context_mask, context_size)) {
                continue;
            }

            hits++;
            if (hits == 1) {
                found = candidate;
            } else {
                append_logf("%s masked context/expected pattern ambiguous hits=%d first=%p another=%p",
                    patch_label,
                    hits,
                    found,
                    candidate);
                return NULL;
            }
        }
    }

    if (hits != 1 || found == NULL) {
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        } else {
            append_logf("%s target unreadable target=%p", patch_label, target);
        }
        append_logf("%s masked context/expected pattern not found hits=%d original_rva=0x%08X", patch_label, hits, rva);
        return NULL;
    }

    target = found + target_offset;
    if (!memory_range_readable(target, expected_size) || !kbo_memory_matches_masked_pattern(target, expected, expected_mask, expected_size)) {
        append_logf("%s masked context/expected resolved target failed expected-byte check target=%p context=%p",
            patch_label,
            target,
            found);
        if (memory_range_readable(target, expected_size)) {
            log_patch_bytes_mismatch(patch_label, target, expected_size);
        }
        return NULL;
    }

    append_logf(
        "%s resolved by masked context/expected signature target=%p rva=0x%llx original_rva=0x%08X context=%p",
        patch_label,
        target,
        (unsigned long long)((uintptr_t)target - (uintptr_t)exe),
        rva,
        found);
    return target;
}

int patch_static_bytes(const char* label, uint8_t* target, const uint8_t* expected, const uint8_t* patch, size_t size)
{
    if (memcmp(target, patch, size) == 0) {
        append_logf("%s already installed target=%p", label, target);
        return 1;
    }

    if (memcmp(target, expected, size) != 0) {
        log_patch_bytes_mismatch(label, target, size);
        return 0;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        append_logf("VirtualProtect failed for %s error=%lu", label, GetLastError());
        return 0;
    }

    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);

    DWORD ignored = 0;
    VirtualProtect(target, size, old_protect, &ignored);

    append_logf("installed %s target=%p", label, target);
    return 1;
}

void* resolve_relative_call_target(uint8_t* call_site)
{
    if (call_site == NULL || !memory_range_readable(call_site, 5) || call_site[0] != 0xE8) {
        return NULL;
    }

    int32_t rel = 0;
    memcpy(&rel, call_site + 1, sizeof(rel));
    return call_site + 5 + rel;
}

void* resolve_rip_relative_lea_target(uint8_t* instruction)
{
    if (instruction == NULL
        || !memory_range_readable(instruction, 7)
        || (instruction[0] != 0x48 && instruction[0] != 0x4C)
        || (instruction[1] != 0x8D && instruction[1] != 0x8B)
        || (instruction[2] & 0xC7u) != 0x05u) {
        return NULL;
    }

    int32_t rel = 0;
    memcpy(&rel, instruction + 3, sizeof(rel));
    return instruction + 7 + rel;
}
