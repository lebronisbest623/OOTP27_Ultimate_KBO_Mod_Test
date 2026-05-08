typedef void (__fastcall *OotpArbitrationNonTenderFn)(void* team, void* player, uint8_t notify);
typedef uint8_t* (__fastcall *KboArbitrationLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);

static int32_t kbo_salary_arbitration_resolve_minimum_salary(uint32_t league_id)
{
    if (league_id == 0u || league_id > 100000u) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    uintptr_t global_db = get_ootp_global_database();
    HMODULE exe = GetModuleHandleA(NULL);
    if (global_db == 0 || exe == NULL) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    KboArbitrationLeagueFinancialsLookupFn lookup =
        (KboArbitrationLeagueFinancialsLookupFn)((uintptr_t)exe + OOTP27_LEAGUE_FINANCIALS_LOOKUP_RVA);
    if (!memory_range_readable((void*)lookup, 16)) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    uint8_t* financials = lookup((void*)global_db, (int32_t)league_id);
    if (financials == NULL
            || !memory_range_readable(financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET, sizeof(int32_t))) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }

    int32_t minimum_salary = *(int32_t*)(financials + OOTP27_LEAGUE_MINIMUM_SALARY_OFFSET);
    if (minimum_salary <= 0 || minimum_salary > 1000000000) {
        int32_t current_floor = kbo_no_minor_resolve_current_league_minimum_salary();
        return current_floor > 0 ? current_floor : 35600;
    }
    return minimum_salary;
}

static uintptr_t kbo_salary_arbitration_caller_rva(void* return_address)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL || return_address == NULL) {
        return 0;
    }
    return (uintptr_t)return_address - (uintptr_t)exe;
}

static int kbo_salary_arbitration_is_known_non_tender_return(uintptr_t caller_rva)
{
    return caller_rva == 0x0067C0F1u
        || caller_rva == 0x00681B6Bu
        || caller_rva == 0x006820C6u;
}

__declspec(noinline) void ootp_kbo_salary_arbitration_non_tender_wrapper(
    void* team_ptr,
    void* player_ptr,
    uint8_t notify,
    OotpArbitrationNonTenderFn original_func)
{
    if (player_ptr == NULL || !memory_range_readable(player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        return;
    }

    void* return_address = __builtin_return_address(0);
    uintptr_t caller_rva = kbo_salary_arbitration_caller_rva(return_address);
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t pre_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t pre_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t player_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t player_draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    uint32_t team_league_id = 0u;
    uint32_t team_id = 0u;
    if (team_ptr != NULL
            && memory_range_readable(
                (uint8_t*)team_ptr + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET,
                OOTP27_KBO_TEAM_ID_OFFSET - OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET + sizeof(uint32_t))) {
        uint8_t* team = (uint8_t*)team_ptr;
        team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    }

    int kbo_context =
        team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_league_id == OOTP27_KBO_MAIN_LEAGUE_ID
        || player_draft_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
    if (!kbo_context) {
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        return;
    }

    uint8_t contract_level = *(uint8_t*)(player + OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET);
    int32_t old_offer = *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET);
    uint32_t original_team_id = team_id != 0u
        ? team_id
        : (pre_active_team_id != 0u ? pre_active_team_id : pre_current_team_id);
    uint32_t original_team_league_id = team_league_id != 0u
        ? team_league_id
        : kbo_fa_filing_team_league_id(original_team_id);
    int fa_filing_candidate =
        kbo_fa_filing_is_official_transition_caller(caller_rva)
        && notify == 0u
        && contract_level != 0u
        && nation_id == OOTP27_KBO_KOREA_NATION_ID
        && original_team_id != 0u
        && original_team_league_id == OOTP27_KBO_MAIN_LEAGUE_ID;
    int direct_block_candidate = contract_level != 0u
        && old_offer <= 0
        && kbo_salary_arbitration_is_known_non_tender_return(caller_rva);
    int should_block = 0;
    if (!should_block) {
        static LONG pass_log_count = 0;
        LONG pass_slot = InterlockedIncrement(&pass_log_count);
        if (pass_slot <= 120) {
            append_logf(
                "KBO salary arbitration non-tender pass-through player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u offer=%d notify=%u caller_rva=0x%llx direct_block_candidate=%d",
                player_id,
                team_id,
                team_league_id,
                player_league_id,
                player_draft_league_id,
                (unsigned)contract_level,
                old_offer,
                (unsigned)notify,
                (unsigned long long)caller_rva,
                direct_block_candidate ? 1 : 0);
        }
        if (original_func != NULL) {
            original_func(team_ptr, player_ptr, notify);
        }
        if (fa_filing_candidate && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
            uint32_t post_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
            if (post_current_team_id == 0u) {
                uint32_t today = 0u;
                if (kbo_get_current_yyyymmdd(&today) && today != 0u) {
                    kbo_record_fa_filing_transition(
                        (uintptr_t)player_ptr,
                        player_id,
                        today,
                        original_team_id,
                        original_team_league_id,
                        (uint32_t)caller_rva,
                        notify,
                        contract_level,
                        "arbitration_non_tender_67d11e");
                }
            }
        }
        return;
    }

    uint32_t floor_league_id = team_league_id != 0u
        ? team_league_id
        : (player_league_id != 0u ? player_league_id : player_draft_league_id);
    int32_t salary_floor = kbo_salary_arbitration_resolve_minimum_salary(floor_league_id);
    if (old_offer < salary_floor) {
        *(int32_t*)(player + OOTP27_PLAYER_ARBITRATION_OFFER_OFFSET) = salary_floor;
    }

    static LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 120) {
        append_logf(
            "KBO salary arbitration non-tender blocked player=%u team=%u team_league=%u player_league=%u draft_league=%u contract_level=%u old_offer=%d floor=%d notify=%u caller_rva=0x%llx",
            player_id,
            team_id,
            team_league_id,
            player_league_id,
            player_draft_league_id,
            (unsigned)contract_level,
            old_offer,
            salary_floor,
            (unsigned)notify,
            (unsigned long long)caller_rva);
    }
}

