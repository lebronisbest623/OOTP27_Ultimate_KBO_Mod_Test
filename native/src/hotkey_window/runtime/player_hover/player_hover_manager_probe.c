#include "player_hover_manager_probe.h"
#include <windows.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../bootstrap/abi/ootp_typedefs.h"
#include "../../../build_verify/build_verify.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PVOID volatile g_kbo_player_hover_manager_ptr = NULL;
static volatile LONG g_kbo_player_hover_manager_log_count = 0;
static volatile LONG g_kbo_player_hover_active = 0;
static volatile LONG g_kbo_player_hover_active_player = 0;
static volatile LONG64 g_kbo_player_hover_active_until_ms = 0;
static uintptr_t g_kbo_player_tooltip_text_append_original = 0u;
static uintptr_t g_kbo_player_tooltip_string_format_original = 0u;
static uintptr_t g_kbo_player_tooltip_rating_common_original = 0u;
static uintptr_t g_kbo_player_tooltip_rating_panel_ctor_original = 0u;
static volatile LONG g_kbo_player_tooltip_capture_lock = 0;
static volatile LONG g_kbo_player_tooltip_text_capture_active = 0;
static char g_kbo_player_tooltip_text_capture[12000] = {0};
static volatile LONG g_kbo_player_tooltip_text_capture_pos = 0;
static char g_kbo_player_tooltip_rating_capture[8192] = {0};
static volatile LONG g_kbo_player_tooltip_rating_capture_pos = 0;
static char g_kbo_player_tooltip_builder_capture[8192] = {0};
static volatile LONG g_kbo_player_tooltip_builder_capture_pos = 0;
static volatile LONG64 g_kbo_player_tooltip_string_format_call_count = 0;
static volatile LONG64 g_kbo_player_tooltip_string_format_active_call_count = 0;
static volatile LONG64 g_kbo_player_tooltip_string_format_numeric_call_count = 0;
static volatile LONG64 g_kbo_player_tooltip_rating_common_call_count = 0;
static volatile LONG64 g_kbo_player_tooltip_rating_common_active_call_count = 0;
static volatile LONG64 g_kbo_player_tooltip_rating_panel_ctor_call_count = 0;
static volatile LONG64 g_kbo_player_tooltip_rating_panel_ctor_active_call_count = 0;

#define KBO_TOOLTIP_OBJECT_BYTES 0xD20u
#define KBO_TOOLTIP_SCAN_STRING_MAX 160u
#define KBO_TOOLTIP_POINTER_SCAN_BYTES 0x500u
#define KBO_TOOLTIP_POINTER_SCAN_LIMIT 96u
#define KBO_TOOLTIP_RATING_PANEL_FIRST_OFFSET 0x830u
#define KBO_TOOLTIP_RATING_PANEL_LAST_OFFSET  0x858u

typedef int (*OotpPlayerHoverManagerFn)(uintptr_t manager_ptr);
typedef void* (__fastcall *OotpPlayerTooltipFactoryFn)(
    void* tooltip_object,
    uint32_t player_id,
    uint32_t y,
    uint32_t x,
    void* source_ui_object,
    uint8_t compact_flag);
typedef void (__fastcall *OotpPlayerTooltipRenderFn)(void* tooltip_object, uint16_t line_count);
typedef void (__fastcall *OotpPlayerTooltipAttachFn)(uintptr_t manager_ptr, void* tooltip_object, uint8_t visible);
typedef void* (__fastcall *OotpPlayerTooltipTextAppendFn)(
    const char* text,
    uint8_t mode,
    char* out,
    uint8_t flags,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uintptr_t arg9,
    uintptr_t arg10);
typedef void* (__fastcall *OotpPlayerTooltipStringFormatFn)(
    void* text_object,
    const char* format,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uintptr_t arg9,
    uintptr_t arg10);
typedef uint8_t (__fastcall *OotpPlayerTooltipRatingCommonFn)(
    void* container,
    uint16_t column,
    uint16_t row,
    uint16_t displayed_rating,
    uint8_t is_potential,
    uint8_t mirror_flag);
typedef void* (__fastcall *OotpPlayerTooltipRatingPanelCtorFn)(
    void* panel_object,
    uint16_t current_rating,
    uint16_t potential_rating,
    uint8_t potential_only);

uintptr_t kbo_player_hover_manager_ptr(void)
{
    return (uintptr_t)g_kbo_player_hover_manager_ptr;
}

void kbo_set_player_tooltip_text_append_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_text_append_original = original_func_ptr;
}

void kbo_set_player_tooltip_string_format_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_string_format_original = original_func_ptr;
}

void kbo_set_player_tooltip_rating_common_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_rating_common_original = original_func_ptr;
}

void kbo_set_player_tooltip_rating_panel_ctor_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_rating_panel_ctor_original = original_func_ptr;
}

static void kbo_tooltip_text_capture_reset(void)
{
    g_kbo_player_tooltip_text_capture[0] = '\0';
    g_kbo_player_tooltip_rating_capture[0] = '\0';
    g_kbo_player_tooltip_builder_capture[0] = '\0';
    InterlockedExchange(&g_kbo_player_tooltip_text_capture_pos, 0);
    InterlockedExchange(&g_kbo_player_tooltip_rating_capture_pos, 0);
    InterlockedExchange(&g_kbo_player_tooltip_builder_capture_pos, 0);
}

static void kbo_tooltip_text_capture_append_line(const char* text)
{
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_text_capture_active, 0, 0) == 0
            || text == NULL
            || !memory_range_readable((uint8_t*)text, 1u)
            || text[0] == '\0') {
        return;
    }

    size_t len = 0u;
    while (len < 180u && memory_range_readable((uint8_t*)text + len, 1u) && text[len] != '\0') {
        unsigned char ch = (unsigned char)text[len];
        if (ch < 0x20u && ch != '\t') {
            break;
        }
        len += 1u;
    }
    if (len < 1u) {
        return;
    }

    LONG start = InterlockedExchangeAdd(&g_kbo_player_tooltip_text_capture_pos, (LONG)(len + 1u));
    if (start < 0 || (size_t)start + len + 2u >= sizeof(g_kbo_player_tooltip_text_capture)) {
        return;
    }

    memcpy(g_kbo_player_tooltip_text_capture + start, text, len);
    g_kbo_player_tooltip_text_capture[start + (LONG)len] = '\n';
    g_kbo_player_tooltip_text_capture[start + (LONG)len + 1] = '\0';
}

__declspec(noinline) void* ootp_kbo_player_tooltip_text_append_probe_wrapper(
    const char* text,
    uint8_t mode,
    char* out,
    uint8_t flags,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uintptr_t arg9,
    uintptr_t arg10)
{
    kbo_tooltip_text_capture_append_line(text);

    OotpPlayerTooltipTextAppendFn original =
        (OotpPlayerTooltipTextAppendFn)g_kbo_player_tooltip_text_append_original;
    if (original == NULL) {
        return out;
    }
    return original(text, mode, out, flags, arg5, arg6, arg7, arg8, arg9, arg10);
}

