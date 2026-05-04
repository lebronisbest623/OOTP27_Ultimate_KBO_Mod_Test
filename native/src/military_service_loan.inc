typedef void (__fastcall *OotpMilitaryServiceEntryFn)(void* player);

static int32_t kbo_military_days_left(uint8_t* player);
static void    kbo_set_military_days_left(uint8_t* player, int32_t days_left);

/* ---- Date serial helpers ---- */

static uint32_t kbo_date_serial(uint32_t year, uint32_t month, uint32_t day)
{
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }

    uint32_t y = year - 1u;
    uint32_t serial = y * 365u + y / 4u - y / 100u + y / 400u;
    serial += days_before_month[month - 1u];
    if (month > 2 && kbo_is_leap_year(year)) {
        serial++;
    }
    serial += day;
    return serial;
}

static uint32_t kbo_current_date_serial(void)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        return 0;
    }
    return kbo_date_serial(year, month, day);
}

/* ---- Active loan registry ---- */

static int find_active_kbo_military_loan_index(uint32_t player_id)
{
    if (player_id == 0) {
        return -1;
    }

    LONG count = g_active_military_loan_count;
    if (count < 0) { count = 0; }
    if (count > OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) { count = OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS; }

    for (LONG i = 0; i < count; i++) {
        if (g_active_military_loans[i].player_id == player_id) {
            return (int)i;
        }
    }
    return -1;
}

static void register_active_kbo_military_loan(
    uint32_t player_id,
    uintptr_t player_ptr,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t service_league_id)
{
    if (player_id == 0 || service_team_id == 0) {
        return;
    }

    int existing = find_active_kbo_military_loan_index(player_id);
    if (existing >= 0) {
        KboMilitaryActiveLoan* loan = &g_active_military_loans[existing];
        loan->player_ptr = player_ptr;
        if (original_team_id  != 0) { loan->original_team_id  = original_team_id;  }
        if (original_league_id != 0) { loan->original_league_id = original_league_id; }
        loan->service_team_id   = service_team_id;
        loan->service_league_id = service_league_id;
        uint32_t today = kbo_current_date_serial();
        int32_t current_days_left = (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr))
            ? kbo_military_days_left((uint8_t*)player_ptr)
            : KBO_MILITARY_SERVICE_DAYS;
        if (loan->service_start_date_serial == 0) {
            if (current_days_left > 0
                    && current_days_left < KBO_MILITARY_SERVICE_DAYS
                    && today > (uint32_t)(KBO_MILITARY_SERVICE_DAYS - current_days_left)) {
                loan->service_start_date_serial = today - (uint32_t)(KBO_MILITARY_SERVICE_DAYS - current_days_left);
            } else {
                loan->service_start_date_serial = today;
            }
        }
        loan->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        if (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr)
                && current_days_left > KBO_MILITARY_SERVICE_DAYS) {
            kbo_set_military_days_left((uint8_t*)player_ptr, KBO_MILITARY_SERVICE_DAYS);
        }
        return;
    }

    LONG slot = InterlockedIncrement(&g_active_military_loan_count) - 1;
    if (slot < 0 || slot >= OOTP27_KBO_MAX_SPECIAL_HISTORY_KEYS) {
        InterlockedDecrement(&g_active_military_loan_count);
        return;
    }

    KboMilitaryActiveLoan* loan = &g_active_military_loans[slot];
    loan->player_id              = player_id;
    loan->player_ptr             = player_ptr;
    loan->original_team_id       = original_team_id;
    loan->original_league_id     = original_league_id;
    loan->service_team_id        = service_team_id;
    loan->service_league_id      = service_league_id;
    loan->service_start_date_serial = kbo_current_date_serial();
    loan->service_total_days     = KBO_MILITARY_SERVICE_DAYS;
    if (player_ptr != 0 && kbo_player_pointer_plausible(player_ptr)) {
        kbo_set_military_days_left((uint8_t*)player_ptr, KBO_MILITARY_SERVICE_DAYS);
    }
}

static void unregister_active_kbo_military_loan(uint32_t player_id)
{
    int index = find_active_kbo_military_loan_index(player_id);
    if (index >= 0) {
        g_active_military_loans[index].player_id = 0;
    }
}

static int kbo_player_is_registered_active_military_loan(uintptr_t player_ptr)
{
    if (player_ptr == 0 || !memory_range_readable(
            (void*)(player_ptr + OOTP27_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
        return 0;
    }
    uint32_t player_id = *(uint32_t*)(player_ptr + OOTP27_PLAYER_ID_OFFSET);
    return find_active_kbo_military_loan_index(player_id) >= 0;
}

/* ---- Player military state helpers ---- */

static int32_t kbo_military_days_left(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET, sizeof(int16_t))) {
        return 0;
    }
    return (int32_t)(*(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET));
}

