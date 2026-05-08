static int kbo_collect_fa_market_classifications(
    uint32_t requested_league_id,
    KboFaMarketClassification* rows,
    int max_rows,
    KboFaMarketScanSummary* summary,
    int write_csv,
    const char* source)
{
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));
    if (summary != NULL) {
        memset(summary, 0, sizeof(*summary));
    }

    uint32_t league_id = kbo_fa_market_resolve_league_id(requested_league_id);
    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);
    uint32_t current_year = 0u;
    kbo_current_year_relaxed(&current_year);

    if (summary != NULL) {
        summary->league_id = league_id;
        summary->today_yyyymmdd = today;
        summary->current_year = current_year;
        kbo_get_fa_market_classification_csv_path(summary->csv_path, sizeof(summary->csv_path));
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        append_log_line("FA market classification: no player vector");
        return 0;
    }

    KboFaMarketSeedCase* seeds = (KboFaMarketSeedCase*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_MARKET_SEED_MAX * sizeof(KboFaMarketSeedCase));
    KboFaRequalificationRecord* records = (KboFaRequalificationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_REQUALIFICATION_MAX * sizeof(KboFaRequalificationRecord));
    KboFaSalarySnapshotGrade* salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (seeds == NULL || records == NULL || salary_grades == NULL) {
        if (seeds != NULL) {
            HeapFree(GetProcessHeap(), 0, seeds);
        }
        if (records != NULL) {
            HeapFree(GetProcessHeap(), 0, records);
        }
        if (salary_grades != NULL) {
            HeapFree(GetProcessHeap(), 0, salary_grades);
        }
        append_log_line("FA market classification: allocation failed");
        return 0;
    }

    char seed_path[MAX_PATH] = {0};
    char salary_snapshot_path[MAX_PATH] = {0};
    int seed_count = kbo_load_fa_market_seed_cases(seeds, KBO_FA_MARKET_SEED_MAX, seed_path, sizeof(seed_path));
    int requalification_count = kbo_load_fa_requalification_records(records, KBO_FA_REQUALIFICATION_MAX);
    int salary_grade_count = kbo_fa_salary_snapshot_load_grade_rows(
        current_year,
        salary_grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        salary_snapshot_path,
        sizeof(salary_snapshot_path));
    KboFaRules fa_rules;
    kbo_fa_rules_load(&fa_rules);
    if (summary != NULL) {
        summary->seed_count = seed_count;
        summary->requalification_count = requalification_count;
        summary->salary_snapshot_count = salary_grade_count;
        snprintf(summary->seed_path, sizeof(summary->seed_path), "%s", seed_path);
        snprintf(summary->salary_snapshot_path, sizeof(summary->salary_snapshot_path), "%s", salary_snapshot_path);
    }

    int row_count = 0;
    int candidate_count = 0;
    int scanned = 0;
    int truncated = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        scanned++;
        if (!kbo_fa_market_player_is_candidate(player, league_id)) {
            continue;
        }
        candidate_count++;
        if (row_count >= max_rows) {
            truncated = 1;
            continue;
        }

        KboFaMarketClassification* row = &rows[row_count];
        row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        row->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        row->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        row->original_team_id = kbo_get_player_original_team_id(player);
        row->current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        row->draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
        row->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        row->retired_flag = player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET];
        row->contract_level = player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET];
        row->dfa = player[OOTP27_PLAYER_DFA_FLAG_OFFSET];
        row->fa_demand = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
        row->foreign_player = kbo_player_is_foreign_for_kbo_rights(player) ? 1u : 0u;
        row->draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
        row->draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
        row->draft_eligible = player[OOTP27_PLAYER_DRAFT_ELIGIBLE_OFFSET];
        row->draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
        row->generation_flags = player[OOTP27_PLAYER_GENERATION_FLAGS_OFFSET];
        row->generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
        row->generation_grade = player[OOTP27_PLAYER_GENERATION_GRADE_OFFSET];
        row->generation_special = player[OOTP27_PLAYER_GENERATION_SPECIAL_OFFSET];
        kbo_copy_player_display_name(player, row->player_name, sizeof(row->player_name));
        if (row->player_name[0] == '\0' || strcmp(row->player_name, "Unknown player") == 0) {
            snprintf(row->player_name, sizeof(row->player_name), "Player #%u", row->player_id);
        }
        row_count++;
    }

    KboFaMarketHistoryCase* histories = NULL;
    int history_count = 0;
    if (row_count > 0) {
        histories = (KboFaMarketHistoryCase*)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            (SIZE_T)row_count * sizeof(KboFaMarketHistoryCase));
        if (histories != NULL) {
            kbo_load_fa_market_history_cases(rows, row_count, histories, row_count);
            kbo_fa_market_overlay_filing_history_cases(rows, row_count, histories, row_count);
            history_count = row_count;
        } else {
            append_log_line("FA market classification: history allocation failed");
        }
    }

    for (int i = 0; i < row_count; i++) {
        const KboFaMarketHistoryCase* history_case =
            kbo_find_fa_market_history_case(histories, history_count, rows[i].player_id);
        kbo_classify_fa_market_row(
            &rows[i],
            seeds,
            seed_count,
            records,
            requalification_count,
            history_case,
            current_year,
            today);
        kbo_fa_market_apply_salary_snapshot_grade(&rows[i], salary_grades, salary_grade_count, &fa_rules);
    }

    if (row_count > 1) {
        qsort(rows, (size_t)row_count, sizeof(rows[0]), kbo_compare_fa_market_classification_rows);
    }

    if (summary != NULL) {
        summary->scanned = scanned;
        summary->candidates = candidate_count;
        summary->rows = row_count;
        summary->truncated = truncated;
    }

    if (write_csv) {
        kbo_write_fa_market_classification_csv(rows, row_count, summary, source);
        if (summary != NULL) {
            summary->csv_written = 1;
        }
    }

    HeapFree(GetProcessHeap(), 0, seeds);
    HeapFree(GetProcessHeap(), 0, records);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    if (histories != NULL) {
        HeapFree(GetProcessHeap(), 0, histories);
    }
    return row_count;
}
