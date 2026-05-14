#include "../internal/foreign_waiver_rights_internal.h"
#include "../../../core/csv/core_csv.h"

/* Foreign reserve-right CSV loading. */

int kbo_load_foreign_waiver_rights(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_foreign_waiver_rights_path(path, sizeof(path))) {
        return 0;
    }

    KboCsvReader* reader = kbo_csv_reader_open(path);
    if (reader == NULL) {
        return 0;
    }

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    g_kbo_foreign_waiver_rights_count = 0;
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    int deduped = 0;
    while (kbo_csv_reader_next_row(reader)) {
        char fields[5][64];
        int field_count = kbo_csv_reader_read_trimmed_fields(reader, (char*)fields, sizeof(fields[0]), 5);
        if (field_count < 5
                || fields[0][0] == '#'
                || fields[0][0] == ';'
                || fields[0][0] < '0'
                || fields[0][0] > '9') {
            continue;
        }

        uint32_t player_id = kbo_csv_parse_u32_text(fields[0], 10);
        uint32_t team_id = kbo_csv_parse_u32_text(fields[1], 10);
        uint32_t league_id = kbo_csv_parse_u32_text(fields[2], 10);
        uint32_t retained_on = kbo_csv_parse_u32_text(fields[3], 10);
        uint32_t expires_on = kbo_csv_parse_u32_text(fields[4], 10);
        if (player_id != 0u && team_id != 0u && league_id != 0u) {
            while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
                Sleep(0);
            }
            int existing_index = -1;
            for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
                if (g_kbo_foreign_waiver_rights[i].player_id == player_id) {
                    existing_index = i;
                    break;
                }
            }
            if (existing_index >= 0) {
                KboForeignWaiverRetention* existing = &g_kbo_foreign_waiver_rights[existing_index];
                if (retained_on >= existing->retained_on_yyyymmdd) {
                    *existing = (KboForeignWaiverRetention){
                        player_id,
                        team_id,
                        league_id,
                        retained_on,
                        expires_on
                    };
                }
                deduped++;
            } else if (g_kbo_foreign_waiver_rights_count < KBO_FOREIGN_WAIVER_RIGHTS_MAX) {
                g_kbo_foreign_waiver_rights[g_kbo_foreign_waiver_rights_count++] = (KboForeignWaiverRetention){
                    player_id,
                    team_id,
                    league_id,
                    retained_on,
                    expires_on
                };
            }
            InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);
        }
    }
    kbo_csv_reader_close(reader);
    kbo_log_runtimef("foreign reserve rights: loaded=%d deduped=%d path=%s", g_kbo_foreign_waiver_rights_count, deduped, path);
    if (deduped > 0) {
        kbo_persist_foreign_waiver_rights();
    }
    return 1;
}