static int kbo_tooltip_copy_safe_c_string(const char* text, char* out, size_t out_size)
{
    size_t len = 0u;
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (text == NULL || !memory_range_readable((uint8_t*)text, 1u)) {
        return 0;
    }
    while (len + 1u < out_size
            && len < 96u
            && memory_range_readable((uint8_t*)text + len, 1u)
            && text[len] != '\0') {
        unsigned char ch = (unsigned char)text[len];
        if (ch < 0x20u && ch != '\t') {
            break;
        }
        out[len++] = (char)ch;
    }
    out[len] = '\0';
    return len > 0u;
}

static int kbo_tooltip_copy_first_inline_digit_string(void* object, char* out, size_t out_size)
{
    uint8_t* bytes = (uint8_t*)object;
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (bytes == NULL || !memory_range_readable(bytes, 0x120u)) {
        return 0;
    }

    for (size_t i = 0u; i + 1u < 0x120u; ++i) {
        if (!isdigit((unsigned char)bytes[i])) {
            continue;
        }

        size_t j = i;
        while (j < 0x120u && isdigit((unsigned char)bytes[j]) && j - i < out_size - 1u) {
            ++j;
        }
        if (j == i || (j < 0x120u && isdigit((unsigned char)bytes[j]))) {
            continue;
        }
        if (j < 0x120u && bytes[j] != 0u) {
            continue;
        }

        memcpy(out, bytes + i, j - i);
        out[j - i] = '\0';
        return 1;
    }
    return 0;
}

static void kbo_tooltip_rating_capture_append(
    const char* format,
    void* result_object,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uintptr_t arg9,
    uintptr_t arg10)
{
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_text_capture_active, 0, 0) == 0) {
        return;
    }

    char format_copy[128] = {0};
    char text_copy[64] = {0};
    if (!kbo_tooltip_copy_safe_c_string(format, format_copy, sizeof(format_copy))) {
        return;
    }
    if (strcmp(format_copy, "%c%hd,%hd,%hd,%hd%c") == 0) {
        return;
    }

    int numeric_format =
        strstr(format_copy, "%d") != NULL
        || strstr(format_copy, "%i") != NULL
        || strstr(format_copy, "%u") != NULL
        || strstr(format_copy, "%hd") != NULL
        || strstr(format_copy, "%hu") != NULL;
    if (numeric_format) {
        InterlockedIncrement64(&g_kbo_player_tooltip_string_format_numeric_call_count);
    }

    (void)kbo_tooltip_copy_first_inline_digit_string(result_object, text_copy, sizeof(text_copy));
    int value_hint = 0;
    if (strcmp(format_copy, "%d") == 0 || strcmp(format_copy, "%i") == 0 || strcmp(format_copy, "%u") == 0) {
        value_hint = (int)(int32_t)(arg3 & 0xffffffffu);
    } else if (strcmp(format_copy, "%hd") == 0 || strcmp(format_copy, "%hu") == 0) {
        value_hint = (int)(int16_t)(arg3 & 0xffffu);
    }

    char line[320] = {0};
    int written = snprintf(
        line,
        sizeof(line),
        "fmt numeric=%d format=\"%s\" text=\"%s\" value=%d args=%d,%d,%d,%d,%d,%d,%d,%d\n",
        numeric_format,
        format_copy,
        text_copy,
        value_hint,
        (int)(int16_t)(arg3 & 0xffffu),
        (int)(int16_t)(arg4 & 0xffffu),
        (int)(int16_t)(arg5 & 0xffffu),
        (int)(int16_t)(arg6 & 0xffffu),
        (int)(int16_t)(arg7 & 0xffffu),
        (int)(int16_t)(arg8 & 0xffffu),
        (int)(int16_t)(arg9 & 0xffffu),
        (int)(int16_t)(arg10 & 0xffffu));
    if (written <= 0) {
        return;
    }

    LONG start = InterlockedExchangeAdd(&g_kbo_player_tooltip_rating_capture_pos, (LONG)written);
    if (start < 0 || (size_t)start + (size_t)written + 1u >= sizeof(g_kbo_player_tooltip_rating_capture)) {
        return;
    }

    memcpy(g_kbo_player_tooltip_rating_capture + start, line, (size_t)written);
    g_kbo_player_tooltip_rating_capture[start + written] = '\0';
}

__declspec(noinline) void* ootp_kbo_player_tooltip_string_format_probe_wrapper(
    void* text_object,
    const char* format,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uintptr_t arg9,
    uintptr_t arg10)
{
    InterlockedIncrement64(&g_kbo_player_tooltip_string_format_call_count);
    OotpPlayerTooltipStringFormatFn original =
        (OotpPlayerTooltipStringFormatFn)g_kbo_player_tooltip_string_format_original;
    if (original == NULL) {
        return text_object;
    }
    void* result = original(text_object, format, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_text_capture_active, 0, 0) != 0) {
        InterlockedIncrement64(&g_kbo_player_tooltip_string_format_active_call_count);
        kbo_tooltip_rating_capture_append(format, result, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    }
    return result;
}

static uint32_t kbo_tooltip_return_rva(void* return_address)
{
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t base = (uintptr_t)exe;
    uintptr_t addr = (uintptr_t)return_address;
    if (base == 0u || addr < base || addr - base > 0xffffffffu) {
        return 0u;
    }
    return (uint32_t)(addr - base);
}

static void kbo_tooltip_builder_capture_append(const char* line)
{
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_text_capture_active, 0, 0) == 0
            || line == NULL
            || line[0] == '\0') {
        return;
    }

    size_t len = strlen(line);
    if (len == 0u || len > 360u) {
        return;
    }

    LONG start = InterlockedExchangeAdd(&g_kbo_player_tooltip_builder_capture_pos, (LONG)(len + 1u));
    if (start < 0 || (size_t)start + len + 2u >= sizeof(g_kbo_player_tooltip_builder_capture)) {
        return;
    }

    memcpy(g_kbo_player_tooltip_builder_capture + start, line, len);
    g_kbo_player_tooltip_builder_capture[start + (LONG)len] = '\n';
    g_kbo_player_tooltip_builder_capture[start + (LONG)len + 1] = '\0';
}

__declspec(noinline) uint8_t ootp_kbo_player_tooltip_rating_common_probe_wrapper(
    void* container,
    uint16_t column,
    uint16_t row,
    uint16_t displayed_rating,
    uint8_t is_potential,
    uint8_t mirror_flag)
{
    InterlockedIncrement64(&g_kbo_player_tooltip_rating_common_call_count);
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_text_capture_active, 0, 0) != 0) {
        char line[256] = {0};
        uint32_t ret_rva = kbo_tooltip_return_rva(__builtin_return_address(0));
        InterlockedIncrement64(&g_kbo_player_tooltip_rating_common_active_call_count);
        snprintf(
            line,
            sizeof(line),
            "rating_common kind=%s display=%u column=%u row=%u mirror=%u ret_rva=0x%08X",
            is_potential ? "POT" : "OVR",
            (unsigned int)displayed_rating,
            (unsigned int)column,
            (unsigned int)row,
            (unsigned int)mirror_flag,
            ret_rva);
        kbo_tooltip_builder_capture_append(line);
    }

    OotpPlayerTooltipRatingCommonFn original =
        (OotpPlayerTooltipRatingCommonFn)g_kbo_player_tooltip_rating_common_original;
    if (original == NULL) {
        return 0u;
    }
    return original(container, column, row, displayed_rating, is_potential, mirror_flag);
}

