#include "../military_seed_registry_internal.h"

int kbo_seed_registry_service_team_id_matches(
    uint32_t team_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id)
{
    return team_id != 0u
        && (team_id == service_team_id
            || (sang_id != 0u && team_id == sang_id)
            || (kpb_id != 0u && team_id == kpb_id));
}

int kbo_military_original_team_from_seed(
    uint32_t player_id,
    uint32_t service_team_id,
    uint32_t sang_id,
    uint32_t kpb_id,
    uint32_t* out_original_team_id,
    uint32_t* out_original_league_id)
{
    if (out_original_team_id != NULL) { *out_original_team_id = 0u; }
    if (out_original_league_id != NULL) { *out_original_league_id = 0u; }
    if (player_id == 0u) {
        return 0;
    }

    kbo_ensure_military_service_seeds_loaded();
    char original_team_code[KBO_MILITARY_SERVICE_TEAM_BYTES] = {0};
    kbo_lock_military_service_seeds();
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id != player_id || seed->original_team_code[0] == '\0') {
            continue;
        }
        snprintf(original_team_code, sizeof(original_team_code), "%s", seed->original_team_code);
        break;
    }
    kbo_unlock_military_service_seeds();

    if (original_team_code[0] == '\0') {
        return 0;
    }

    uint8_t* original_team = kbo_military_find_team_from_seed_code(original_team_code);
    if (original_team == NULL || !memory_range_readable(original_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t original_team_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (kbo_seed_registry_service_team_id_matches(original_team_id, service_team_id, sang_id, kpb_id)) {
        return 0;
    }

    if (out_original_team_id != NULL) { *out_original_team_id = original_team_id; }
    if (out_original_league_id != NULL) {
        *out_original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    }
    return 1;
}

