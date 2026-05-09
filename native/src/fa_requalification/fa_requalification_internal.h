#ifndef NATIVE_SRC_FA_REQUALIFICATION_FA_REQUALIFICATION_C_INTERNAL_H
#define NATIVE_SRC_FA_REQUALIFICATION_FA_REQUALIFICATION_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/files/atomic/core_atomic_file.h"
#include "../core/core_flags/api/flags_api.h"
#include "../core/core_league_context_parts/api/league_context_lookup.h"
#include "../core/logging/core_log.h"
#include "../core/files/save_paths/core_save_paths.h"
#include "../fa_compensation/history/fa_compensation_history.h"
#include "../foreign/common/dates/foreign_waiver_date.h"
#include "../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../foreign/common/policy/foreign_waiver_policy.h"
#include "../foreign/injury/api/foreign_injury_labels.h"
#include "../military_service/players/team_policy/military_service_team_policy.h"
#include "../runtime_memory/runtime_memory.h"
#include "../team/lookup/team_lookup.h"
#include "../team/assignment/roster_arrays/team_roster_arrays.h"
#include "fa_requalification.h"
#include <stdint.h>
#ifndef KBO_FA_REQUALIFICATION_TYPES_DEFINED
#define KBO_FA_REQUALIFICATION_TYPES_DEFINED
#define KBO_FA_REQUALIFICATION_YEARS 4
#define KBO_FA_REQUALIFICATION_MAX 4096
typedef struct KboFaRequalificationRecord {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t last_fa_year;
    uint32_t fa_count;
} KboFaRequalificationRecord;
#endif

extern LONG g_kbo_fa_requalification_thread_started;
extern LONG g_kbo_fa_requalification_no_date_log_count;
extern LONG g_kbo_fa_requalification_no_records_log_count;
extern LONG g_kbo_fa_requalification_skip_log_count;
extern LONG g_kbo_fa_requalification_hook_skip_log_count;
extern volatile LONG g_kbo_fa_requalification_records_lock;
extern uint32_t g_kbo_fa_requalification_last_no_records_date;

int get_kbo_fa_requalification_path(char* out, size_t out_size);
void kbo_lock_fa_requalification_records(void);
void kbo_unlock_fa_requalification_records(void);
int kbo_fa_parse_u32_csv_field(const char** cursor, uint32_t* out_value);
void kbo_ensure_fa_requalification_template(void);
int kbo_load_fa_requalification_records(KboFaRequalificationRecord* records, int max_records);
int kbo_write_fa_requalification_records(const KboFaRequalificationRecord* records, int count);
int kbo_record_fa_requalification_signing(uint32_t player_id, uint32_t team_id, uint32_t signing_year, const char* source);
int kbo_fa_requalification_team_ptr_is_kbo(
    uintptr_t team_ptr,
    uint32_t* out_team_id,
    uint32_t* out_league_id);
int kbo_restore_fa_requalification_team_control(
    const KboFaRequalificationRecord* rec,
    uint32_t current_year,
    const char* source);
void kbo_run_fa_requalification_once(const char* source);
DWORD WINAPI kbo_fa_requalification_thread(LPVOID parameter);
void start_kbo_fa_requalification_thread(void);

#endif
