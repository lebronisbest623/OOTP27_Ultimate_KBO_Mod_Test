#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_RECORDS_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_RECORDS_H_

#include <stddef.h>
#include <stdint.h>

#define KBO_CBT_RECORDS_MAX 500

typedef struct KboCbtRecord {
    uint32_t season;
    uint32_t team_id;
    int32_t  payroll;
    int32_t  threshold;
    int32_t  overage;
    uint32_t tax_rate_pct;
    int32_t  tax_amount;
    uint32_t consecutive_count;
    uint32_t processed_date;
    char     team_name[64];
} KboCbtRecord;

int      kbo_cbt_load_records(KboCbtRecord* records, int max, char* path_out, size_t path_size);
int      kbo_cbt_save_records(const KboCbtRecord* records, int count);
uint32_t kbo_cbt_get_consecutive_count(const KboCbtRecord* records, int count, uint32_t team_id, uint32_t before_season);
int      kbo_cbt_find_record(const KboCbtRecord* records, int count, uint32_t season, uint32_t team_id);

#endif