__declspec(noinline) void* ootp_kbo_player_tooltip_rating_panel_ctor_probe_wrapper(
    void* panel_object,
    uint16_t current_rating,
    uint16_t potential_rating,
    uint8_t potential_only)
{
    InterlockedIncrement64(&g_kbo_player_tooltip_rating_panel_ctor_call_count);
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_text_capture_active, 0, 0) != 0) {
        char line[256] = {0};
        uint32_t ret_rva = kbo_tooltip_return_rva(__builtin_return_address(0));
        InterlockedIncrement64(&g_kbo_player_tooltip_rating_panel_ctor_active_call_count);
        snprintf(
            line,
            sizeof(line),
            "rating_panel_ctor current=%u potential=%u potential_only=%u ret_rva=0x%08X",
            (unsigned int)current_rating,
            (unsigned int)potential_rating,
            (unsigned int)potential_only,
            ret_rva);
        kbo_tooltip_builder_capture_append(line);
    }

    OotpPlayerTooltipRatingPanelCtorFn original =
        (OotpPlayerTooltipRatingPanelCtorFn)g_kbo_player_tooltip_rating_panel_ctor_original;
    if (original == NULL) {
        return panel_object;
    }
    return original(panel_object, current_rating, potential_rating, potential_only);
}

static int kbo_tooltip_parse_builder_display_value(const char* line, const char* kind, int* out_value)
{
    char needle[48] = {0};
    if (line == NULL || kind == NULL || out_value == NULL) {
        return 0;
    }
    *out_value = 0;
    snprintf(needle, sizeof(needle), "rating_common kind=%s display=", kind);
    const char* value_text = strstr(line, needle);
    if (value_text == NULL) {
        return 0;
    }
    value_text += strlen(needle);

    char* end = NULL;
    long value = strtol(value_text, &end, 10);
    if (end == value_text || value <= 0 || value > 500) {
        return 0;
    }
    *out_value = (int)value;
    return 1;
}

static int kbo_tooltip_extract_overall_potential_from_builder_capture(
    int* out_overall,
    int* out_potential)
{
    const char* cursor = g_kbo_player_tooltip_builder_capture;
    int overall = 0;
    int potential = 0;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (out_overall == NULL || out_potential == NULL || cursor[0] == '\0') {
        return 0;
    }

    while (*cursor != '\0' && (overall == 0 || potential == 0)) {
        const char* line_end = strchr(cursor, '\n');
        size_t line_len = line_end != NULL ? (size_t)(line_end - cursor) : strlen(cursor);
        if (line_len > 0u && line_len < 280u) {
            char line[280] = {0};
            int value = 0;
            memcpy(line, cursor, line_len);
            if (overall == 0 && kbo_tooltip_parse_builder_display_value(line, "OVR", &value)) {
                overall = value;
            } else if (potential == 0 && kbo_tooltip_parse_builder_display_value(line, "POT", &value)) {
                potential = value;
            }
        }
        if (line_end == NULL) {
            break;
        }
        cursor = line_end + 1;
    }

    if (overall == 0 || potential == 0) {
        return 0;
    }
    *out_overall = overall;
    *out_potential = potential;
    return 1;
}

static int kbo_tooltip_parse_format_capture_value(const char* line, int* out_value)
{
    if (line == NULL || out_value == NULL) {
        return 0;
    }
    *out_value = 0;
    if (strstr(line, "numeric=1") == NULL
            || strstr(line, "format=\"%c%hd,%hd,%hd,%hd%c\"") != NULL) {
        return 0;
    }

    const char* text = strstr(line, "text=\"");
    if (text != NULL) {
        text += 6;
        char* end = NULL;
        long value = strtol(text, &end, 10);
        if (end != text && end != NULL && *end == '"' && value > 0 && value <= 500) {
            *out_value = (int)value;
            return 1;
        }
    }

    const char* value_hint = strstr(line, "value=");
    if (value_hint != NULL) {
        value_hint += 6;
        char* end = NULL;
        long value = strtol(value_hint, &end, 10);
        if (end != value_hint && value > 0 && value <= 500) {
            *out_value = (int)value;
            return 1;
        }
    }
    return 0;
}

static int kbo_tooltip_extract_overall_potential_from_format_capture(
    int* out_overall,
    int* out_potential)
{
    const char* cursor = g_kbo_player_tooltip_rating_capture;
    int values[2] = {0};
    int count = 0;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (out_overall == NULL || out_potential == NULL || cursor[0] == '\0') {
        return 0;
    }

    while (*cursor != '\0' && count < 2) {
        const char* line_end = strchr(cursor, '\n');
        size_t line_len = line_end != NULL ? (size_t)(line_end - cursor) : strlen(cursor);
        if (line_len > 0u && line_len < 360u) {
            char line[360] = {0};
            memcpy(line, cursor, line_len);
            int value = 0;
            if (kbo_tooltip_parse_format_capture_value(line, &value)) {
                values[count++] = value;
            }
        }
        if (line_end == NULL) {
            break;
        }
        cursor = line_end + 1;
    }

    if (count < 2) {
        return 0;
    }
    *out_overall = values[0];
    *out_potential = values[1];
    return 1;
}

