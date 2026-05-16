#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "foreign_retention_guard_internal.h"
#include "../../core/logging/core_log.h"
#include "../common/dates/foreign_waiver_date.h"
#include "../common/policy/foreign_player_policy.h"
#include "../rights/query/foreign_waiver_rights_query.h"

static KboLock g_kbo_foreign_retention_guard_lock = KBO_LOCK_INIT;
static KboForeignRetentionGuardRecord g_kbo_foreign_retention_guard[KBO_FOREIGN_RETENTION_GUARD_MAX] = {{0}};
static volatile LONG g_kbo_foreign_retention_guard_record_log_count = 0;
static volatile LONG g_kbo_foreign_retention_guard_skip_log_count = 0;

static void kbo_foreign_retention_guard_lock(void)
{
    kbo_lock_enter(&g_kbo_foreign_retention_guard_lock);
}

static void kbo_foreign_retention_guard_unlock(void)
{
    kbo_lock_leave(&g_kbo_foreign_retention_guard_lock);
}

static int kbo_foreign_retention_guard_record_expired(
    const KboForeignRetentionGuardRecord* rec,
    uint32_t today)
{
    return rec == NULL
        || rec->active == 0u
        || rec->player_id == 0u
        || rec->team_id == 0u
        || (today != 0u && rec->expires_on_yyyymmdd != 0u && today > rec->expires_on_yyyymmdd);
}

void kbo_foreign_retention_guard_record_team_add(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t league_id,
    uint32_t signed_on_yyyymmdd,
    uint32_t contract_status,
    uint32_t contract_start_year,
    const int32_t* contract_years,
    uint32_t contract_year_count)
{
    if (player_id == 0u || team_id == 0u || league_id == 0u || signed_on_yyyymmdd == 0u) {
        return;
    }

    uint32_t expires_on = kbo_add_days_yyyymmdd(
        signed_on_yyyymmdd,
        (uint32_t)kbo_foreign_player_policy()->retention_guard_days);
    if (expires_on == 0u) {
        expires_on = signed_on_yyyymmdd;
    }

    int log_recorded = 0;
    int slot = -1;
    kbo_foreign_retention_guard_lock();
    for (int i = 0; i < KBO_FOREIGN_RETENTION_GUARD_MAX; i++) {
        if (g_kbo_foreign_retention_guard[i].active
                && g_kbo_foreign_retention_guard[i].player_id == player_id
                && g_kbo_foreign_retention_guard[i].team_id == team_id) {
            slot = i;
            break;
        }
        if (slot < 0 && kbo_foreign_retention_guard_record_expired(
                &g_kbo_foreign_retention_guard[i],
                signed_on_yyyymmdd)) {
            slot = i;
        }
    }

    if (slot >= 0) {
        KboForeignRetentionGuardRecord* rec = &g_kbo_foreign_retention_guard[slot];
        log_recorded = rec->active == 0u
            || rec->player_id != player_id
            || rec->team_id != team_id
            || rec->signed_on_yyyymmdd != signed_on_yyyymmdd;
        rec->player_id = player_id;
        rec->team_id = team_id;
        rec->league_id = league_id;
        rec->signed_on_yyyymmdd = signed_on_yyyymmdd;
        rec->expires_on_yyyymmdd = expires_on;
        rec->last_repaired_on_yyyymmdd = 0u;
        rec->contract_status = contract_status;
        rec->contract_start_year = contract_start_year;
        for (uint32_t year = 0u; year < OOTP27_PLAYER_CONTRACT_SALARY_YEARS; year++) {
            rec->contract_years[year] = contract_years != NULL && year < contract_year_count
                ? contract_years[year]
                : 0;
        }
        rec->repair_count = 0u;
        rec->active = 1u;
    }
    kbo_foreign_retention_guard_unlock();

    if (slot < 0) {
        LONG skip_slot = InterlockedIncrement(&g_kbo_foreign_retention_guard_skip_log_count);
        if (skip_slot <= 40) {
            kbo_log_runtimef(
                "foreign retention guard: record skipped reason=guard_full player=%u team=%u league=%u signed_on=%u expires_on=%u",
                player_id,
                team_id,
                league_id,
                signed_on_yyyymmdd,
                expires_on);
        }
        return;
    }

    kbo_consume_foreign_waiver_right_after_holder_signing(team_id, player_id);

    if (log_recorded) {
        LONG record_slot = InterlockedIncrement(&g_kbo_foreign_retention_guard_record_log_count);
        if (record_slot <= 200) {
            kbo_log_runtimef(
                "foreign retention guard: recorded holder signing player=%u team=%u league=%u signed_on=%u expires_on=%u contract_status=%u contract_start_year=%u salary_y1=%d",
                player_id,
                team_id,
                league_id,
                signed_on_yyyymmdd,
                expires_on,
                contract_status,
                contract_start_year,
                contract_years != NULL && contract_year_count > 0u ? contract_years[0] : 0);
        }
    }
}

void kbo_foreign_retention_guard_snapshot(
    KboForeignRetentionGuardRecord* out_records,
    int* out_count,
    uint32_t today)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (out_records == NULL || out_count == NULL) {
        return;
    }

    kbo_foreign_retention_guard_lock();
    int count = 0;
    for (int i = 0; i < KBO_FOREIGN_RETENTION_GUARD_MAX; i++) {
        KboForeignRetentionGuardRecord* rec = &g_kbo_foreign_retention_guard[i];
        if (kbo_foreign_retention_guard_record_expired(rec, today)) {
            rec->active = 0u;
            continue;
        }
        if (count < KBO_FOREIGN_RETENTION_GUARD_MAX) {
            out_records[count++] = *rec;
        }
    }
    kbo_foreign_retention_guard_unlock();
    *out_count = count;
}

void kbo_foreign_retention_guard_mark_repaired(
    uint32_t player_id,
    uint32_t team_id,
    uint32_t today)
{
    kbo_foreign_retention_guard_lock();
    for (int i = 0; i < KBO_FOREIGN_RETENTION_GUARD_MAX; i++) {
        KboForeignRetentionGuardRecord* rec = &g_kbo_foreign_retention_guard[i];
        if (rec->active && rec->player_id == player_id && rec->team_id == team_id) {
            rec->last_repaired_on_yyyymmdd = today;
            if (rec->repair_count < 255u) {
                rec->repair_count++;
            }
            break;
        }
    }
    kbo_foreign_retention_guard_unlock();
}
