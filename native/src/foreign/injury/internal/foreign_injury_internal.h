#ifndef NATIVE_SRC_FOREIGN_INJURY_FOREIGN_INJURY_C_INTERNAL_H
#define NATIVE_SRC_FOREIGN_INJURY_FOREIGN_INJURY_C_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"
#include "../../common/csv/foreign_csv_parse.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../../replacement_seed/api/foreign_replacement_seed.h"
#include "../paths/foreign_injury_paths.h"
#ifndef KBO_FOREIGN_INJURY_SLOT_REGULAR
#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#endif
#ifndef KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2
#endif
#define KBO_FOREIGN_INJURY_REPLACEMENT_MIN_DAYS 42
#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4
typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
} KboForeignInjuryReplacement;

extern KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX];
extern int g_kbo_foreign_injury_replacement_count;
extern LONG g_kbo_foreign_injury_replacement_lock;
extern char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH];
extern LONG g_kbo_foreign_injury_replacement_thread_started;

int kbo_foreign_injury_replacement_enabled(void);
int kbo_foreign_injury_status_uses_slot(uint8_t status);
uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player);
const char* kbo_foreign_injury_slot_label(uint8_t slot_type);
const char* kbo_foreign_injury_status_label(uint8_t status);
void kbo_lock_foreign_injury_replacements(void);
void kbo_unlock_foreign_injury_replacements(void);
int kbo_load_foreign_injury_replacements_locked(const char* path);
int kbo_parse_foreign_injury_replacement_seed_line(
    const char* line,
    uint32_t today,
    KboForeignInjuryReplacement* out);
int kbo_import_foreign_injury_replacement_seed_file_locked(
    const char* path,
    uint32_t today,
    const char* source);
int kbo_persist_foreign_injury_replacements_locked(void);
void kbo_ensure_foreign_injury_replacements_loaded(void);
int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed);
int kbo_team_has_foreign_injury_slot_locked(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id);
int kbo_team_has_foreign_injury_slot(uint32_t team_id, uint8_t slot_type, uint32_t* out_injured_player_id);
int kbo_team_has_foreign_injury_slot_for_candidate_locked(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed);
void kbo_emit_foreign_injury_replacement_news(
    const KboForeignInjuryReplacement* rec,
    int days_left,
    const char* phase);
void kbo_foreign_injury_replacement_scan_once(const char* source);
DWORD WINAPI kbo_foreign_injury_replacement_thread(LPVOID parameter);
void start_kbo_foreign_injury_replacement_thread(void);

#endif