static void kbo_tooltip_appendf(char* out, size_t out_size, size_t* pos, const char* fmt, ...)
{
    if (out == NULL || out_size == 0u || pos == NULL || *pos >= out_size) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(out + *pos, out_size - *pos, fmt, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    size_t advance = (size_t)written;
    if (advance >= out_size - *pos) {
        *pos = out_size - 1u;
        out[*pos] = '\0';
        return;
    }

    *pos += advance;
}

static int kbo_tooltip_is_ascii_text_byte(uint8_t ch)
{
    return ch == '\t' || ch == ' ' || (ch >= 0x21u && ch <= 0x7eu);
}

static void kbo_tooltip_append_ascii_string(
    char* out,
    size_t out_size,
    size_t* pos,
    const char* label,
    size_t offset,
    const uint8_t* text,
    size_t max_len)
{
    char scratch[KBO_TOOLTIP_SCAN_STRING_MAX + 1u] = {0};
    size_t len = 0u;
    while (len < KBO_TOOLTIP_SCAN_STRING_MAX && len < max_len && kbo_tooltip_is_ascii_text_byte(text[len])) {
        scratch[len] = (char)text[len];
        len += 1u;
    }
    scratch[len] = '\0';
    if (len >= 3u) {
        kbo_tooltip_appendf(out, out_size, pos, "%s+0x%04Ix: %s\n", label, offset, scratch);
    }
}

static void kbo_tooltip_append_utf16_string(
    char* out,
    size_t out_size,
    size_t* pos,
    const char* label,
    size_t offset,
    const uint8_t* text,
    size_t max_bytes)
{
    char scratch[KBO_TOOLTIP_SCAN_STRING_MAX + 1u] = {0};
    size_t len = 0u;
    while ((len + 1u) * sizeof(uint16_t) <= max_bytes && len < KBO_TOOLTIP_SCAN_STRING_MAX) {
        uint16_t ch = *(const uint16_t*)(text + len * sizeof(uint16_t));
        if (ch == 0u || ch > 0x7eu || !kbo_tooltip_is_ascii_text_byte((uint8_t)ch)) {
            break;
        }
        scratch[len] = (char)ch;
        len += 1u;
    }
    scratch[len] = '\0';
    if (len >= 3u) {
        kbo_tooltip_appendf(out, out_size, pos, "%s+0x%04Ix utf16: %s\n", label, offset, scratch);
    }
}

static void kbo_tooltip_scan_inline_strings(
    const char* label,
    uint8_t* base,
    size_t bytes,
    char* out,
    size_t out_size,
    size_t* pos)
{
    if (base == NULL || !memory_range_readable(base, bytes)) {
        return;
    }

    for (size_t i = 0u; i + 3u < bytes;) {
        if (kbo_tooltip_is_ascii_text_byte(base[i])
                && kbo_tooltip_is_ascii_text_byte(base[i + 1u])
                && kbo_tooltip_is_ascii_text_byte(base[i + 2u])) {
            kbo_tooltip_append_ascii_string(out, out_size, pos, label, i, base + i, bytes - i);
            while (i < bytes && kbo_tooltip_is_ascii_text_byte(base[i])) {
                i += 1u;
            }
            continue;
        }
        if (i + 6u < bytes
                && kbo_tooltip_is_ascii_text_byte(base[i])
                && base[i + 1u] == 0u
                && kbo_tooltip_is_ascii_text_byte(base[i + 2u])
                && base[i + 3u] == 0u
                && kbo_tooltip_is_ascii_text_byte(base[i + 4u])
                && base[i + 5u] == 0u) {
            kbo_tooltip_append_utf16_string(out, out_size, pos, label, i, base + i, bytes - i);
            while (i + 1u < bytes && base[i] != 0u && base[i + 1u] == 0u) {
                i += 2u;
            }
            continue;
        }
        i += 1u;
    }
}

static void kbo_tooltip_scan_pointer_strings(
    void* tooltip,
    char* out,
    size_t out_size,
    size_t* pos)
{
    uint8_t* base = (uint8_t*)tooltip;
    if (!memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return;
    }

    for (size_t i = 0u; i + sizeof(uintptr_t) <= KBO_TOOLTIP_OBJECT_BYTES; i += sizeof(uintptr_t)) {
        uintptr_t ptr = *(uintptr_t*)(base + i);
        if (ptr < 0x10000u) {
            continue;
        }
        uint8_t* target = (uint8_t*)ptr;
        if (memory_range_readable(target, 96u)) {
            if (kbo_tooltip_is_ascii_text_byte(target[0]) && kbo_tooltip_is_ascii_text_byte(target[1])
                    && kbo_tooltip_is_ascii_text_byte(target[2])) {
                kbo_tooltip_append_ascii_string(out, out_size, pos, "ptr", i, target, 96u);
            } else if (target[1] == 0u && target[3] == 0u && target[5] == 0u
                    && kbo_tooltip_is_ascii_text_byte(target[0])
                    && kbo_tooltip_is_ascii_text_byte(target[2])
                    && kbo_tooltip_is_ascii_text_byte(target[4])) {
                kbo_tooltip_append_utf16_string(out, out_size, pos, "ptr", i, target, 96u);
            }
        }
    }
}

static int kbo_tooltip_seen_ptr(uintptr_t* seen, size_t seen_count, uintptr_t ptr)
{
    for (size_t i = 0u; i < seen_count; ++i) {
        if (seen[i] == ptr) {
            return 1;
        }
    }
    return 0;
}

static void kbo_tooltip_scan_nested_pointer_strings(
    void* tooltip,
    char* out,
    size_t out_size,
    size_t* pos)
{
    uint8_t* base = (uint8_t*)tooltip;
    uintptr_t seen[KBO_TOOLTIP_POINTER_SCAN_LIMIT] = {0};
    size_t seen_count = 0u;
    if (!memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return;
    }

    for (size_t i = 0u; i + sizeof(uintptr_t) <= KBO_TOOLTIP_OBJECT_BYTES
            && seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT; i += sizeof(uintptr_t)) {
        uintptr_t ptr = *(uintptr_t*)(base + i);
        if (ptr < 0x10000u || kbo_tooltip_seen_ptr(seen, seen_count, ptr)) {
            continue;
        }

        uint8_t* child = (uint8_t*)ptr;
        if (!memory_range_readable(child, KBO_TOOLTIP_POINTER_SCAN_BYTES)) {
            continue;
        }
        seen[seen_count++] = ptr;

        char label[64] = {0};
        snprintf(label, sizeof(label), "child[0x%04llx]", (unsigned long long)i);
        kbo_tooltip_scan_inline_strings(label, child, KBO_TOOLTIP_POINTER_SCAN_BYTES, out, out_size, pos);

        for (size_t j = 0u; j + sizeof(uintptr_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES
                && seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT; j += sizeof(uintptr_t)) {
            uintptr_t nested_ptr = *(uintptr_t*)(child + j);
            if (nested_ptr < 0x10000u || kbo_tooltip_seen_ptr(seen, seen_count, nested_ptr)) {
                continue;
            }

            uint8_t* nested = (uint8_t*)nested_ptr;
            if (!memory_range_readable(nested, 160u)) {
                continue;
            }
            seen[seen_count++] = nested_ptr;

            if (kbo_tooltip_is_ascii_text_byte(nested[0])
                    && kbo_tooltip_is_ascii_text_byte(nested[1])
                    && kbo_tooltip_is_ascii_text_byte(nested[2])) {
                snprintf(label, sizeof(label), "child[0x%04llx]->0x%04llx", (unsigned long long)i, (unsigned long long)j);
                kbo_tooltip_append_ascii_string(out, out_size, pos, label, 0u, nested, 160u);
            } else if (nested[1] == 0u && nested[3] == 0u && nested[5] == 0u
                    && kbo_tooltip_is_ascii_text_byte(nested[0])
                    && kbo_tooltip_is_ascii_text_byte(nested[2])
                    && kbo_tooltip_is_ascii_text_byte(nested[4])) {
                snprintf(label, sizeof(label), "child[0x%04llx]->0x%04llx", (unsigned long long)i, (unsigned long long)j);
                kbo_tooltip_append_utf16_string(out, out_size, pos, label, 0u, nested, 160u);
            }
        }
    }
}

static uint16_t kbo_tooltip_read_u16(uint8_t* base, size_t offset)
{
    return memory_range_readable(base + offset, sizeof(uint16_t)) ? *(uint16_t*)(base + offset) : 0u;
}

static int16_t kbo_tooltip_read_i16(uint8_t* base, size_t offset)
{
    return memory_range_readable(base + offset, sizeof(int16_t)) ? *(int16_t*)(base + offset) : 0;
}

static uint8_t kbo_tooltip_read_u8(uint8_t* base, size_t offset)
{
    return memory_range_readable(base + offset, sizeof(uint8_t)) ? *(uint8_t*)(base + offset) : 0u;
}

static void kbo_tooltip_scan_rating_panels(
    void* tooltip,
    char* out,
    size_t out_size,
    size_t* pos)
{
    uint8_t* base = (uint8_t*)tooltip;
    if (!memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return;
    }

    int index = 0;
    for (size_t offset = KBO_TOOLTIP_RATING_PANEL_FIRST_OFFSET;
            offset <= KBO_TOOLTIP_RATING_PANEL_LAST_OFFSET;
            offset += sizeof(uintptr_t), ++index) {
        uintptr_t ptr = *(uintptr_t*)(base + offset);
        if (ptr < 0x10000u || !memory_range_readable((uint8_t*)ptr, 0x100u)) {
            kbo_tooltip_appendf(out, out_size, pos, "panel%d tooltip+0x%04Ix ptr=NULL\n", index, offset);
            continue;
        }

        uint8_t* panel = (uint8_t*)ptr;
        uint16_t scale_a = kbo_tooltip_read_u16(panel, 0xe8u);
        uint16_t scale_b = kbo_tooltip_read_u16(panel, 0xeau);
        int16_t shown_a = kbo_tooltip_read_i16(panel, 0xecu);
        int16_t shown_b = kbo_tooltip_read_i16(panel, 0xeeu);
        uint8_t visible_a = kbo_tooltip_read_u8(panel, 0xf1u);
        uint8_t visible_b = kbo_tooltip_read_u8(panel, 0xf2u);
        uint8_t mode = kbo_tooltip_read_u8(panel, 0xf6u);
        kbo_tooltip_appendf(
            out,
            out_size,
            pos,
            "panel%d tooltip+0x%04Ix ptr=%p scale_a=%u shown_a=%d scale_b=%u shown_b=%d visible_a=%u visible_b=%u mode=%u\n",
            index,
            offset,
            (void*)ptr,
            (unsigned int)scale_a,
            (int)shown_a,
            (unsigned int)scale_b,
            (int)shown_b,
            (unsigned int)visible_a,
            (unsigned int)visible_b,
            (unsigned int)mode);
    }
}

static int kbo_tooltip_child_contains_any_rating_label(uint8_t* child, size_t bytes)
{
    if (child == NULL || !memory_range_readable(child, bytes)) {
        return 0;
    }

    for (size_t i = 0u; i + 4u < bytes; ++i) {
        if (memcmp(child + i, "OVR", 3u) == 0) {
            return 1;
        } else if (memcmp(child + i, "POT", 3u) == 0) {
            return 1;
        } else if (memcmp(child + i, "STU", 3u) == 0
                || memcmp(child + i, "MOV", 3u) == 0
                || memcmp(child + i, "CON", 3u) == 0
                || memcmp(child + i, "STA", 3u) == 0) {
            return 1;
        }
    }
    return 0;
}

static int kbo_tooltip_child_contains_ascii(uint8_t* child, size_t bytes, const char* needle)
{
    size_t needle_len = 0u;
    if (child == NULL || needle == NULL || !memory_range_readable(child, bytes)) {
        return 0;
    }

    needle_len = strlen(needle);
    if (needle_len == 0u || needle_len > bytes) {
        return 0;
    }

    for (size_t i = 0u; i + needle_len <= bytes; ++i) {
        if (memcmp(child + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static size_t kbo_tooltip_find_ascii(uint8_t* bytes, size_t byte_count, const char* needle)
{
    size_t needle_len = 0u;
    if (bytes == NULL || needle == NULL || !memory_range_readable(bytes, byte_count)) {
        return SIZE_MAX;
    }

    needle_len = strlen(needle);
    if (needle_len == 0u || needle_len > byte_count) {
        return SIZE_MAX;
    }

    for (size_t i = 0u; i + needle_len <= byte_count; ++i) {
        if (memcmp(bytes + i, needle, needle_len) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static int kbo_tooltip_parse_aligned_text_record_number(
    uint8_t* child,
    size_t bytes,
    size_t offset,
    int* out_value,
    int* out_decimal)
{
    int value = 0;
    size_t i = offset;
    int digits = 0;
    if (out_value != NULL) { *out_value = 0; }
    if (out_decimal != NULL) { *out_decimal = 0; }
    if (child == NULL || out_value == NULL || offset >= bytes || !memory_range_readable(child, bytes)) {
        return 0;
    }

    if ((offset & 0x0fu) != 0u || !isdigit((unsigned char)child[offset])) {
        return 0;
    }

    while (i < bytes && isdigit((unsigned char)child[i]) && digits < 4) {
        value = (value * 10) + (int)(child[i] - (uint8_t)'0');
        ++i;
        ++digits;
    }

    if (i < bytes && child[i] == '.') {
        if (out_decimal != NULL) { *out_decimal = 1; }
        return 0;
    }

    if (digits == 0 || digits > 3 || (i < bytes && isdigit((unsigned char)child[i]))) {
        return 0;
    }
    if (i < bytes && child[i] != 0u) {
        return 0;
    }
    if (value <= 0) {
        return 0;
    }

    *out_value = value;
    return 1;
}

static int kbo_tooltip_collect_numbers_after_label(
    uint8_t* child,
    size_t bytes,
    const char* label,
    int* out_values,
    size_t out_values_count)
{
    size_t label_pos = SIZE_MAX;
    size_t scan_start = 0u;
    size_t scan_end = 0u;
    size_t count = 0u;
    if (child == NULL || label == NULL || out_values == NULL || out_values_count == 0u) {
        return 0;
    }

    label_pos = kbo_tooltip_find_ascii(child, bytes, label);
    if (label_pos == SIZE_MAX) {
        return 0;
    }

    scan_start = (label_pos + strlen(label) + 0x0fu) & ~(size_t)0x0f;
    scan_end = label_pos + 0x240u;
    if (scan_end > bytes) {
        scan_end = bytes;
    }

    for (size_t i = scan_start; i + 4u <= scan_end && count < out_values_count; i += 0x10u) {
        int value = 0;
        int decimal = 0;
        if (!kbo_tooltip_parse_aligned_text_record_number(child, bytes, i, &value, &decimal)) {
            if (decimal && count > 0u) {
                break;
            }
            continue;
        }
        out_values[count++] = value;
    }
    return (int)count;
}

static int kbo_tooltip_extract_overall_potential_from_child(
    uint8_t* child,
    size_t bytes,
    int* out_overall,
    int* out_potential)
{
    int ovr_values[2] = {0};
    int pot_values[2] = {0};
    int ovr_count = 0;
    int pot_count = 0;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (child == NULL || out_overall == NULL || out_potential == NULL) {
        return 0;
    }

    ovr_count = kbo_tooltip_collect_numbers_after_label(
        child,
        bytes,
        "OVR",
        ovr_values,
        sizeof(ovr_values) / sizeof(ovr_values[0]));
    pot_count = kbo_tooltip_collect_numbers_after_label(
        child,
        bytes,
        "POT",
        pot_values,
        sizeof(pot_values) / sizeof(pot_values[0]));

    if (ovr_count >= 1 && pot_count >= 1) {
        *out_overall = ovr_values[0];
        *out_potential = pot_values[0];
        return 1;
    }

    if (pot_count >= 2) {
        *out_overall = pot_values[0];
        *out_potential = pot_values[1];
        return 1;
    }

    if (ovr_count >= 2) {
        *out_overall = ovr_values[0];
        *out_potential = ovr_values[1];
        return 1;
    }

    return 0;
}

static int kbo_tooltip_extract_overall_potential(
    void* tooltip,
    int* out_overall,
    int* out_potential)
{
    uint8_t* base = (uint8_t*)tooltip;
    uintptr_t seen[KBO_TOOLTIP_POINTER_SCAN_LIMIT] = {0};
    size_t seen_count = 0u;
    if (out_overall != NULL) { *out_overall = 0; }
    if (out_potential != NULL) { *out_potential = 0; }
    if (tooltip == NULL || out_overall == NULL || out_potential == NULL
            || !memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return 0;
    }

    for (size_t i = 0u; i + sizeof(uintptr_t) <= KBO_TOOLTIP_OBJECT_BYTES
            && seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT; i += sizeof(uintptr_t)) {
        uintptr_t ptr = *(uintptr_t*)(base + i);
        if (ptr < 0x10000u || kbo_tooltip_seen_ptr(seen, seen_count, ptr)) {
            continue;
        }
        uint8_t* child = (uint8_t*)ptr;
        if (!memory_range_readable(child, KBO_TOOLTIP_POINTER_SCAN_BYTES)) {
            continue;
        }
        seen[seen_count++] = ptr;
        if (kbo_tooltip_extract_overall_potential_from_child(
                    child,
                    KBO_TOOLTIP_POINTER_SCAN_BYTES,
                    out_overall,
                    out_potential)) {
            return 1;
        }

        for (size_t j = 0u; j + sizeof(uintptr_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uintptr_t)) {
            uintptr_t nested_ptr = *(uintptr_t*)(child + j);
            if (nested_ptr < 0x10000u || kbo_tooltip_seen_ptr(seen, seen_count, nested_ptr)) {
                continue;
            }
            uint8_t* nested = (uint8_t*)nested_ptr;
            if (!memory_range_readable(nested, KBO_TOOLTIP_POINTER_SCAN_BYTES)) {
                continue;
            }
            if (seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT) {
                seen[seen_count++] = nested_ptr;
            }
            if (kbo_tooltip_extract_overall_potential_from_child(
                        nested,
                        KBO_TOOLTIP_POINTER_SCAN_BYTES,
                        out_overall,
                        out_potential)) {
                return 1;
            }
        }
    }
    return 0;
}

static void kbo_tooltip_scan_rating_child_values(
    void* tooltip,
    char* out,
    size_t out_size,
    size_t* pos)
{
    uint8_t* base = (uint8_t*)tooltip;
    uintptr_t seen[KBO_TOOLTIP_POINTER_SCAN_LIMIT] = {0};
    size_t seen_count = 0u;
    if (!memory_range_readable(base, KBO_TOOLTIP_OBJECT_BYTES)) {
        return;
    }

    for (size_t i = 0u; i + sizeof(uintptr_t) <= KBO_TOOLTIP_OBJECT_BYTES
            && seen_count < KBO_TOOLTIP_POINTER_SCAN_LIMIT; i += sizeof(uintptr_t)) {
        uintptr_t ptr = *(uintptr_t*)(base + i);
        if (ptr < 0x10000u || kbo_tooltip_seen_ptr(seen, seen_count, ptr)) {
            continue;
        }

        uint8_t* child = (uint8_t*)ptr;
        if (!memory_range_readable(child, KBO_TOOLTIP_POINTER_SCAN_BYTES)) {
            continue;
        }
        seen[seen_count++] = ptr;

        int has_rating_label =
            kbo_tooltip_child_contains_any_rating_label(child, KBO_TOOLTIP_POINTER_SCAN_BYTES);
        int has_rating_visual =
            kbo_tooltip_child_contains_ascii(child, KBO_TOOLTIP_POINTER_SCAN_BYTES, "RATING_BAR")
            || kbo_tooltip_child_contains_ascii(child, KBO_TOOLTIP_POINTER_SCAN_BYTES, "rating_panel/images")
            || kbo_tooltip_child_contains_ascii(child, KBO_TOOLTIP_POINTER_SCAN_BYTES, "GRID_TITLE_SMALL_RIGHT");
        if (!has_rating_label && !has_rating_visual) {
            continue;
        }

        kbo_tooltip_appendf(
            out,
            out_size,
            pos,
            "child[0x%04llx] ptr=%p rating_block labels=%d visuals=%d\n",
            (unsigned long long)i,
            (void*)ptr,
            has_rating_label,
            has_rating_visual);

        for (size_t j = 0u; j + 4u < KBO_TOOLTIP_POINTER_SCAN_BYTES; ++j) {
            if (memcmp(child + j, "OVR", 3u) == 0
                    || memcmp(child + j, "POT", 3u) == 0
                    || memcmp(child + j, "STU", 3u) == 0
                    || memcmp(child + j, "MOV", 3u) == 0
                    || memcmp(child + j, "CON", 3u) == 0
                    || memcmp(child + j, "STA", 3u) == 0) {
                char label[4] = {(char)child[j], (char)child[j + 1u], (char)child[j + 2u], '\0'};
                kbo_tooltip_appendf(out, out_size, pos, "  label %s at +0x%04Ix\n", label, j);
                size_t start = j >= 0x30u ? j - 0x30u : 0u;
                size_t end = j + 0x50u < KBO_TOOLTIP_POINTER_SCAN_BYTES ? j + 0x50u : KBO_TOOLTIP_POINTER_SCAN_BYTES;
                for (size_t k = start; k + sizeof(uint16_t) <= end; k += sizeof(uint16_t)) {
                    uint16_t value = *(uint16_t*)(child + k);
                    if (value >= 1u && value <= 250u) {
                        kbo_tooltip_appendf(out, out_size, pos, "    near u16 +0x%04Ix = %u\n", k, (unsigned int)value);
                    }
                }
            }
        }

        for (size_t j = 0u; j + sizeof(uint16_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uint16_t)) {
            uint16_t value = *(uint16_t*)(child + j);
            if (value >= 1u && value <= 250u) {
                kbo_tooltip_appendf(out, out_size, pos, "  u16 +0x%04Ix = %u\n", j, (unsigned int)value);
            }
        }

        for (size_t j = 0u; j + sizeof(uint32_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uint32_t)) {
            uint32_t value = *(uint32_t*)(child + j);
            if (value >= 1u && value <= 250u) {
                kbo_tooltip_appendf(out, out_size, pos, "  u32 +0x%04Ix = %u\n", j, (unsigned int)value);
            }
        }

        for (size_t j = 0u; j + sizeof(uintptr_t) <= KBO_TOOLTIP_POINTER_SCAN_BYTES; j += sizeof(uintptr_t)) {
            uintptr_t nested_ptr = *(uintptr_t*)(child + j);
            if (nested_ptr < 0x10000u || !memory_range_readable((uint8_t*)nested_ptr, 0x80u)) {
                continue;
            }
            uint8_t* nested = (uint8_t*)nested_ptr;
            if (kbo_tooltip_child_contains_ascii(nested, 0x80u, "rating_panel/images")
                    || kbo_tooltip_child_contains_ascii(nested, 0x80u, "RATING_BAR")
                    || kbo_tooltip_child_contains_ascii(nested, 0x80u, "background")) {
                kbo_tooltip_appendf(out, out_size, pos, "  nested +0x%04Ix ptr=%p\n", j, (void*)nested_ptr);
                for (size_t k = 0u; k + sizeof(uint16_t) <= 0x80u; k += sizeof(uint16_t)) {
                    uint16_t value = *(uint16_t*)(nested + k);
                    if (value >= 1u && value <= 250u) {
                        kbo_tooltip_appendf(
                            out,
                            out_size,
                            pos,
                            "    nested u16 +0x%04Ix = %u\n",
                            k,
                            (unsigned int)value);
                    }
                }
            }
        }
    }
}

static void kbo_write_tooltip_capture_debug_file(uint32_t player_id, const char* payload)
{
    if (payload == NULL) {
        return;
    }

    char path[MAX_PATH] = {0};
    const char* local_appdata = getenv("LOCALAPPDATA");
    if (local_appdata == NULL || local_appdata[0] == '\0') {
        return;
    }

    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\player_tooltip_capture_last.txt", local_appdata);
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        return;
    }

    fprintf(fp, "player_id: %u\r\n\r\n", player_id);
    fputs(payload, fp);
    fclose(fp);
}

int kbo_capture_ootp_player_tooltip_payload(uint32_t player_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    if (InterlockedCompareExchange(&g_kbo_player_tooltip_capture_lock, 1, 0) != 0) {
        kbo_log_runtimef("KBO player tooltip capture skipped reason=capture_busy player=%u", player_id);
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        InterlockedExchange(&g_kbo_player_tooltip_capture_lock, 0);
        return 0;
    }

    OotpOperatorNewFn ootp_new =
        (OotpOperatorNewFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_UI_OPERATOR_NEW_RVA);
    OotpPlayerTooltipFactoryFn factory =
        (OotpPlayerTooltipFactoryFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_FACTORY_RVA);
    OotpPlayerTooltipRenderFn render =
        (OotpPlayerTooltipRenderFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_RENDER_RVA);
    void** current_tooltip =
        (void**)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_CURRENT_GLOBAL_RVA);
    if (ootp_new == NULL || factory == NULL || render == NULL || current_tooltip == NULL) {
        kbo_log_runtimef(
            "KBO player tooltip capture skipped reason=resolve_failed player=%u new=%p factory=%p render=%p current=%p",
            player_id,
            (void*)ootp_new,
            (void*)factory,
            (void*)render,
            (void*)current_tooltip);
        InterlockedExchange(&g_kbo_player_tooltip_capture_lock, 0);
        return 0;
    }

    void* raw = ootp_new(KBO_TOOLTIP_OBJECT_BYTES);
    if (raw == NULL) {
        InterlockedExchange(&g_kbo_player_tooltip_capture_lock, 0);
        return 0;
    }

    kbo_tooltip_text_capture_reset();
    LONG64 format_calls_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_call_count, 0, 0);
    LONG64 format_active_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_active_call_count, 0, 0);
    LONG64 format_numeric_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_numeric_call_count, 0, 0);
    LONG64 rating_common_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_call_count, 0, 0);
    LONG64 rating_common_active_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_active_call_count, 0, 0);
    LONG64 rating_panel_ctor_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_call_count, 0, 0);
    LONG64 rating_panel_ctor_active_before =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_active_call_count, 0, 0);
    InterlockedExchange(&g_kbo_player_tooltip_text_capture_active, 1);
    void* tooltip = factory(raw, player_id, 0u, 0u, NULL, 0u);
    if (tooltip == NULL) {
        InterlockedExchange(&g_kbo_player_tooltip_text_capture_active, 0);
        kbo_log_runtimef("KBO player tooltip capture skipped reason=factory_failed player=%u raw=%p", player_id, raw);
        InterlockedExchange(&g_kbo_player_tooltip_capture_lock, 0);
        return 0;
    }

    *current_tooltip = tooltip;
    render(tooltip, 8u);
    InterlockedExchange(&g_kbo_player_tooltip_text_capture_active, 0);
    LONG64 format_calls_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_call_count, 0, 0);
    LONG64 format_active_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_active_call_count, 0, 0);
    LONG64 format_numeric_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_string_format_numeric_call_count, 0, 0);
    LONG64 rating_common_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_call_count, 0, 0);
    LONG64 rating_common_active_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_common_active_call_count, 0, 0);
    LONG64 rating_panel_ctor_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_call_count, 0, 0);
    LONG64 rating_panel_ctor_active_after =
        InterlockedCompareExchange64(&g_kbo_player_tooltip_rating_panel_ctor_active_call_count, 0, 0);

    int builder_overall = 0;
    int builder_potential = 0;
    int memory_overall = 0;
    int memory_potential = 0;
    int scan_overall = 0;
    int scan_potential = 0;
    int has_builder_overall_potential =
        kbo_tooltip_extract_overall_potential_from_builder_capture(&builder_overall, &builder_potential);
    int has_format_overall_potential =
        kbo_tooltip_extract_overall_potential_from_format_capture(&memory_overall, &memory_potential);
    int has_scan_overall_potential =
        kbo_tooltip_extract_overall_potential(tooltip, &scan_overall, &scan_potential);
    int display_overall = has_builder_overall_potential ? builder_overall : memory_overall;
    int display_potential = has_builder_overall_potential ? builder_potential : memory_potential;
    int has_display_overall_potential = has_builder_overall_potential || has_format_overall_potential;
    const char* display_rating_source =
        has_builder_overall_potential ? "builder" : (has_format_overall_potential ? "format" : "none");

    size_t pos = 0u;
    kbo_tooltip_appendf(out, out_size, &pos, "OOTP tooltip capture\n");
    kbo_tooltip_appendf(out, out_size, &pos, "player_id: %u\n", player_id);
    kbo_tooltip_appendf(out, out_size, &pos, "tooltip_ptr: %p\n", tooltip);
    kbo_tooltip_appendf(out, out_size, &pos, "\nrender text append capture\n%s", g_kbo_player_tooltip_text_capture);
    if (has_display_overall_potential) {
        kbo_tooltip_appendf(
            out,
            out_size,
            &pos,
            "\nmemory display ratings\nsource %s\nOVR %d\nPOT %d\n",
            display_rating_source,
            display_overall,
            display_potential);
    }

    char debug_payload[24000] = {0};
    size_t debug_pos = 0u;
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "%s", out);
    kbo_tooltip_appendf(
        debug_payload,
        sizeof(debug_payload),
        &debug_pos,
        "\nformat probe counters\ncalls_delta=%lld\nactive_delta=%lld\nnumeric_delta=%lld\n",
        (long long)(format_calls_after - format_calls_before),
        (long long)(format_active_after - format_active_before),
        (long long)(format_numeric_after - format_numeric_before));
    kbo_tooltip_appendf(
        debug_payload,
        sizeof(debug_payload),
        &debug_pos,
        "\nrating builder probe counters\ncommon_calls_delta=%lld\ncommon_active_delta=%lld\npanel_ctor_calls_delta=%lld\npanel_ctor_active_delta=%lld\n",
        (long long)(rating_common_after - rating_common_before),
        (long long)(rating_common_active_after - rating_common_active_before),
        (long long)(rating_panel_ctor_after - rating_panel_ctor_before),
        (long long)(rating_panel_ctor_active_after - rating_panel_ctor_active_before));
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating builder capture\n%s", g_kbo_player_tooltip_builder_capture);
    if (has_scan_overall_potential) {
        kbo_tooltip_appendf(
            debug_payload,
            sizeof(debug_payload),
            &debug_pos,
            "\nmemory scan candidate ratings\nOVR %d\nPOT %d\n",
            scan_overall,
            scan_potential);
    }
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\ninline strings\n");
    kbo_tooltip_scan_inline_strings("obj", (uint8_t*)tooltip, KBO_TOOLTIP_OBJECT_BYTES, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\npointer strings\n");
    kbo_tooltip_scan_pointer_strings(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating formats\n%s", g_kbo_player_tooltip_rating_capture);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating panels\n");
    kbo_tooltip_scan_rating_panels(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nrating child values\n");
    kbo_tooltip_scan_rating_child_values(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);
    kbo_tooltip_appendf(debug_payload, sizeof(debug_payload), &debug_pos, "\nnested pointer strings\n");
    kbo_tooltip_scan_nested_pointer_strings(tooltip, debug_payload, sizeof(debug_payload), &debug_pos);

    kbo_write_tooltip_capture_debug_file(player_id, debug_payload);
    kbo_log_runtimef(
        "KBO player tooltip captured player=%u tooltip=%p ui_bytes=%u debug_bytes=%u rating_source=%s ovr=%d pot=%d format_calls=%lld active=%lld numeric=%lld rating_common=%lld rating_common_active=%lld panel_ctor=%lld panel_ctor_active=%lld scan_ovr=%d scan_pot=%d",
        player_id,
        tooltip,
        (unsigned int)pos,
        (unsigned int)debug_pos,
        display_rating_source,
        has_display_overall_potential ? display_overall : 0,
        has_display_overall_potential ? display_potential : 0,
        (long long)(format_calls_after - format_calls_before),
        (long long)(format_active_after - format_active_before),
        (long long)(format_numeric_after - format_numeric_before),
        (long long)(rating_common_after - rating_common_before),
        (long long)(rating_common_active_after - rating_common_active_before),
        (long long)(rating_panel_ctor_after - rating_panel_ctor_before),
        (long long)(rating_panel_ctor_active_after - rating_panel_ctor_active_before),
        has_scan_overall_potential ? scan_overall : 0,
        has_scan_overall_potential ? scan_potential : 0);
    InterlockedExchange(&g_kbo_player_tooltip_capture_lock, 0);
    return pos > 0u;
}

static int kbo_player_hover_active_now(void)
{
    if (InterlockedCompareExchange(&g_kbo_player_hover_active, 0, 0) == 0) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    LONG64 until = InterlockedCompareExchange64(&g_kbo_player_hover_active_until_ms, 0, 0);
    if ((LONG64)now <= until) {
        return 1;
    }

    InterlockedExchange(&g_kbo_player_hover_active, 0);
    return 0;
}

void kbo_clear_ootp_player_hover_popup(uint32_t player_id)
{
    LONG active_player = InterlockedCompareExchange(&g_kbo_player_hover_active_player, 0, 0);
    if (player_id == 0u || active_player == 0 || active_player == (LONG)player_id) {
        InterlockedExchange(&g_kbo_player_hover_active, 0);
        InterlockedExchange(&g_kbo_player_hover_active_player, 0);
        InterlockedExchange64(&g_kbo_player_hover_active_until_ms, 0);
    }
}

int kbo_show_ootp_player_hover_popup(uint32_t player_id, int screen_x, int screen_y)
{
    uintptr_t manager = kbo_player_hover_manager_ptr();
    if (manager == 0u || !memory_range_readable((uint8_t*)manager, 0x98u)) {
        kbo_log_runtimef(
            "KBO player hover popup skipped reason=manager_unavailable player=%u manager=%p",
            player_id,
            (void*)manager);
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    OotpOperatorNewFn ootp_new =
        (OotpOperatorNewFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_UI_OPERATOR_NEW_RVA);
    OotpPlayerTooltipFactoryFn factory =
        (OotpPlayerTooltipFactoryFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_FACTORY_RVA);
    OotpPlayerTooltipRenderFn render =
        (OotpPlayerTooltipRenderFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_RENDER_RVA);
    void** current_tooltip =
        (void**)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_CURRENT_GLOBAL_RVA);
    if (ootp_new == NULL || factory == NULL || render == NULL || current_tooltip == NULL) {
        kbo_log_runtimef(
            "KBO player hover popup skipped reason=resolve_failed player=%u new=%p factory=%p render=%p current=%p",
            player_id,
            (void*)ootp_new,
            (void*)factory,
            (void*)render,
            (void*)current_tooltip);
        return 0;
    }

    void* vtable = *(void**)manager;
    if (vtable == NULL || !memory_range_readable((uint8_t*)vtable, 0x18u)) {
        kbo_log_runtimef(
            "KBO player hover popup skipped reason=manager_vtable_unreadable player=%u manager=%p vtable=%p",
            player_id,
            (void*)manager,
            vtable);
        return 0;
    }

    OotpPlayerTooltipAttachFn attach = *(OotpPlayerTooltipAttachFn*)((uint8_t*)vtable + 0x08u);
    if (attach == NULL) {
        kbo_log_runtimef("KBO player hover popup skipped reason=attach_unavailable player=%u", player_id);
        return 0;
    }

    void* raw = ootp_new(0xD20u);
    if (raw == NULL) {
        kbo_log_runtimef("KBO player hover popup skipped reason=allocation_failed player=%u", player_id);
        return 0;
    }

    void* tooltip = factory(
        raw,
        player_id,
        (uint32_t)screen_y,
        (uint32_t)screen_x,
        NULL,
        0u);
    if (tooltip == NULL) {
        kbo_log_runtimef("KBO player hover popup skipped reason=factory_failed player=%u raw=%p", player_id, raw);
        return 0;
    }

    *current_tooltip = tooltip;
    render(tooltip, 8u);
    attach(manager, tooltip, 1u);
    InterlockedExchange(&g_kbo_player_hover_active_player, (LONG)player_id);
    InterlockedExchange64(&g_kbo_player_hover_active_until_ms, (LONG64)(GetTickCount64() + 1800u));
    InterlockedExchange(&g_kbo_player_hover_active, 1);

    kbo_log_runtimef(
        "KBO player hover popup shown player=%u tooltip=%p manager=%p x=%d y=%d",
        player_id,
        tooltip,
        (void*)manager,
        screen_x,
        screen_y);
    return 1;
}

__declspec(noinline) int ootp_kbo_player_hover_manager_probe_wrapper(
    uintptr_t manager_ptr,
    uintptr_t original_func_ptr)
{
    if (kbo_player_hover_active_now()) {
        return 5;
    }

    if (manager_ptr != 0u) {
        PVOID previous = InterlockedExchangePointer(
            &g_kbo_player_hover_manager_ptr,
            (PVOID)manager_ptr);
        if ((uintptr_t)previous != manager_ptr) {
            LONG count = InterlockedIncrement(&g_kbo_player_hover_manager_log_count);
            if (count <= 8) {
                kbo_log_runtimef("KBO player hover manager captured manager=%p", (void*)manager_ptr);
            }
        }
    }

    OotpPlayerHoverManagerFn original = (OotpPlayerHoverManagerFn)original_func_ptr;
    if (original == NULL) {
        return 0;
    }
    return original(manager_ptr);
}
