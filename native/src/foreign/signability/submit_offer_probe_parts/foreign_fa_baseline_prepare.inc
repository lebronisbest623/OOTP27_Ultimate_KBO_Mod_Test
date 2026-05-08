__declspec(noinline) void ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper(
    uintptr_t financials_ptr,
    uintptr_t player_ptr,
    uint32_t source_rva)
{
    kbo_restore_foreign_fa_demand_salary_ladder("prepare_enter");

    if (financials_ptr == 0
            || !memory_range_readable(
                (void*)(financials_ptr + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET),
                sizeof(int32_t))) {
        return;
    }
    if (!kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }
    int asian_quota = kbo_player_is_asian_quota_candidate(player);

    uint8_t* financials = (uint8_t*)financials_ptr;
    for (int i = 0; i < 9; i++) {
        g_kbo_foreign_fa_demand_ladder_snapshot.values[i] =
            *(int32_t*)(financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i]);
    }
    g_kbo_foreign_fa_demand_ladder_snapshot.financials = financials;

    int patched = 0;
    for (int i = 0; i < 9; i++) {
        patched += kbo_write_i32(
            financials + KBO_FINANCIALS_SALARY_LADDER_OFFSETS[i],
            kbo_get_foreign_fa_demand_baseline_value_for_player(i, asian_quota));
    }

    if (patched == 9) {
        InterlockedExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 1);
    } else {
        kbo_restore_foreign_fa_demand_salary_ladder("prepare_failed");
    }

    static LONG prepare_log_count = 0;
    LONG slot = InterlockedIncrement(&prepare_log_count);
    if (slot <= 120) {
        append_logf(
            "KBO foreign FA demand baseline prepared source=0x%x player=%u asian_quota=%d financials=%p patched=%d original_superstar=%d foreign_superstar=%d",
            source_rva,
            memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                : 0u,
            asian_quota,
            (void*)financials,
            patched,
            g_kbo_foreign_fa_demand_ladder_snapshot.values[8],
            kbo_get_foreign_fa_demand_baseline_value_for_player(8, asian_quota));
    }
}

static void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source)
{
    if (player_ptr == 0 || !kbo_player_pointer_plausible(player_ptr)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_current_league_financials(&league_id);
    if (financials == NULL) {
        static LONG no_financials_log_count = 0;
        LONG slot = InterlockedIncrement(&no_financials_log_count);
        if (slot <= 40) {
            append_logf(
                "KBO foreign FA demand baseline offer-build skipped source=%s reason=no_financials player=%u",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u);
        }
        return;
    }

    uint32_t source_rva = 0u;
    if (source != NULL && strstr(source, "17B50B4") != NULL) {
        source_rva = OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17B50B4_RVA;
    } else if (source != NULL && strstr(source, "17A79BB") != NULL) {
        source_rva = OOTP27_NO_MINOR_CONTRACT_FA_OFFER_DEMAND_FLOOR_17A79BB_RVA;
    }

    ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper((uintptr_t)financials, player_ptr, source_rva);
    if (InterlockedCompareExchange(&g_kbo_foreign_fa_demand_ladder_snapshot.active, 0, 0) != 0) {
        kbo_schedule_foreign_fa_demand_restore_timer();
        static LONG offer_prepare_log_count = 0;
        LONG slot = InterlockedIncrement(&offer_prepare_log_count);
        if (slot <= 120) {
            append_logf(
                "KBO foreign FA demand baseline offer-build active source=%s player=%u league=%u financials=%p",
                source != NULL ? source : "",
                memory_range_readable(player + OOTP27_PLAYER_ID_OFFSET, sizeof(uint32_t))
                    ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET)
                    : 0u,
                league_id,
                (void*)financials);
        }
    }
}

static uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id)
{
    if (out_league_id != NULL) {
        *out_league_id = 0u;
    }

    int32_t league_id = kbo_no_minor_resolve_current_league_id();
    if (league_id <= 0) {
        return NULL;
    }

    uintptr_t global_db = get_ootp_global_database();
    if (global_db == 0) {
        return NULL;
    }
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }

    OotpLeagueFinancialsLookupFn lookup =
        (OotpLeagueFinancialsLookupFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        return NULL;
    }

    uint8_t* financials = lookup((void*)global_db, league_id);
    if (financials == NULL
            || !memory_range_readable(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET, sizeof(int32_t))) {
        return NULL;
    }

    if (out_league_id != NULL) {
        *out_league_id = (uint32_t)league_id;
    }
    return financials;
}

static void kbo_log_financials_salary_baseline_probe(const char* source)
{
#define KBO_FINANCIALS_PROBE_LOW_OFFSET(index) \
    (OOTP27_FINANCIALS_PROBE_FIRST_OFFSET + ((uint32_t)(index) * sizeof(int32_t)))
#define KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(index) \
    (OOTP27_FINANCIALS_PROBE_SECONDARY_FIRST_OFFSET + ((uint32_t)(index) * sizeof(int32_t)))

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 80 && !read_kbo_localappdata_flag_file("enable_kbo_financials_salary_baseline_probe_log.txt")) {
        return;
    }

    uint32_t league_id = 0u;
    uint8_t* financials = kbo_resolve_current_league_financials(&league_id);
    if (financials == NULL) {
        if (slot <= 20) {
            append_logf("KBO financials salary baseline probe skipped source=%s reason=no_financials", source != NULL ? source : "");
        }
        return;
    }

    append_logf(
        "KBO financials salary baseline probe source=%s league=%u financials=%p off278=%d off27c=%d off280=%d off284=%d off288=%d off28c=%d off290=%d off294=%d off298=%d off29c=%d off2a0=%d off2a4=%d off2a8=%d off2ac=%d off360=%d off364=%d off368=%d off36c=%d off370=%d off374=%d off378=%d off37c=%d off380=%d off384=%d off388=%d off38c=%d off390=%d",
        source != NULL ? source : "",
        league_id,
        (void*)financials,
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(0)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(1)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(2)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(3)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(4)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(5)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(6)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(7)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(8)),
        *(int32_t*)(financials + OOTP27_FINANCIALS_AVERAGE_SALARY_OFFSET),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(10)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(11)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(12)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_LOW_OFFSET(13)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(0)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(1)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(2)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(3)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(4)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(5)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(6)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(7)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(8)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(9)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(10)),
        *(int32_t*)(financials + KBO_FINANCIALS_PROBE_SECONDARY_OFFSET(11)),
        *(int32_t*)(financials + OOTP27_FINANCIALS_PROBE_LAST_OFFSET));

#undef KBO_FINANCIALS_PROBE_LOW_OFFSET
#undef KBO_FINANCIALS_PROBE_SECONDARY_OFFSET
}

