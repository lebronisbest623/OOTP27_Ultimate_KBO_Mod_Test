#include "player_hover_manager_probe_internal.h"
#include "../../../../bootstrap/profiling/profiler.h"

void kbo_tooltip_text_capture_reset(void)
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
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    kbo_tooltip_text_capture_append_line(text);

    OotpPlayerTooltipTextAppendFn original =
        (OotpPlayerTooltipTextAppendFn)g_kbo_player_tooltip_text_append_original;
    if (original == NULL) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_text_append", out);
    }
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    void* result = original(text, mode, out, flags, arg5, arg6, arg7, arg8, arg9, arg10);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_text_append", result);
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
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    InterlockedIncrement64(&g_kbo_player_tooltip_string_format_call_count);
    OotpPlayerTooltipStringFormatFn original =
        (OotpPlayerTooltipStringFormatFn)g_kbo_player_tooltip_string_format_original;
    if (original == NULL) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_string_format", text_object);
    }
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    void* result = original(text_object, format, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    if (InterlockedCompareExchange(&g_kbo_player_tooltip_text_capture_active, 0, 0) != 0) {
        InterlockedIncrement64(&g_kbo_player_tooltip_string_format_active_call_count);
        kbo_tooltip_rating_capture_append(format, result, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
    }
    KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_string_format", result);
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
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
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
        KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_rating_common", 0u);
    }
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    uint8_t result = original(container, column, row, displayed_rating, is_potential, mirror_flag);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_rating_common", result);
}

__declspec(noinline) void* ootp_kbo_player_tooltip_rating_panel_ctor_probe_wrapper(
    void* panel_object,
    uint16_t current_rating,
    uint16_t potential_rating,
    uint8_t potential_only)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
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
        KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_rating_panel_ctor", panel_object);
    }
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    void* result = original(panel_object, current_rating, potential_rating, potential_only);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.tooltip_rating_panel_ctor", result);
}