static void kbo_set_military_days_left(uint8_t* player, int32_t days_left)
{
    if (player == NULL || !memory_range_readable(
            player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET, sizeof(int16_t))) {
        return;
    }
    *(int16_t*)(player + OOTP27_PLAYER_INJURY_DAYS_LEFT_OFFSET) =
        days_left > 0 ? (int16_t)days_left : 0;
}

static void clear_kbo_military_unavailable_flags(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }
    player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET]           = 0;
    player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 0;
    player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET]             = 0;
}

static void clear_kbo_military_status_flags(uint8_t* player)
{
    clear_kbo_military_unavailable_flags(player);
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] = 0;
    }
}

static void complete_kbo_military_service_status(uint8_t* player)
{
    clear_kbo_military_status_flags(player);
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] = 1;
        kbo_set_military_days_left(player, 0);
    }
}

/* ---- Native loan helpers ---- */

static int kbo_player_native_on_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }
    return *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) > 0
        && player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0
        && player[OOTP27_PLAYER_DFA_FLAG_OFFSET] == 0
        && player[OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET] != 0;
}

static int kbo_clear_native_player_loan(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET) = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0;
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET] = 1;
    return !kbo_player_native_on_loan(player);
}

/* ---- Return player from service ---- */

static int return_completed_kbo_military_loan_player(
    uint8_t* player,
    const char* source,
    uint32_t vector_offset,
    int require_registered)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 0;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int active_index = find_active_kbo_military_loan_index(player_id);
    if (require_registered && active_index < 0) {
        return 0;
    }

    uint32_t sang_id = 0;
    uint32_t kpb_id  = 0;
    uint32_t registered_service_team_id  = 0;
    uint32_t registered_original_team_id = 0;
    uint32_t registered_original_league_id = 0;

    if (active_index >= 0) {
        KboMilitaryActiveLoan* active = &g_active_military_loans[active_index];
        active->player_ptr              = (uintptr_t)player;
        registered_service_team_id      = active->service_team_id;
        registered_original_team_id     = active->original_team_id;
        registered_original_league_id   = active->original_league_id;
    } else {
        uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
        uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
        sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
        kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    }

    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t service_team_id = 0;
    if (registered_service_team_id != 0) {
        service_team_id = registered_service_team_id;
    } else if (current_team_id != 0
               && (current_team_id == sang_id || current_team_id == kpb_id)) {
        service_team_id = current_team_id;
    } else {
        return 0;
    }

    uint32_t original_team_id =
        registered_original_team_id != 0
            ? registered_original_team_id
            : *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_league_id =
        registered_original_league_id != 0
            ? registered_original_league_id
            : *(uint32_t*)(player + 0x4c);

    if (original_team_id == 0
            || original_team_id == service_team_id
            || original_team_id == sang_id
            || original_team_id == kpb_id) {
        original_team_id = *(uint32_t*)(player + 0x5c);
    }
    if (original_team_id == 0
            || original_team_id == service_team_id
            || original_team_id == sang_id
            || original_team_id == kpb_id) {
        return 0;
    }

    int32_t days_left = kbo_military_days_left(player);
    if (days_left > 0) {
        clear_kbo_military_unavailable_flags(player);
        return 0;
    }

    uint8_t* original_team = find_kbo_team_by_numeric_id_any_league(original_team_id, 0);
    if (original_team == NULL) {
        return 0;
    }

    int called_pre_change = 0;
    int called_register   = 0;
    int called_attach     = 0;
    int native_loan_before  = kbo_player_native_on_loan(player);
    int native_loan_cleared = native_loan_before ? kbo_clear_native_player_loan(player) : 0;
    uint32_t fallback_league_id = original_league_id != 0
        ? original_league_id
        : *(uint32_t*)(original_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);

    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != original_team_id) {
        kbo_assign_player_to_team_like_ootp(
            player, original_team, fallback_league_id,
            &called_pre_change, &called_register, &called_attach);
    }

    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET)   = 0;
    *(uint32_t*)(player + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET) = 0;
    player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET]   = 0;
    player[OOTP27_PLAYER_LOAN_CLEARED_MARKER_OFFSET] = 1;
    player[OOTP27_PLAYER_LOAN_DIRTY_FLAG_OFFSET]    = 0;
    complete_kbo_military_service_status(player);
    unregister_active_kbo_military_loan(player_id);

    uint8_t old_restricted     = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
    uint8_t old_secondary      = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
    uint8_t old_injury_active  = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
    uint8_t old_mil_exempt     = player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET];
    uint8_t old_mil_active     = player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];

    char history_date[16] = {0};
    uint32_t year = 0;
    if (!kbo_current_year_relaxed(&year) || year == 0) { year = 2001; }
    kbo_current_history_date(history_date, sizeof(history_date), year, "military_service_return");
    append_special_player_history_csv(
        player_id, (uint16_t)year, history_date,
        "military_service_return",
        "Returned to his original KBO organization after military service assignment.");

    LONG log_index = InterlockedIncrement(&g_military_loan_return_log_count);
    if (log_index <= 120) {
        append_logf(
            "KBO military assignment returned #%ld source=%s player_id=%u"
            " service_team=%u original_team=%u"
            " current_team=%u current_league=%u org_team=%u days_left=%d"
            " native_loan_before=%d native_loan_cleared=%d"
            " old_restricted=%u old_secondary=%u old_injury=%u"
            " old_mil_exempt=%u mil_exempt=%u old_mil_active=%u mil_active=%u"
            " pre_change=%d register=%d attach=%d player=%p vector_off=0x%x",
            log_index,
            source != NULL ? source : "",
            player_id,
            service_team_id,
            original_team_id,
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
            *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
            *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
            days_left,
            native_loan_before,
            native_loan_cleared,
            (uint32_t)old_restricted,
            (uint32_t)old_secondary,
            (uint32_t)old_injury_active,
            (uint32_t)old_mil_exempt,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET],
            (uint32_t)old_mil_active,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET],
            called_pre_change,
            called_register,
            called_attach,
            player,
            vector_offset);
    }

    return 1;
}

