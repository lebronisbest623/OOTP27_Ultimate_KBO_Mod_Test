#include "player_hover_manager_probe_internal.h"

PVOID volatile g_kbo_player_hover_manager_ptr = NULL;
volatile LONG g_kbo_player_hover_manager_log_count = 0;
volatile LONG g_kbo_player_hover_active = 0;
volatile LONG g_kbo_player_hover_active_player = 0;
volatile LONG64 g_kbo_player_hover_active_until_ms = 0;
uintptr_t g_kbo_player_tooltip_text_append_original = 0u;
uintptr_t g_kbo_player_tooltip_string_format_original = 0u;
uintptr_t g_kbo_player_tooltip_rating_common_original = 0u;
uintptr_t g_kbo_player_tooltip_rating_panel_ctor_original = 0u;
KboLock g_kbo_player_tooltip_capture_lock = KBO_LOCK_INIT;
volatile LONG g_kbo_player_tooltip_text_capture_active = 0;
char g_kbo_player_tooltip_text_capture[12000] = {0};
volatile LONG g_kbo_player_tooltip_text_capture_pos = 0;
char g_kbo_player_tooltip_rating_capture[8192] = {0};
volatile LONG g_kbo_player_tooltip_rating_capture_pos = 0;
char g_kbo_player_tooltip_builder_capture[8192] = {0};
volatile LONG g_kbo_player_tooltip_builder_capture_pos = 0;
volatile LONG64 g_kbo_player_tooltip_string_format_call_count = 0;
volatile LONG64 g_kbo_player_tooltip_string_format_active_call_count = 0;
volatile LONG64 g_kbo_player_tooltip_string_format_numeric_call_count = 0;
volatile LONG64 g_kbo_player_tooltip_rating_common_call_count = 0;
volatile LONG64 g_kbo_player_tooltip_rating_common_active_call_count = 0;
volatile LONG64 g_kbo_player_tooltip_rating_panel_ctor_call_count = 0;
volatile LONG64 g_kbo_player_tooltip_rating_panel_ctor_active_call_count = 0;
