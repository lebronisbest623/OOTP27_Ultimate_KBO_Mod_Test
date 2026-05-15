#include "../foreign_injury_scanner_internal.h"

#define KBO_FOREIGN_INJURY_PENDING_INACTIVE_MAX 512

typedef struct KboForeignInjuryPendingInactive {
    uint32_t player_id;
    uint32_t team_id;
    uint32_t first_seen_yyyymmdd;
    uint32_t last_seen_yyyymmdd;
} KboForeignInjuryPendingInactive;

static KboForeignInjuryPendingInactive g_kbo_foreign_injury_pending_inactive[KBO_FOREIGN_INJURY_PENDING_INACTIVE_MAX];

void kbo_foreign_injury_clear_pending_inactive(uint32_t player_id)
{
    if (player_id == 0u) {
        return;
    }
    for (int i = 0; i < KBO_FOREIGN_INJURY_PENDING_INACTIVE_MAX; i++) {
        if (g_kbo_foreign_injury_pending_inactive[i].player_id == player_id) {
            memset(&g_kbo_foreign_injury_pending_inactive[i], 0, sizeof(g_kbo_foreign_injury_pending_inactive[i]));
            return;
        }
    }
}

static int kbo_foreign_injury_pending_inactive_elapsed_days(uint32_t first_seen, uint32_t today)
{
    if (first_seen == 0u || today == 0u || today <= first_seen) {
        return 0;
    }
    uint32_t cursor = first_seen;
    for (int days = 1; days <= 366; days++) {
        cursor = kbo_add_days_yyyymmdd(cursor, 1u);
        if (cursor == 0u) {
            return 0;
        }
        if (cursor >= today) {
            return days;
        }
    }
    return 366;
}

int kbo_foreign_injury_note_pending_inactive(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t today_yyyymmdd)
{
    if (player_id == 0u || team_id == 0u || today_yyyymmdd == 0u) {
        return 0;
    }

    int free_slot = -1;
    for (int i = 0; i < KBO_FOREIGN_INJURY_PENDING_INACTIVE_MAX; i++) {
        KboForeignInjuryPendingInactive* rec = &g_kbo_foreign_injury_pending_inactive[i];
        if (rec->player_id == player_id) {
            if (rec->team_id != team_id || rec->first_seen_yyyymmdd == 0u || today_yyyymmdd < rec->first_seen_yyyymmdd) {
                rec->team_id = team_id;
                rec->first_seen_yyyymmdd = today_yyyymmdd;
            }
            rec->last_seen_yyyymmdd = today_yyyymmdd;
            return kbo_foreign_injury_pending_inactive_elapsed_days(rec->first_seen_yyyymmdd, today_yyyymmdd);
        }
        if (free_slot < 0 && rec->player_id == 0u) {
            free_slot = i;
        }
    }

    if (free_slot < 0) {
        free_slot = (int)(player_id % KBO_FOREIGN_INJURY_PENDING_INACTIVE_MAX);
    }
    g_kbo_foreign_injury_pending_inactive[free_slot].player_id = player_id;
    g_kbo_foreign_injury_pending_inactive[free_slot].team_id = team_id;
    g_kbo_foreign_injury_pending_inactive[free_slot].first_seen_yyyymmdd = today_yyyymmdd;
    g_kbo_foreign_injury_pending_inactive[free_slot].last_seen_yyyymmdd = today_yyyymmdd;
    return 0;
}
