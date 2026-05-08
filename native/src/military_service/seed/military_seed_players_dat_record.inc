/* Military service players.dat record-header resolver. Included from native/KBOFix.c. */

static uint32_t kbo_military_resolve_player_id_from_players_dat_record_start(
    const uint8_t* raw,
    size_t read,
    size_t key_pos)
{
    if (raw == NULL || read < 16u || key_pos < 16u) {
        return 0u;
    }

    size_t min_start = key_pos > 512u ? key_pos - 512u : 0u;
    for (size_t start = key_pos - 16u; ; start--) {
        if (start + 16u <= read) {
            uint32_t candidate_id =
                (uint32_t)raw[start]
                | ((uint32_t)raw[start + 1u] << 8)
                | ((uint32_t)raw[start + 2u] << 16)
                | ((uint32_t)raw[start + 3u] << 24);
            uint32_t first_name_id =
                (uint32_t)raw[start + 4u]
                | ((uint32_t)raw[start + 5u] << 8)
                | ((uint32_t)raw[start + 6u] << 16)
                | ((uint32_t)raw[start + 7u] << 24);
            uint32_t last_name_id =
                (uint32_t)raw[start + 8u]
                | ((uint32_t)raw[start + 9u] << 8)
                | ((uint32_t)raw[start + 10u] << 16)
                | ((uint32_t)raw[start + 11u] << 24);
            uint8_t day = raw[start + 12u];
            uint8_t month = raw[start + 13u];
            uint16_t year = (uint16_t)raw[start + 14u] | ((uint16_t)raw[start + 15u] << 8);
            if (candidate_id != 0u
                    && first_name_id != 0u
                    && last_name_id != 0u
                    && day >= 1u && day <= 31u
                    && month >= 1u && month <= 12u
                    && year >= 1800u && year <= 2100u
                    && kbo_military_find_player_by_id(candidate_id) != NULL) {
                return candidate_id;
            }
        }
        if (start == min_start || start == 0u) {
            break;
        }
    }
    return 0u;
}
