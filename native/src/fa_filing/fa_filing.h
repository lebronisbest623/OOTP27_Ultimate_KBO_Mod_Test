#ifndef KBOFIX_SRC_FA_FILING_FA_FILING_H_
#define KBOFIX_SRC_FA_FILING_FA_FILING_H_

#include <stdint.h>
#include <stddef.h>

#define KBO_FA_FILING_MAX 4096

typedef struct KboFaFilingRecord {
    uint32_t player_id;
    uint32_t filing_date;
    uint32_t season;
    uint32_t original_team_id;
    uint32_t league_id;
    uint32_t source_caller_rva;
    uint8_t notify;
    uint8_t contract_level;
    char player_name[96];
    char source[48];
} KboFaFilingRecord;

int kbo_fa_filing_find_latest_player(
    uint32_t player_id,
    uint32_t* out_original_team_id,
    uint32_t* out_league_id,
    uint32_t* out_season);

int kbo_load_fa_filing_records(
    KboFaFilingRecord* rows,
    int max_rows,
    char* out_path,
    size_t out_path_size);
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
