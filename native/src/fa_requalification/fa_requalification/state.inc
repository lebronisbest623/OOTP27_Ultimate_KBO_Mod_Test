#include <stdint.h>

/* ---- KBO FA requalification control ---- */

#define KBO_FA_REQUALIFICATION_YEARS 4
#define KBO_FA_REQUALIFICATION_MAX 4096

typedef struct KboFaRequalificationRecord {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t last_fa_year;
    uint32_t fa_count;
} KboFaRequalificationRecord;

static LONG g_kbo_fa_requalification_thread_started = 0;
static LONG g_kbo_fa_requalification_no_date_log_count = 0;
static LONG g_kbo_fa_requalification_no_records_log_count = 0;
static LONG g_kbo_fa_requalification_skip_log_count = 0;
static LONG g_kbo_fa_requalification_hook_skip_log_count = 0;
static volatile LONG g_kbo_fa_requalification_records_lock = 0;
static uint32_t g_kbo_fa_requalification_last_no_records_date = 0;

static int get_kbo_fa_requalification_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("fa_requalification.csv", out, out_size);
}

static void kbo_lock_fa_requalification_records(void)
{
    while (InterlockedCompareExchange(&g_kbo_fa_requalification_records_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_fa_requalification_records(void)
{
    InterlockedExchange(&g_kbo_fa_requalification_records_lock, 0);
}

