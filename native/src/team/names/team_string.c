#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>

#include "team_string.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/abi/ootp_typedefs.h"
#include "../../build_verify/build_verify.h"
#include "../../core/dates/core_text_date.h"
#include "../../runtime_memory/runtime_memory.h"
/* String helpers (from league_team_rules) */

int copy_limited_ascii_string(const char* source, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (source == NULL || !memory_range_readable(source, 1)) {
        return 0;
    }

    size_t len = 0;
    for (; len + 1 < out_size && len < 512; len++) {
        if (!memory_range_readable(source + len, 1)) {
            break;
        }
        unsigned char c = (unsigned char)source[len];
        if (c == '\0') {
            break;
        }
        if (c < 0x20u || c > 0x7eu) {
            break;
        }
        out[len] = (char)c;
    }

    out[len] = '\0';
    return len > 0;
}

int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size)
{
    uint8_t* string_object = object_base + string_offset;
    uint8_t* text_slot     = string_object + OOTP27_KBO_STRING_OBJECT_TEXT_OFFSET;
    if (!memory_range_readable(text_slot, sizeof(char*))) {
        return 0;
    }

    const char* text = *(const char**)text_slot;
    return copy_limited_ascii_string(text, out, out_size);
}

static OotpCoreStringAssignFn get_ootp_string_assign_fn(void)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpCoreStringAssignFn assign_string =
        (OotpCoreStringAssignFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PISD_STRING_ASSIGN_RVA);
    if (!memory_range_readable((void*)assign_string, 16)) {
        return NULL;
    }

    return assign_string;
}

int assign_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, const char* text)
{
    if (object_base == NULL || text == NULL || text[0] == '\0') {
        return 0;
    }

    uint8_t* string_object = object_base + string_offset;
    if (!memory_range_readable(string_object, 0x18)) {
        return 0;
    }

    OotpCoreStringAssignFn assign_string = get_ootp_string_assign_fn();
    if (assign_string == NULL) {
        return 0;
    }

    assign_string(string_object, text);
    return 1;
}

int assign_ootp_string_object_text_if_different(uint8_t* object_base, uint32_t string_offset, const char* text)
{
    char current[128] = {0};
    if (copy_ootp_string_object_text(object_base, string_offset, current, sizeof(current))
            && ascii_equals_ignore_case(current, text)) {
        return 0;
    }

    return assign_ootp_string_object_text(object_base, string_offset, text);
}

int team_has_ootp_string_text(uint8_t* team, const char* expected)
{
    if (team == NULL || expected == NULL || expected[0] == '\0') {
        return 0;
    }

    static const uint32_t string_offsets[] = {
        0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u
    };

    char text[96] = {0};
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        text[0] = '\0';
        if (copy_ootp_string_object_text(team, string_offsets[i], text, sizeof(text))
                && ascii_equals_ignore_case(text, expected)) {
            return 1;
        }
    }

    return 0;
}


