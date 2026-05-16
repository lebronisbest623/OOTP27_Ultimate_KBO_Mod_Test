#ifndef NATIVE_SRC_FA_FILING_FA_FILING_C_INTERNAL_H
#define NATIVE_SRC_FA_FILING_FA_FILING_C_INTERNAL_H

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../bootstrap/abi/ootp_offsets.h"
#include "../bootstrap/profiling/profiler.h"
#include "../core/logging/core_log.h"
#include "../core/files/save_paths/core_save_paths.h"
#include "../core/sync/lock.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/lookup/team_lookup.h"
#include "../team/names/team_name_cache.h"
#include "fa_filing.h"
#include "fa_filing_parts/fa_filing_csv_parse.h"
#include "fa_filing_parts/fa_filing_csv_write_helpers.h"
#define KBO_FA_FILING_NEGATIVE_CACHE_MAX 512

extern KboLock g_kbo_fa_filing_lock;
extern volatile LONG g_kbo_fa_filing_cache_dirty;
extern KboFaFilingRecord* g_kbo_fa_filing_cache_rows;
extern int g_kbo_fa_filing_cache_count;
extern char g_kbo_fa_filing_cache_path[MAX_PATH];
extern uint32_t g_kbo_fa_filing_negative_cache[KBO_FA_FILING_NEGATIVE_CACHE_MAX];
extern volatile LONG g_kbo_fa_filing_negative_cache_cursor;

int kbo_fa_filing_negative_cache_contains(uint32_t player_id);
void kbo_fa_filing_negative_cache_add(uint32_t player_id);
void kbo_fa_filing_negative_cache_clear(void);
int kbo_get_fa_filing_csv_path(char* out, size_t out_size);
void kbo_fa_filing_enter_lock(void);
void kbo_fa_filing_leave_lock(void);
int kbo_load_fa_filing_records_unlocked(
    KboFaFilingRecord* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size);
int kbo_write_fa_filing_records_unlocked(
    const KboFaFilingRecord* rows,
    int row_count,
    char* out_path,
    size_t out_path_size);
int kbo_load_fa_filing_records(
    KboFaFilingRecord* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size);
int kbo_fa_filing_find_latest_player(
    uint32_t player_id,
    uint32_t* out_original_team_id,
    uint32_t* out_league_id,
    uint32_t* out_season);
int kbo_fa_filing_is_official_transition_caller(uintptr_t caller_rva);
uint32_t kbo_fa_filing_team_league_id(uint32_t team_id);
int kbo_record_fa_filing_transition(
    uintptr_t player_ptr,
    uint32_t player_id,
    uint32_t filing_date,
    uint32_t original_team_id,
    uint32_t league_id,
    uint32_t caller_rva,
    uint8_t notify,
    uint8_t contract_level,
    const char* source);

#endif
