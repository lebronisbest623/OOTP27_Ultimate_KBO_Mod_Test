#ifndef KBO_HOTKEY_WINDOW_STATE_LEAGUE_LOOKUP_INTERNAL_H
#define KBO_HOTKEY_WINDOW_STATE_LEAGUE_LOOKUP_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../core/dates/core_text_date.h"
#include "../../../../custom_events/asian_games/player_eval/asian_games_player_eval.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/names/team_string.h"
#include "../api/state_league_lookup.h"
#include "../../team_vector/state_team_vector.h"
#include "../../../support/text/language/ui_language.h"


#define KBO_LEAGUE_PTR_MISS_CACHE_MAX 32
#define KBO_NAMED_LEAGUE_SCAN_MIN_SCORE 90
#define KBO_NAMED_LEAGUE_SCAN_EARLY_SCORE 115
#define KBO_NAMED_LEAGUE_SCAN_OBJECT_SPAN (OOTP27_KBO_LEAGUE_ID_OFFSET + 16u)
#define KBO_LEAGUE_DISPLAY_CACHE_MAX 64
typedef struct KboHubLeagueDisplayCacheEntry {
    uint32_t league_id;
    uintptr_t league_ptr;
    uint32_t year;
    int score;
    char name[96];
    char logo_file[128];
} KboHubLeagueDisplayCacheEntry;

extern uint32_t  g_kbo_league_ptr_cache_id;
extern uintptr_t g_kbo_league_ptr_cache_ptr;
extern uint32_t  g_kbo_league_ptr_miss_cache_ids[KBO_LEAGUE_PTR_MISS_CACHE_MAX];
extern ULONGLONG g_kbo_league_ptr_miss_cache_until_ms[KBO_LEAGUE_PTR_MISS_CACHE_MAX];
extern uintptr_t g_kbo_league_display_cache_global;
extern uintptr_t g_kbo_league_display_cache_prewarmed_global;
extern KboHubLeagueDisplayCacheEntry g_kbo_league_display_cache[KBO_LEAGUE_DISPLAY_CACHE_MAX];

void kbo_hub_clear_league_display_cache(void);
void kbo_hub_refresh_league_cache_context(void);
int kbo_hub_find_league_display_cache_slot(uint32_t league_id);
int kbo_hub_find_league_display_cache_insert_slot(uint32_t league_id);
void kbo_hub_store_league_display_cache(
    uint32_t league_id,
    uintptr_t league_ptr,
    int score,
    const char* name,
    const char* logo_file);
int kbo_hub_try_copy_cached_league_name(uint32_t league_id, char* out, size_t out_size);
int kbo_hub_try_copy_cached_league_logo_file(uint32_t league_id, char* out, size_t out_size);
int kbo_hub_try_get_cached_league_ptr(uint32_t league_id, uintptr_t* out_league_ptr);
int kbo_league_ptr_recent_miss(uint32_t league_id, ULONGLONG now_ms);
void kbo_remember_league_ptr_miss(uint32_t league_id, ULONGLONG now_ms);
void kbo_forget_league_ptr_miss(uint32_t league_id);
int kbo_hub_text_ends_with_ignore_case(const char* text, const char* suffix);
int kbo_hub_league_name_likeness_score(const char* name);
int kbo_hub_abbr_likeness_score(const char* abbr);
int kbo_hub_named_league_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    char* out_name,
    size_t out_name_size,
    char* out_logo_file,
    size_t out_logo_file_size);
uintptr_t kbo_scan_named_league_ptr(uint32_t league_id, SIZE_T max_region_size, int* out_score, char* out_name, size_t out_name_size);
int kbo_hub_league_id_list_index(const uint32_t* league_ids, int league_count, uint32_t league_id);
int kbo_hub_collect_visible_league_ids(uint32_t* league_ids, int max_leagues);
int kbo_hub_count_cached_league_ids(const uint32_t* league_ids, int league_count);
int kbo_scan_named_league_ptrs_for_ids(const uint32_t* league_ids, int league_count, SIZE_T max_region_size);
void kbo_hub_prewarm_league_display_cache(void);
uintptr_t kbo_find_league_ptr(uint32_t league_id);
void kbo_hub_read_league_name(uintptr_t league_ptr, char* out, size_t out_size);
void kbo_hub_copy_league_display_name_fast(uint32_t league_id, char* out, size_t out_size);
void kbo_hub_copy_league_display_name(uint32_t league_id, char* out, size_t out_size);

#endif
