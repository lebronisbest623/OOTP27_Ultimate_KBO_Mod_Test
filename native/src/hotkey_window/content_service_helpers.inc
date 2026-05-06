static int kbo_hub_count_service_players(uint32_t service_team_id, int* out_due_60, int* out_due_now)
{
    if (out_due_60 != NULL) { *out_due_60 = 0; }
    if (out_due_now != NULL) { *out_due_now = 0; }
    if (service_team_id == 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int count = 0;
    int due_60 = 0;
    int due_now = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        if (current_team_id != service_team_id && loan_team_id != service_team_id) {
            continue;
        }
        int32_t days_left = kbo_military_effective_days_left(player);
        if (days_left <= 0) { due_now++; }
        if (days_left > 0 && days_left <= 60) { due_60++; }
        count++;
    }

    if (out_due_60 != NULL)  { *out_due_60  = due_60;  }
    if (out_due_now != NULL) { *out_due_now = due_now; }
    return count;
}
