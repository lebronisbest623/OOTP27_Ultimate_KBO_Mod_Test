#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EXCEPTIONS_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_EXCEPTIONS_H_

#include <stddef.h>
#include <stdint.h>

#define KBO_CBT_EXCEPTION_MAX 64
#define KBO_CBT_EXCEPTION_SEASON_SEED_MAX 8192

typedef struct KboCbtExceptionDesignation {
    uint32_t season;
    uint32_t team_id;
    char player_key[64];
    char player_name[96];
} KboCbtExceptionDesignation;

int kbo_cbt_exception_load_designations(KboCbtExceptionDesignation* rows, int max);
int kbo_cbt_exception_save_designation(uint32_t season, uint32_t team_id, const char* player_key, const char* player_name);
int kbo_cbt_exception_clear_designation(uint32_t season, uint32_t team_id);
int kbo_cbt_exception_find_designation(
    const KboCbtExceptionDesignation* rows,
    int count,
    uint32_t season,
    uint32_t team_id,
    const char* player_key);
int kbo_cbt_exception_player_eligible(uint32_t team_id, const char* player_key, int* out_season_count);
int kbo_cbt_exception_resolve_opening_day(uint32_t season, uint32_t* out_opening_day);
int kbo_cbt_exception_designation_window_open(uint32_t season, uint32_t current_date);
int kbo_cbt_exception_auto_designate_missing(uint32_t season, const char* source);

#endif
