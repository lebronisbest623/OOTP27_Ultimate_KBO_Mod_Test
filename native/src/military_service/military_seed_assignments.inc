/* Applying configured military service seed assignments to players. Included from native/src/military_service_loan.inc. */

static int kbo_apply_military_service_seed_assignments(uint8_t* sang, uint8_t* kpb, const char* source)
{
    if (sang == NULL && kpb == NULL) {
        return 0;
    }
    kbo_ensure_military_service_seeds_loaded();

    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0u;
    uint32_t today_serial = kbo_current_date_serial();
    int applied = 0;

    kbo_lock_military_service_seeds();
    for (int i = 0; i < g_kbo_military_service_seed_count; i++) {
        KboMilitaryServiceSeed* seed = &g_kbo_military_service_seeds[i];
        if (seed->player_id == 0u) {
            continue;
        }

        uint8_t* player = kbo_military_find_player_by_id(seed->player_id);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            continue;
        }

        uint8_t* service_team = (seed->service_team_code[0] != '\0' && _stricmp(seed->service_team_code, "KPB") == 0)
            ? kpb
            : sang;
        uint32_t service_team_id = service_team != NULL
            ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_ID_OFFSET)
            : 0u;
        if (service_team_id == 0u) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t loan_team_id    = *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
        uint32_t seed_return_serial = kbo_military_yyyymmdd_to_serial(seed->service_return_yyyymmdd);
        if (today_serial != 0u
                && seed_return_serial != 0u
                && seed_return_serial <= today_serial
                && current_team_id != service_team_id
                && loan_team_id != service_team_id) {
            LONG skip_log = InterlockedIncrement(&g_military_seed_expired_skip_log_count);
            if (skip_log <= 40) {
                append_logf(
                    "KBO military service seed skipped expired key=%s player=%u return=%u today_serial=%u current_team=%u service_team=%u",
                    seed->key,
                    seed->player_id,
                    seed->service_return_yyyymmdd,
                    today_serial,
                    current_team_id,
                    service_team_id);
            }
            continue;
        }

        uint32_t original_team_id = 0u;
        uint32_t original_league_id = 0u;
        if (seed->original_team_code[0] != '\0') {
            uint8_t* original_team = NULL;
            if (seed->original_team_code[0] >= '0' && seed->original_team_code[0] <= '9') {
                original_team = find_kbo_team_by_numeric_id_any_league((uint32_t)strtoul(seed->original_team_code, NULL, 10), 0);
            } else {
                original_team = find_kbo_team_by_csv_id_any_league(seed->original_team_code, 0);
            }
            if (original_team != NULL) {
                original_team_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_ID_OFFSET);
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }
        if (original_team_id == 0u) {
            original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            original_league_id = *(uint32_t*)(player + 0x4c);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            original_team_id = *(uint32_t*)(player + 0x5c);
        }
        if (original_team_id == 0u
                || original_team_id == service_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            continue;
        }
        if (original_league_id == 0u) {
            uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(original_team_id, 0);
            if (original_team != NULL) {
                original_league_id = *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            }
        }

        if (current_team_id != service_team_id && loan_team_id != service_team_id) {
            int called_pre_change = 0;
            int called_register = 0;
            int called_attach = 0;
            uint32_t service_league_id = *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
            kbo_assign_player_to_team_internal(
                player,
                service_team,
                service_league_id,
                0,
                &called_pre_change,
                &called_register,
                &called_attach);
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = original_team_id;
            *(uint32_t*)(player + 0x5c) = original_team_id;
            if (original_league_id != 0u) {
                *(uint32_t*)(player + 0x4c) = original_league_id;
            }
            append_logf(
                "KBO military service seed assigned to service key=%s player=%u old_team=%u service_team=%u original_team=%u pre=%d register=%d attach=%d",
                seed->key,
                seed->player_id,
                current_team_id,
                service_team_id,
                original_team_id,
                called_pre_change,
                called_register,
                called_attach);
        }

        register_active_kbo_military_loan(
            seed->player_id,
            (uintptr_t)player,
            original_team_id,
            original_league_id,
            service_team_id,
            *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET));

        int active_index = find_active_kbo_military_loan_index(seed->player_id);
        if (active_index >= 0) {
            KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
            uint32_t start_serial = kbo_military_yyyymmdd_to_serial(seed->service_start_yyyymmdd);
            if (start_serial != 0u) {
                loan->service_start_date_serial = start_serial;
            }
            loan->service_total_days = seed->service_total_days > 0
                ? seed->service_total_days
                : KBO_MILITARY_SERVICE_DAYS;
            uint32_t return_serial = seed_return_serial;
            if (return_serial == 0u && loan->service_start_date_serial != 0u) {
                return_serial = loan->service_start_date_serial + (uint32_t)loan->service_total_days;
            }
            if (return_serial != 0u) {
                loan->service_return_date_serial = return_serial;
            }
            if (today_serial != 0u && loan->service_return_date_serial != 0u) {
                kbo_set_military_days_left(
                    player,
                    kbo_military_days_left_from_return_serial(loan->service_return_date_serial, today_serial));
            }
            player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 1;
            clear_kbo_military_unavailable_flags(player);
            applied++;
        }
    }
    kbo_unlock_military_service_seeds();

    if (applied > 0) {
        append_logf("KBO military service seed applied source=%s assignments=%d", source != NULL ? source : "", applied);
    }
    return applied;
}

