/* Foreign replacement-player seed match API. Included from foreign_replacement_seed.inc. */

static int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type)
{
    if (out_slot_type != NULL) {
        *out_slot_type = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    kbo_ensure_foreign_replacement_player_seeds_loaded();
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (player_id == 0u) {
        return 0;
    }

    int matched = 0;
    kbo_lock_foreign_replacement_player_seeds();
    for (int i = 0; i < g_kbo_foreign_replacement_player_seed_count; i++) {
        KboForeignReplacementPlayerSeed* seed = &g_kbo_foreign_replacement_player_seeds[i];
        if (seed->player_id == player_id) {
            if (out_slot_type != NULL) {
                *out_slot_type = seed->slot_type;
            }
            matched = 1;
            break;
        }
    }
    kbo_unlock_foreign_replacement_player_seeds();
    return matched;
}

