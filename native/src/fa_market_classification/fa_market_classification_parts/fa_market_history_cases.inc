static void kbo_fa_market_mark_history_case(KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_text[0] == '\0') {
        return;
    }
    history->became_free_agent = strcmp(history->history_text, "[G]Became a free agent.") == 0;
    history->undrafted_free_agent =
        strstr(history->history_text, "Was not drafted and became a free agent") != NULL;
    history->released = strncmp(history->history_text, "[G]Released by ", 15) == 0;
}

static int kbo_load_fa_market_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int max_histories)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || max_histories < row_count) {
        return 0;
    }
    memset(histories, 0, (SIZE_T)max_histories * sizeof(histories[0]));
    for (int i = 0; i < row_count; i++) {
        histories[i].player_id = rows[i].player_id;
    }

    char save_path[MAX_PATH] = {0};
    KboFaMarketFileSignature db_sig = {0};
    KboFaMarketFileSignature wal_sig = {0};
    KboFaMarketFileSignature shm_sig = {0};
    int have_signature = 0;
    if (kbo_get_current_save_path(save_path, sizeof(save_path))) {
        have_signature = kbo_fa_market_get_text_data_signatures(save_path, &db_sig, &wal_sig, &shm_sig);
        if (have_signature && kbo_fa_market_history_cache_matches(save_path, &db_sig, &wal_sig, &shm_sig)) {
            int cached_found = kbo_fa_market_copy_history_cache_for_rows(rows, row_count, histories, max_histories);
            append_logf("FA market history sqlite cache hit found=%d rows=%d save=%s", cached_found, row_count, save_path);
            return cached_found;
        }
    }

    KboFaMarketSqliteApi* api = kbo_fa_market_get_sqlite_api();
    if (api == NULL) {
        return 0;
    }

    char db_path[MAX_PATH] = {0};
    if (!kbo_fa_market_copy_text_data_sqlite(db_path, sizeof(db_path))) {
        return 0;
    }

    KboFaMarketSqlite3* db = NULL;
    int open_result = api->open_v2(db_path, &db, KBO_SQLITE_OPEN_READONLY, NULL);
    if (open_result != KBO_SQLITE_OK || db == NULL) {
        append_logf(
            "FA market history sqlite open failed result=%d path=%s msg=%s",
            open_result,
            db_path,
            (db != NULL && api->errmsg != NULL) ? api->errmsg(db) : "");
        if (db != NULL) {
            api->close(db);
        }
        return 0;
    }

    const char* sql =
        "SELECT history_date, history_text "
        "FROM player_history "
        "WHERE player_id=?1 AND ("
        "history_text='[G]Became a free agent.' "
        "OR history_text LIKE '[G]Was not drafted and became a free agent%' "
        "OR history_text LIKE '[G]Released by %') "
        "ORDER BY history_date DESC, history_id DESC LIMIT 1;";

    KboFaMarketSqlite3Stmt* stmt = NULL;
    int prepare_result = api->prepare_v2(db, sql, -1, &stmt, NULL);
    if (prepare_result != KBO_SQLITE_OK || stmt == NULL) {
        append_logf(
            "FA market history sqlite prepare failed result=%d msg=%s",
            prepare_result,
            api->errmsg != NULL ? api->errmsg(db) : "");
        api->close(db);
        return 0;
    }

    int found = 0;
    for (int i = 0; i < row_count; i++) {
        api->reset(stmt);
        api->clear_bindings(stmt);
        api->bind_int(stmt, 1, (int)rows[i].player_id);
        int step_result = api->step(stmt);
        if (step_result == KBO_SQLITE_ROW) {
            histories[i].found = 1;
            kbo_fa_market_copy_sqlite_text_column(api, stmt, 0, histories[i].history_date, sizeof(histories[i].history_date));
            kbo_fa_market_copy_sqlite_text_column(api, stmt, 1, histories[i].history_text, sizeof(histories[i].history_text));
            kbo_fa_market_mark_history_case(&histories[i]);
            found++;
        } else if (step_result != KBO_SQLITE_DONE) {
            append_logf(
                "FA market history sqlite step failed player=%u result=%d msg=%s",
                rows[i].player_id,
                step_result,
                api->errmsg != NULL ? api->errmsg(db) : "");
        }
    }

    api->finalize(stmt);
    api->close(db);
    append_logf("FA market history sqlite loaded found=%d rows=%d db=%s", found, row_count, db_path);
    if (have_signature) {
        kbo_fa_market_store_history_cache(save_path, &db_sig, &wal_sig, &shm_sig, histories, row_count);
    }
    return found;
}

static const KboFaMarketHistoryCase* kbo_find_fa_market_history_case(
    const KboFaMarketHistoryCase* histories,
    int history_count,
    uint32_t player_id)
{
    if (histories == NULL || history_count <= 0 || player_id == 0u) {
        return NULL;
    }
    for (int i = 0; i < history_count; i++) {
        if (histories[i].player_id == player_id && histories[i].found) {
            return &histories[i];
        }
    }
    return NULL;
}

static uint32_t kbo_fa_market_history_date_u32(const KboFaMarketHistoryCase* history)
{
    if (history == NULL || history->history_date[0] == '\0') {
        return 0u;
    }
    uint32_t date = 0u;
    if (kbo_parse_yyyymmdd(history->history_date, &date)) {
        return date;
    }
    return kbo_fa_filing_parse_u32(history->history_date);
}

static int kbo_fa_market_overlay_filing_history_cases(
    KboFaMarketClassification* rows,
    int row_count,
    KboFaMarketHistoryCase* histories,
    int history_count)
{
    if (rows == NULL || row_count <= 0 || histories == NULL || history_count < row_count) {
        return 0;
    }

    KboFaFilingRecord* filings = (KboFaFilingRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_FILING_MAX * sizeof(KboFaFilingRecord));
    if (filings == NULL) {
        append_log_line("FA market filing ledger overlay skipped: allocation failed");
        return 0;
    }

    char path[MAX_PATH] = {0};
    int filing_count = kbo_load_fa_filing_records(filings, KBO_FA_FILING_MAX, path, sizeof(path));
    int applied = 0;
    for (int f = 0; f < filing_count; f++) {
        const KboFaFilingRecord* filing = &filings[f];
        if (filing->player_id == 0u || filing->filing_date == 0u) {
            continue;
        }
        for (int i = 0; i < row_count; i++) {
            if (rows[i].player_id != filing->player_id) {
                continue;
            }
            uint32_t existing_date = kbo_fa_market_history_date_u32(&histories[i]);
            if (histories[i].found && existing_date > filing->filing_date) {
                break;
            }

            histories[i].player_id = rows[i].player_id;
            histories[i].found = 1;
            histories[i].became_free_agent = 1;
            histories[i].undrafted_free_agent = 0;
            histories[i].released = 0;
            snprintf(histories[i].history_date, sizeof(histories[i].history_date), "%u", filing->filing_date);
            snprintf(histories[i].history_text, sizeof(histories[i].history_text), "[G]Became a free agent.");
            if (rows[i].original_team_id == 0u && filing->original_team_id != 0u) {
                rows[i].original_team_id = filing->original_team_id;
            }
            applied++;
            break;
        }
    }

    if (filing_count > 0 || applied > 0) {
        append_logf(
            "FA market filing ledger overlay applied=%d filings=%d rows=%d csv=%s",
            applied,
            filing_count,
            row_count,
            path);
    }
    HeapFree(GetProcessHeap(), 0, filings);
    return applied;
}
