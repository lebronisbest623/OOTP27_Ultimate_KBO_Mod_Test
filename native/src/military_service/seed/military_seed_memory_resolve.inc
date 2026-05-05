/* Military service seed resolution from live player memory. Included from native/src/military_service/military_service_seed_store.inc. */

static uint32_t kbo_resolve_military_service_seed_key_from_memory(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0u;
    }
    if (key[0] >= '0' && key[0] <= '9') {
        return (uint32_t)strtoul(key, NULL, 10);
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0u;
    }

    uint32_t first_match = 0u;
    int matches = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (!kbo_military_player_memory_contains_seed_key(player, key)) {
            continue;
        }
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u) {
            continue;
        }
        if (first_match == 0u) {
            first_match = player_id;
        }
        matches++;
    }
    if (first_match != 0u) {
        append_logf("KBO military service seed resolved key=%s player=%u matches=%d", key, first_match, matches);
    }
    return first_match;
}