/* ---- Background tick ---- */

static int tick_kbo_military_service_days(const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }

    flush_pending_special_player_history_sql(source);

    uint32_t today_serial = kbo_current_date_serial();
    if (today_serial == 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t   player_count  = 0;
    uint32_t  vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        return 0;
    }

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    if (sang_id == 0 && kpb_id == 0) {
        return 0;
    }

    int tracked          = 0;
    int monitored        = 0;
    int managed          = 0;
    int returned         = 0;
    int newly_registered = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }

        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        if (current_team_id == 0
                || (current_team_id != sang_id && current_team_id != kpb_id)) {
            continue;
        }

        int32_t days_left = kbo_military_days_left(player);
        if (days_left <= 0) {
            returned += return_completed_kbo_military_loan_player(player, source, vector_offset, 0);
            continue;
        }

        tracked++;
        uint32_t player_id        = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        uint32_t original_league_id = *(uint32_t*)(player + 0x4c);
        if (original_team_id == 0
                || original_team_id == current_team_id
                || original_team_id == sang_id
                || original_team_id == kpb_id) {
            original_team_id = *(uint32_t*)(player + 0x5c);
        }

        int active_index = find_active_kbo_military_loan_index(player_id);
        if (active_index < 0
                && original_team_id != 0
                && original_team_id != current_team_id
                && original_team_id != sang_id
                && original_team_id != kpb_id) {
            uint8_t* service_team   = (current_team_id == sang_id) ? sang : kpb;
            uint32_t service_league = service_team != NULL
                ? *(uint32_t*)(service_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET)
                : 0;
            register_active_kbo_military_loan(
                player_id, player_ptr,
                original_team_id, original_league_id,
                current_team_id,
                service_league != 0 ? service_league
                    : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET));
            active_index = find_active_kbo_military_loan_index(player_id);
            newly_registered++;
        }

        if (active_index < 0) {
            clear_kbo_military_unavailable_flags(player);
            continue;
        }

        KboMilitaryActiveLoan* loan = &g_active_military_loans[active_index];
        loan->player_ptr = player_ptr;
        if (loan->service_start_date_serial == 0
                || loan->service_start_date_serial > today_serial) {
            loan->service_start_date_serial = today_serial;
            loan->service_total_days        = KBO_MILITARY_SERVICE_DAYS;
            clear_kbo_military_unavailable_flags(player);
            continue;
        }
        if (loan->service_total_days <= 0) {
            loan->service_total_days = KBO_MILITARY_SERVICE_DAYS;
        }

        uint32_t elapsed_days    = today_serial - loan->service_start_date_serial;
        int32_t  managed_days_left = loan->service_total_days - (int32_t)elapsed_days;
        if (managed_days_left != days_left) {
            kbo_set_military_days_left(player, managed_days_left);
            days_left = kbo_military_days_left(player);
            managed++;
        }
        if (days_left <= 0) {
            returned += return_completed_kbo_military_loan_player(player, source, vector_offset, 0);
            continue;
        }

        clear_kbo_military_unavailable_flags(player);
        monitored++;
    }

    LONG log_index = InterlockedIncrement(&g_military_days_tick_log_count);
    if (returned > 0 || newly_registered > 0 || managed > 0 || log_index <= 20) {
        append_logf(
            "KBO military service day tick source=%s date_serial=%u"
            " tracked=%d newly_registered=%d monitored=%d managed=%d returned=%d"
            " count=%d vector_off=0x%x",
            source != NULL ? source : "",
            today_serial,
            tracked, newly_registered, monitored, managed, returned,
            player_count, vector_offset);
    }

    return returned;
}

