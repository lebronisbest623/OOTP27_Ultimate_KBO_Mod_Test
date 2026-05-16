#ifndef KBO_PLAYER_HOVER_MANAGER_PROBE_INTERNAL_H
#define KBO_PLAYER_HOVER_MANAGER_PROBE_INTERNAL_H

#include "../player_hover_manager_probe.h"
#include <windows.h>
#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../bootstrap/abi/ootp_typedefs.h"
#include "../../../../build_verify/build_verify.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/sync/lock.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

extern PVOID volatile g_kbo_player_hover_manager_ptr;
extern volatile LONG g_kbo_player_hover_manager_log_count;
extern volatile LONG g_kbo_player_hover_active;
extern volatile LONG g_kbo_player_hover_active_player;
extern volatile LONG64 g_kbo_player_hover_active_until_ms;
extern uintptr_t g_kbo_player_tooltip_text_append_original;
extern uintptr_t g_kbo_player_tooltip_string_format_original;
extern uintptr_t g_kbo_player_tooltip_rating_common_original;
extern uintptr_t g_kbo_player_tooltip_rating_panel_ctor_original;
extern KboLock g_kbo_player_tooltip_capture_lock;
extern volatile LONG g_kbo_player_tooltip_text_capture_active;
extern char g_kbo_player_tooltip_text_capture[12000];
extern volatile LONG g_kbo_player_tooltip_text_capture_pos;
extern char g_kbo_player_tooltip_rating_capture[8192];
extern volatile LONG g_kbo_player_tooltip_rating_capture_pos;
extern char g_kbo_player_tooltip_builder_capture[8192];
extern volatile LONG g_kbo_player_tooltip_builder_capture_pos;
extern volatile LONG64 g_kbo_player_tooltip_string_format_call_count;
extern volatile LONG64 g_kbo_player_tooltip_string_format_active_call_count;
extern volatile LONG64 g_kbo_player_tooltip_string_format_numeric_call_count;
extern volatile LONG64 g_kbo_player_tooltip_rating_common_call_count;
extern volatile LONG64 g_kbo_player_tooltip_rating_common_active_call_count;
extern volatile LONG64 g_kbo_player_tooltip_rating_panel_ctor_call_count;
extern volatile LONG64 g_kbo_player_tooltip_rating_panel_ctor_active_call_count;

void kbo_tooltip_text_capture_reset(void);
int kbo_tooltip_extract_overall_potential_from_builder_capture(int* out_overall, int* out_potential);
int kbo_tooltip_extract_overall_potential_from_format_capture(int* out_overall, int* out_potential);
void kbo_tooltip_appendf(char* out, size_t out_size, size_t* pos, const char* fmt, ...);
void kbo_tooltip_scan_inline_strings(const char* label, uint8_t* base, size_t bytes, char* out, size_t out_size, size_t* pos);
void kbo_tooltip_scan_pointer_strings(void* tooltip, char* out, size_t out_size, size_t* pos);
void kbo_tooltip_scan_nested_pointer_strings(void* tooltip, char* out, size_t out_size, size_t* pos);
int kbo_tooltip_seen_ptr(uintptr_t* seen, size_t seen_count, uintptr_t ptr);
void kbo_tooltip_scan_rating_panels(void* tooltip, char* out, size_t out_size, size_t* pos);
int kbo_tooltip_extract_overall_potential(void* tooltip, int* out_overall, int* out_potential);
void kbo_tooltip_scan_rating_child_values(void* tooltip, char* out, size_t out_size, size_t* pos);

#endif
