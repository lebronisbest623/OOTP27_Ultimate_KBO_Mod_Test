static void kbo_classify_fa_market_row(
    KboFaMarketClassification* row,
    const KboFaMarketSeedCase* seeds,
    int seed_count,
    const KboFaRequalificationRecord* records,
    int record_count,
    const KboFaMarketHistoryCase* history_case,
    uint32_t current_year,
    uint32_t today_yyyymmdd)
{
    if (row == NULL) {
        return;
    }

    snprintf(row->grade, sizeof(row->grade), "UNKNOWN");
    row->case_label[0] = '\0';
    row->reason[0] = '\0';

    const KboFaMarketSeedCase* seed = kbo_find_fa_market_seed_case(seeds, seed_count, row->player_id);
    if (seed != NULL) {
        const char* seed_case = kbo_fa_market_canonical_case_label(seed->case_label);
        snprintf(row->case_label, sizeof(row->case_label), "%s", seed_case != NULL ? seed_case : seed->case_label);
        snprintf(row->grade, sizeof(row->grade), "%s", seed->grade[0] != '\0' ? seed->grade : "UNKNOWN");
        if (seed->note[0] != '\0') {
            snprintf(row->reason, sizeof(row->reason), "%s", seed->note);
        } else if (kbo_fa_market_case_is_seeded_official(seed_case)) {
            snprintf(row->reason, sizeof(row->reason), "manual official FA seed");
        } else {
            snprintf(row->reason, sizeof(row->reason), "manual case seed");
        }
        return;
    }

    if (row->foreign_player) {
        uint32_t rights_team_id = 0u;
        if (today_yyyymmdd != 0u && kbo_find_active_foreign_waiver_holder(row->player_id, today_yyyymmdd, &rights_team_id)) {
            row->rights_team_id = rights_team_id;
            snprintf(row->case_label, sizeof(row->case_label), "FOREIGN_RESERVED_RIGHT");
            snprintf(row->reason, sizeof(row->reason), "active foreign reserve right held by team #%u", rights_team_id);
            return;
        }
        snprintf(row->case_label, sizeof(row->case_label), "FOREIGN_FREE");
        snprintf(row->reason, sizeof(row->reason), "foreign player with no active KBO reserve right");
        return;
    }

    const KboFaRequalificationRecord* rec =
        kbo_find_fa_market_requalification_record(records, record_count, row->player_id);
    if (rec != NULL) {
        uint32_t eligible_year = rec->last_fa_year + KBO_FA_REQUALIFICATION_YEARS;
        if (row->original_team_id == 0u) {
            row->original_team_id = rec->original_team_id;
        }
        if (current_year != 0u && current_year < eligible_year) {
            snprintf(row->case_label, sizeof(row->case_label), "KBO_REQUALIFICATION_LOCKED");
            snprintf(
                row->reason,
                sizeof(row->reason),
                "last_fa=%u eligible=%u count=%u original_team=%u",
                rec->last_fa_year,
                eligible_year,
                rec->fa_count,
                rec->original_team_id);
            return;
        }
        snprintf(row->case_label, sizeof(row->case_label), "KBO_REQUALIFICATION_ELIGIBLE");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "timer complete last_fa=%u eligible=%u; approval/declaration still needs manual seed",
            rec->last_fa_year,
            eligible_year);
        return;
    }

    if (kbo_fa_market_apply_history_case(row, history_case)) {
        return;
    }

    if (kbo_fa_market_row_is_undrafted_domestic(row)) {
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_UNDRAFTED_FREE_AGENT");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "domestic undrafted free agent marker draft_league=%u subtype=%u gen=%u/%u",
            row->draft_league_id,
            (uint32_t)row->draft_subtype,
            (uint32_t)row->generation_context,
            (uint32_t)row->generation_grade);
        return;
    }

    if (kbo_fa_market_row_is_independent_league_fa(row)) {
        uint32_t original_league_id = kbo_fa_market_get_team_league_id(row->original_team_id);
        snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_INDEPENDENT_LEAGUE_FA");
        snprintf(
            row->reason,
            sizeof(row->reason),
            "domestic independent-league free agent marker original_league=%u original_team=%u draft_league=%u",
            original_league_id,
            row->original_team_id,
            row->draft_league_id);
        return;
    }

    snprintf(row->case_label, sizeof(row->case_label), "DOMESTIC_TEAMLESS_UNVERIFIED");
    snprintf(row->reason, sizeof(row->reason), "domestic active teamless player; release/official FA origin not proven without seed");
}

static int kbo_fa_market_case_rank(const char* case_label)
{
    if (case_label == NULL) {
        return 99;
    }
    if (strcmp(case_label, "KBO_FA_APPROVED") == 0) { return 10; }
    if (strcmp(case_label, "KBO_FA_ELIGIBLE_NOT_APPROVED") == 0) { return 11; }
    if (strcmp(case_label, "KBO_FA_DEFERRED") == 0) { return 12; }
    if (strcmp(case_label, "KBO_FA_BY_HISTORY_UNGRADED") == 0) { return 13; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_LOCKED") == 0) { return 20; }
    if (strcmp(case_label, "KBO_REQUALIFICATION_ELIGIBLE") == 0) { return 21; }
    if (strcmp(case_label, "DOMESTIC_RELEASED_NON_FA") == 0) { return 30; }
    if (strcmp(case_label, "DOMESTIC_UNDRAFTED_FREE_AGENT") == 0) { return 31; }
    if (strcmp(case_label, "DOMESTIC_INDEPENDENT_LEAGUE_FA") == 0) { return 32; }
    if (strcmp(case_label, "DOMESTIC_TEAMLESS_UNVERIFIED") == 0) { return 33; }
    if (strcmp(case_label, "DOMESTIC_MARKET_UNVERIFIED") == 0) { return 33; }
    if (strcmp(case_label, "FOREIGN_RESERVED_RIGHT") == 0) { return 40; }
    if (strcmp(case_label, "FOREIGN_FREE") == 0) { return 41; }
    return 99;
}

static int kbo_compare_fa_market_classification_rows(const void* lhs, const void* rhs)
{
    const KboFaMarketClassification* a = (const KboFaMarketClassification*)lhs;
    const KboFaMarketClassification* b = (const KboFaMarketClassification*)rhs;
    int rank_a = kbo_fa_market_case_rank(a->case_label);
    int rank_b = kbo_fa_market_case_rank(b->case_label);
    if (rank_a != rank_b) {
        return rank_a - rank_b;
    }
    int name_cmp = _stricmp(a->player_name, b->player_name);
    if (name_cmp != 0) {
        return name_cmp;
    }
    if (a->player_id < b->player_id) { return -1; }
    if (a->player_id > b->player_id) { return 1; }
    return 0;
}
