#ifndef KBOFIX_SRC_FA_REQUALIFICATION_FA_REQUALIFICATION_H_
#define KBOFIX_SRC_FA_REQUALIFICATION_FA_REQUALIFICATION_H_

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

int kbo_load_fa_requalification_records(KboFaRequalificationRecord* records, int max_records);
uint8_t* kbo_find_fa_requalification_player_by_id(uint32_t player_id);
__declspec(noinline) int ootp_kbo_fa_signing_branch_wrapper(
    uintptr_t player_ptr,
    uintptr_t team_ptr);
__declspec(noinline) void ootp_kbo_fa_signing_success_post_wrapper(
    uintptr_t player_ptr,
    uintptr_t team_ptr);
void start_kbo_fa_requalification_thread(void);

#endif
