#ifndef KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_STATE_H_
#define KBOFIX_SRC_FA_SALARY_SNAPSHOT_FA_SALARY_SNAPSHOT_PARTS_SALARY_SNAPSHOT_STATE_H_

#include <stdint.h>
#include <windows.h>

#include "../../bootstrap/abi/ootp_offsets.h"

#define KBO_FA_SALARY_SNAPSHOT_GRADE_MAX 4096
#define KBO_FA_SALARY_PHASE_EVENT_RING_SIZE 32u
#define KBO_FA_SALARY_PHASE_EVENT_RING_MASK (KBO_FA_SALARY_PHASE_EVENT_RING_SIZE - 1u)

typedef struct KboFaSalarySnapshotRow {
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t ranking_team_id;
    uint32_t current_league_id;
    uint32_t draft_league_id;
    uint16_t age;
    uint8_t retired_flag;
    uint8_t contract_level;
    uint8_t foreign_flag;
    int32_t salary;
    int32_t contract_status;
    int32_t contract_start_year;
    int32_t contract_years[OOTP27_PLAYER_CONTRACT_SALARY_YEARS];
    uint32_t overall_rank;
    uint32_t overall_ordinal;
    uint32_t team_rank;
    uint32_t team_ordinal;
    char player_name[96];
    char player_key[64];
} KboFaSalarySnapshotRow;

typedef struct KboFaSalarySnapshotGrade {
    uint32_t player_id;
    uint32_t snapshot_date;
    uint32_t season;
    uint32_t opening_day;
    uint32_t ranking_team_id;
    uint8_t foreign_flag;
    int32_t salary;
    uint32_t overall_rank;
    uint32_t overall_ordinal;
    uint32_t team_rank;
    uint32_t team_ordinal;
    char grade[12];
    char player_key[64];
    char player_name[96];
} KboFaSalarySnapshotGrade;

extern volatile LONG g_kbo_fa_salary_snapshot_thread_started;
extern volatile LONG g_kbo_fa_salary_snapshot_phase_event_thread_started;
extern volatile LONG g_kbo_fa_salary_snapshot_phase_event_write_cursor;
extern volatile LONG g_kbo_fa_salary_snapshot_phase_event_published_sequence;
extern volatile LONG g_kbo_fa_salary_snapshot_phase_event_consumed_sequence;
extern uintptr_t g_kbo_fa_salary_snapshot_phase_event_league_ptrs[KBO_FA_SALARY_PHASE_EVENT_RING_SIZE];
extern uint32_t g_kbo_fa_salary_snapshot_phase_event_values[KBO_FA_SALARY_PHASE_EVENT_RING_SIZE];
extern uint32_t g_kbo_fa_salary_snapshot_phase_event_site_rvas[KBO_FA_SALARY_PHASE_EVENT_RING_SIZE];
extern volatile LONG g_kbo_fa_salary_snapshot_phase_pending_active;
extern uintptr_t g_kbo_fa_salary_snapshot_phase_pending_league_ptr;
extern volatile LONG g_kbo_fa_salary_snapshot_phase_pending_value;
extern volatile LONG g_kbo_fa_salary_snapshot_phase_pending_site_rva;
extern volatile LONG g_kbo_fa_salary_snapshot_cached_exists_season;
extern volatile LONG g_kbo_fa_salary_snapshot_cached_exists_value;

#endif
