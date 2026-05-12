#include "team_add_player_guard_ai_roster_daily_internal.h"

static int32_t kbo_ai_roster_daily_read_score(uintptr_t player_ptr, uint32_t offset)
{
    if (player_ptr == 0u
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player_ptr + offset);
}

static void kbo_ai_roster_daily_read_default_status(
    uint8_t* player,
    uint32_t* out_status24,
    uint32_t* out_status25,
    uint32_t* out_status26)
{
    if (out_status24 != NULL) { *out_status24 = 0u; }
    if (out_status25 != NULL) { *out_status25 = 0u; }
    if (out_status26 != NULL) { *out_status26 = 0u; }
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        return;
    }

    uintptr_t status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    if (status_ptr == 0u || !memory_range_readable((void*)status_ptr, 0x29u)) {
        return;
    }

    uint8_t* status = (uint8_t*)status_ptr;
    if (out_status24 != NULL) { *out_status24 = status[0x24u]; }
    if (out_status25 != NULL) { *out_status25 = status[0x25u]; }
    if (out_status26 != NULL) { *out_status26 = status[0x26u]; }
}

void kbo_ai_roster_daily_fill_summary(KboAiRosterDailyCandidateSummary* summary, int32_t index, uint8_t* player)
{
    if (summary == NULL) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->index = -1;
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    summary->index = index;
    summary->player_ptr = (uintptr_t)player;
    summary->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    summary->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    summary->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    summary->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    summary->league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        summary->default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    kbo_ai_roster_daily_read_default_status(player, &summary->status24, &summary->status25, &summary->status26);
    summary->f25 = (uint32_t)player[0xf25u];
    summary->f62 = (uint32_t)player[0xf62u];
    summary->f65 = (uint32_t)player[0xf65u];
    summary->f06 = kbo_read_player_i16(player, 0xf06u);
    summary->score_fe0 = kbo_ai_roster_daily_read_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    summary->score_fe4 = kbo_ai_roster_daily_read_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    summary->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    summary->talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    summary->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
}

int64_t kbo_ai_roster_daily_score(const KboAiRosterDailyCandidateSummary* summary, uint8_t* player)
{
    if (summary == NULL || player == NULL) {
        return INT64_MIN;
    }

    int64_t score = 0;
    score += (int64_t)summary->overall * 6;
    score += (int64_t)summary->ratings * 4;
    score += (int64_t)summary->talent * 2;
    score += (int64_t)summary->score_fe4 * 150;
    score += (int64_t)summary->score_fe0 * 80;
    score += (int64_t)summary->f25 * 12;
    score += (int64_t)kbo_read_player_i16(player, 0xf06u) * 8;
    score -= (int64_t)summary->status26 * 40;
    return score;
}