static DWORD WINAPI kbo_military_days_tick_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO military service day tick thread started");
    for (;;) {
        Sleep(2000);
        tick_kbo_military_service_days("military_days_tick");
    }
    return 0;
}

static void start_kbo_military_days_tick_thread(void)
{
    if (!kbo_fix_enabled()) {
        return;
    }
    if (InterlockedCompareExchange(&g_military_days_tick_started, 1, 0) != 0) {
        return;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_military_days_tick_thread, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    } else {
        InterlockedExchange(&g_military_days_tick_started, 0);
        append_log_line("KBO military service day tick thread failed to start");
    }
}

/* ---- Hook wrappers ---- */

__declspec(noinline) void ootp_kbo_military_service_entry_wrapper(
    uintptr_t player_ptr,
    uintptr_t original_func_ptr)
{
    OotpMilitaryServiceEntryFn original_func = (OotpMilitaryServiceEntryFn)original_func_ptr;
    if (original_func != NULL) {
        original_func((void*)player_ptr);
    }

    LONG log_index = InterlockedIncrement(&g_military_service_entry_log_count);
    if (log_index <= 120) {
        uint32_t player_id    = 0;
        uint32_t parent_team  = 0;
        uint32_t active_team  = 0;
        uint32_t cur_league   = 0;
        uint32_t loan_team    = 0;
        uint32_t loan_league  = 0;
        int32_t  days_left    = INT32_MIN;
        uint8_t  restricted   = 0;
        uint8_t  secondary    = 0;
        uint8_t  inj_active   = 0;
        uint8_t  mil_active   = 0;
        if (kbo_player_pointer_plausible(player_ptr)) {
            uint8_t* p = (uint8_t*)player_ptr;
            player_id   = *(uint32_t*)(p + OOTP27_PLAYER_ID_OFFSET);
            parent_team = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            active_team = *(uint32_t*)(p + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
            cur_league  = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
            loan_team   = *(uint32_t*)(p + OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
            loan_league = *(uint32_t*)(p + OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
            days_left   = kbo_military_days_left(p);
            restricted  = p[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
            secondary   = p[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
            inj_active  = p[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
            mil_active  = p[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET];
        }
        append_logf(
            "KBO military service entry wrapper #%ld original=%p player=%p plausible=%d"
            " player_id=%u parent_team=%u active_team=%u cur_league=%u"
            " loan_team=%u loan_league=%u days_left=%d"
            " restricted=%u secondary=%u inj_active=%u mil_active=%u",
            log_index,
            (void*)original_func_ptr, (void*)player_ptr,
            kbo_player_pointer_plausible(player_ptr),
            player_id, parent_team, active_team, cur_league,
            loan_team, loan_league, days_left,
            (uint32_t)restricted, (uint32_t)secondary,
            (uint32_t)inj_active, (uint32_t)mil_active);
    }

    if (kbo_player_pointer_plausible(player_ptr)) {
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0
                && kbo_military_days_left(player) > 0) {
            uint32_t cur_year = 0;
            uint32_t cur_month = 0;
            uint32_t cur_day   = 0;
            if (!kbo_current_date_is_valid(&cur_year, &cur_month, &cur_day)) {
                kbo_current_year_relaxed(&cur_year);
            }
            append_logf(
                "KBO military service entry deferred player=%p player_id=%u"
                " year=%u date=%04u-%02u-%02u days_left=%d",
                (void*)player_ptr,
                *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
                cur_year, cur_year, cur_month, cur_day,
                kbo_military_days_left(player));
        }
    }
}

__declspec(noinline) void ootp_kbo_military_status_update_wrapper(
    uintptr_t player_ptr,
    uintptr_t original_func_ptr)
{
    OotpMilitaryServiceEntryFn original_func = (OotpMilitaryServiceEntryFn)original_func_ptr;
    if (original_func != NULL) {
        original_func((void*)player_ptr);
    }

    kbo_flush_pending_foreign_priority_events("military_status_update_wrapper");

    if (!kbo_fix_enabled() || !kbo_player_is_registered_active_military_loan(player_ptr)) {
        return;
    }

    return_completed_kbo_military_loan_player(
        (uint8_t*)player_ptr, "military_status_update_wrapper", 0, 1);
}
