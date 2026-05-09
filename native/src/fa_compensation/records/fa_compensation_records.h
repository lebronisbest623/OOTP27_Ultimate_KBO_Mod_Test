#ifndef KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_RECORDS_H_
#define KBOFIX_SRC_FA_COMPENSATION_FA_COMPENSATION_RECORDS_H_

#include <stddef.h>
#include <stdint.h>

#include "../state/fa_compensation_state.h"

int kbo_load_fa_compensation_records(
    KboFaCompensationRecord* records,
    int max_records,
    char* out_path,
    size_t out_path_size);
int kbo_persist_fa_compensation_records(const KboFaCompensationRecord* records, int record_count);
int kbo_append_fa_compensation_record(const KboFaCompensationRecord* rec);
int kbo_fa_compensation_find_existing(
    const KboFaCompensationRecord* records,
    int record_count,
    uint32_t player_id,
    uint32_t season,
    uint32_t original_team_id,
    uint32_t signing_team_id);

#endif
