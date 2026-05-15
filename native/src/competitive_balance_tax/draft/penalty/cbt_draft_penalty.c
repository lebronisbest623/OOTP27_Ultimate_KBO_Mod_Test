#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "cbt_draft_penalty.h"
#include "../../records/cbt_records.h"
#include "../../rules/cbt_rules.h"
#include "../../../core/logging/core_log.h"

static volatile LONG g_cbt_draft_penalty_cache_valid = 0;
static KboCbtRecord g_cbt_draft_penalty_records[KBO_CBT_RECORDS_MAX];
static int g_cbt_draft_penalty_record_count = 0;
static uint32_t g_cbt_draft_penalty_min_consecutive = 3u;
static uint32_t g_cbt_draft_penalty_stages = 9u;

void kbo_cbt_draft_penalty_cache_clear(void)
{
    InterlockedExchange(&g_cbt_draft_penalty_cache_valid, 0);
}

static void kbo_cbt_draft_penalty_load_cache(void)
{
    if (InterlockedCompareExchange(&g_cbt_draft_penalty_cache_valid, 1, 0) != 0) {
        return;
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    g_cbt_draft_penalty_min_consecutive = rules.draft_penalty_min_consecutive;
    g_cbt_draft_penalty_stages = rules.draft_penalty_stages;

    g_cbt_draft_penalty_record_count = kbo_cbt_load_records(
        g_cbt_draft_penalty_records, KBO_CBT_RECORDS_MAX, NULL, 0);

    kbo_log_runtimef(
        "KBO CBT draft penalty cache loaded records=%d min_consecutive=%u stages=%u",
        g_cbt_draft_penalty_record_count,
        g_cbt_draft_penalty_min_consecutive,
        g_cbt_draft_penalty_stages);
}

int kbo_cbt_draft_penalty_stages_for_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }
    kbo_cbt_draft_penalty_load_cache();

    int count = g_cbt_draft_penalty_record_count;
    if (count <= 0) {
        return 0;
    }

    /* Find most recent CBT record for this team. */
    uint32_t best_season = 0u;
    int best_index = -1;
    for (int i = 0; i < count; i++) {
        const KboCbtRecord* r = &g_cbt_draft_penalty_records[i];
        if (r->team_id == team_id && r->season > best_season) {
            best_season = r->season;
            best_index = i;
        }
    }
    if (best_index < 0) {
        return 0;
    }

    const KboCbtRecord* rec = &g_cbt_draft_penalty_records[best_index];
    if (rec->overage <= 0) {
        return 0;
    }
    if (rec->consecutive_count < g_cbt_draft_penalty_min_consecutive) {
        return 0;
    }
    return (int)g_cbt_draft_penalty_stages;
}
