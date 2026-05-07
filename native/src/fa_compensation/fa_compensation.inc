/* KBO FA compensation obligation ledger. */

#define KBO_FA_COMPENSATION_MAX 1024
#define KBO_FA_COMPENSATION_STATUS_PENDING 0u
#define KBO_FA_COMPENSATION_STATUS_RECORDED 1u

typedef struct KboFaCompensationRecord {
    uint32_t player_id;
    uint32_t signed_on_yyyymmdd;
    uint32_t season;
    uint32_t league_id;
    uint32_t original_team_id;
    uint32_t signing_team_id;
    char grade[12];
    int32_t previous_salary;
    uint32_t cash_with_player;
    uint32_t cash_only;
    uint32_t protect_count;
    uint8_t requires_player_compensation;
    uint8_t status;
    char case_label[48];
    char player_name[96];
    char source[48];
} KboFaCompensationRecord;

static int kbo_get_fa_compensation_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation.csv", out, out_size);
}

static uint32_t kbo_fa_compensation_parse_u32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0u;
    }
    char* end = NULL;
    unsigned long value = strtoul(text, &end, 10);
    return end != text ? (uint32_t)value : 0u;
}

static int32_t kbo_fa_compensation_parse_i32(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    char* end = NULL;
    long value = strtol(text, &end, 10);
    return end != text ? (int32_t)value : 0;
}

static void kbo_fa_compensation_copy_token(const char* value, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (value == NULL) {
        return;
    }
    size_t len = strlen(value);
    if (len >= out_size) {
        len = out_size - 1u;
    }
    if (len > 0u) {
        memcpy(out, value, len);
    }
    out[len] = '\0';
}

static int kbo_load_fa_compensation_records(
    KboFaCompensationRecord* records,
    int max_records,
    char* out_path,
    size_t out_path_size)
{
    if (out_path != NULL && out_path_size > 0) {
        out_path[0] = '\0';
    }
    if (records == NULL || max_records <= 0) {
        return 0;
    }
    memset(records, 0, (SIZE_T)max_records * sizeof(records[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
        return 0;
    }
    if (out_path != NULL && out_path_size > 0) {
        snprintf(out_path, out_path_size, "%s", path);
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    if (!ReadFile(file, buffer, size, &read, NULL)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return 0;
    }
    CloseHandle(file);
    buffer[read] = '\0';

    int count = 0;
    char* cursor = buffer;
    while (*cursor != '\0' && count < max_records) {
        char* next = strchr(cursor, '\n');
        if (next != NULL) {
            *next = '\0';
        }

        char* p = cursor;
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            char fields[16][128];
            memset(fields, 0, sizeof(fields));
            int ok = 1;
            for (int field_index = 0; field_index < 16; field_index++) {
                if (!kbo_fa_market_parse_csv_field(&p, fields[field_index], sizeof(fields[field_index]))) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                KboFaCompensationRecord* rec = &records[count];
                rec->player_id = kbo_fa_compensation_parse_u32(fields[0]);
                rec->signed_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[1]);
                rec->season = kbo_fa_compensation_parse_u32(fields[2]);
                rec->league_id = kbo_fa_compensation_parse_u32(fields[3]);
                rec->original_team_id = kbo_fa_compensation_parse_u32(fields[4]);
                rec->signing_team_id = kbo_fa_compensation_parse_u32(fields[5]);
                kbo_fa_compensation_copy_token(fields[6], rec->grade, sizeof(rec->grade));
                rec->previous_salary = kbo_fa_compensation_parse_i32(fields[7]);
                rec->cash_with_player = kbo_fa_compensation_parse_u32(fields[8]);
                rec->cash_only = kbo_fa_compensation_parse_u32(fields[9]);
                rec->protect_count = kbo_fa_compensation_parse_u32(fields[10]);
                rec->requires_player_compensation = kbo_fa_compensation_parse_u32(fields[11]) != 0u ? 1u : 0u;
                rec->status = (uint8_t)kbo_fa_compensation_parse_u32(fields[12]);
                kbo_fa_compensation_copy_token(fields[13], rec->case_label, sizeof(rec->case_label));
                kbo_fa_compensation_copy_token(fields[14], rec->player_name, sizeof(rec->player_name));
                kbo_fa_compensation_copy_token(fields[15], rec->source, sizeof(rec->source));
                if (rec->player_id != 0u && rec->season >= 1982u && rec->season <= 2300u) {
                    count++;
                } else {
                    memset(rec, 0, sizeof(*rec));
                }
            }
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return count;
}

static int kbo_persist_fa_compensation_records(const KboFaCompensationRecord* records, int record_count)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_path(path, sizeof(path))) {
        append_log_line("KBO FA compensation persist skipped reason=path_unavailable");
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    char tmp_path[MAX_PATH] = {0};
    HANDLE file = kbo_atomic_open_tmp(path, tmp_path, sizeof(tmp_path));
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA compensation persist failed reason=create_tmp gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    const char* header =
        "player_id,signed_on,season,league_id,original_team_id,signing_team_id,grade,"
        "previous_salary,cash_with_player,cash_only,protect_count,requires_player_compensation,"
        "status,kbo_case,player_name,source\r\n";
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    for (int i = 0; i < record_count; i++) {
        const KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id == 0u || rec->season == 0u) {
            continue;
        }
        char prefix[192] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%u,%u,%u,%u,",
            rec->player_id,
            rec->signed_on_yyyymmdd,
            rec->season,
            rec->league_id,
            rec->original_team_id,
            rec->signing_team_id);
        if (len > 0 && len < (int)sizeof(prefix)) {
            WriteFile(file, prefix, (DWORD)len, &written, NULL);
        }
        kbo_fa_market_write_csv_text(file, rec->grade);

        char middle[192] = {0};
        len = snprintf(
            middle,
            sizeof(middle),
            ",%d,%u,%u,%u,%u,%u,",
            rec->previous_salary,
            rec->cash_with_player,
            rec->cash_only,
            rec->protect_count,
            (uint32_t)rec->requires_player_compensation,
            (uint32_t)rec->status);
        if (len > 0 && len < (int)sizeof(middle)) {
            WriteFile(file, middle, (DWORD)len, &written, NULL);
        }
        kbo_fa_market_write_csv_text(file, rec->case_label);
        WriteFile(file, ",", 1, &written, NULL);
        kbo_fa_market_write_csv_text(file, rec->player_name);
        WriteFile(file, ",", 1, &written, NULL);
        kbo_fa_market_write_csv_text(file, rec->source);
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    int ok = kbo_atomic_commit(file, tmp_path, path);
    if (!ok) {
        append_logf("KBO FA compensation atomic commit failed path=%s", path);
        return 0;
    }
    append_logf("KBO FA compensation persisted records=%d path=%s", record_count, path);
    return 1;
}

static int kbo_fa_compensation_find_existing(
    const KboFaCompensationRecord* records,
    int record_count,
    uint32_t player_id,
    uint32_t season,
    uint32_t original_team_id,
    uint32_t signing_team_id)
{
    if (records == NULL || player_id == 0u || season == 0u) {
        return -1;
    }
    for (int i = 0; i < record_count; i++) {
        const KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id == player_id
                && rec->season == season
                && rec->original_team_id == original_team_id
                && rec->signing_team_id == signing_team_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_fa_compensation_build_market_row(
    uint8_t* player,
    KboFaMarketClassification* row,
    uint32_t league_id,
    uint32_t current_year,
    uint32_t today,
    const KboFaRules* rules)
{
    if (player == NULL || row == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    memset(row, 0, sizeof(*row));
    row->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    row->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    row->current_team_id = 0u;
    row->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    row->original_team_id = kbo_get_player_original_team_id(player);
    row->current_league_id = league_id;
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

    KboFaMarketSeedCase* seeds = (KboFaMarketSeedCase*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_MARKET_SEED_MAX * sizeof(KboFaMarketSeedCase));
    KboFaRequalificationRecord* requal_records = (KboFaRequalificationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_REQUALIFICATION_MAX * sizeof(KboFaRequalificationRecord));
    KboFaSalarySnapshotGrade* salary_grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (seeds == NULL || requal_records == NULL || salary_grades == NULL) {
        if (seeds != NULL) { HeapFree(GetProcessHeap(), 0, seeds); }
        if (requal_records != NULL) { HeapFree(GetProcessHeap(), 0, requal_records); }
        if (salary_grades != NULL) { HeapFree(GetProcessHeap(), 0, salary_grades); }
        return 0;
    }

    char ignored_path[MAX_PATH] = {0};
    int seed_count = kbo_load_fa_market_seed_cases(seeds, KBO_FA_MARKET_SEED_MAX, ignored_path, sizeof(ignored_path));
    int requal_count = kbo_load_fa_requalification_records(requal_records, KBO_FA_REQUALIFICATION_MAX);
    int salary_grade_count = kbo_fa_salary_snapshot_load_grade_rows(
        current_year,
        salary_grades,
        KBO_FA_SALARY_SNAPSHOT_GRADE_MAX,
        ignored_path,
        sizeof(ignored_path));

    KboFaMarketHistoryCase history;
    memset(&history, 0, sizeof(history));
    KboFaMarketClassification history_row = *row;
    kbo_load_fa_market_history_cases(&history_row, 1, &history, 1);

    kbo_classify_fa_market_row(
        row,
        seeds,
        seed_count,
        requal_records,
        requal_count,
        &history,
        current_year,
        today);
    kbo_fa_market_apply_salary_snapshot_grade(row, salary_grades, salary_grade_count, rules);

    if (row->original_team_id == 0u && history_row.original_team_id != 0u) {
        row->original_team_id = history_row.original_team_id;
    }
    if (row->original_team_id == 0u) {
        row->original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    }
    HeapFree(GetProcessHeap(), 0, seeds);
    HeapFree(GetProcessHeap(), 0, requal_records);
    HeapFree(GetProcessHeap(), 0, salary_grades);
    return 1;
}

static void kbo_fa_compensation_record_history_and_news(const KboFaCompensationRecord* rec)
{
    if (rec == NULL || rec->player_id == 0u || rec->signed_on_yyyymmdd == 0u) {
        return;
    }

    uint32_t year = rec->signed_on_yyyymmdd / 10000u;
    uint32_t month = (rec->signed_on_yyyymmdd / 100u) % 100u;
    uint32_t day = rec->signed_on_yyyymmdd % 100u;
    if (year < 1982u || month == 0u || day == 0u) {
        return;
    }

    char salary_text[32] = "-";
    char cash_with_player_text[32] = "-";
    char cash_only_text[32] = "-";
    kbo_fa_market_format_salary(rec->previous_salary, salary_text, sizeof(salary_text));
    kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player_text, sizeof(cash_with_player_text));
    kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only_text, sizeof(cash_only_text));

    char history_text[512] = {0};
    if (rec->requires_player_compensation) {
        snprintf(
            history_text,
            sizeof(history_text),
            "[G]KBO FA compensation recorded: Grade %s, previous salary %s, %u protected players; compensation is %s plus one player or %s cash.",
            rec->grade,
            salary_text,
            rec->protect_count,
            cash_with_player_text,
            cash_only_text);
    } else {
        snprintf(
            history_text,
            sizeof(history_text),
            "[G]KBO FA compensation recorded: Grade %s, previous salary %s; compensation is %s cash.",
            rec->grade,
            salary_text,
            cash_only_text);
    }
    insert_kbo_player_history_sql(rec->player_id, year, month, day, history_text, "fa_compensation");

    char title[128] = {0};
    snprintf(title, sizeof(title), "KBO FA compensation: %s", rec->player_name[0] != '\0' ? rec->player_name : "player");

    char body[1024] = {0};
    if (rec->requires_player_compensation) {
        snprintf(
            body,
            sizeof(body),
            "<%s:player#%u> signed with <Team #%u:team#%u>, creating a Grade %s FA compensation obligation to <Team #%u:team#%u>.\n\nThe recorded KBO rule package is %s plus one eligible compensation player outside a %u-player protected list, or %s as cash-only compensation.",
            rec->player_name[0] != '\0' ? rec->player_name : "Player",
            rec->player_id,
            rec->signing_team_id,
            rec->signing_team_id,
            rec->grade,
            rec->original_team_id,
            rec->original_team_id,
            cash_with_player_text,
            rec->protect_count,
            cash_only_text);
    } else {
        snprintf(
            body,
            sizeof(body),
            "<%s:player#%u> signed with <Team #%u:team#%u>, creating a Grade %s FA compensation obligation to <Team #%u:team#%u>.\n\nThe recorded KBO rule package is cash-only compensation of %s.",
            rec->player_name[0] != '\0' ? rec->player_name : "Player",
            rec->player_id,
            rec->signing_team_id,
            rec->signing_team_id,
            rec->grade,
            rec->original_team_id,
            rec->original_team_id,
            cash_only_text);
    }
    create_kbo_native_live_news_with_body(year, month, day, rec->league_id, 10u, title, body);
}

static int kbo_record_fa_compensation_signing(
    uintptr_t player_ptr,
    uint32_t signing_team_id,
    uint32_t league_id,
    const char* source)
{
    if (!kbo_fix_enabled()) {
        return 0;
    }
    if (read_kbo_localappdata_flag_file("disable_kbo_fa_compensation.txt")) {
        return 0;
    }
    if (!kbo_player_pointer_plausible(player_ptr) || signing_team_id == 0u || league_id == 0u) {
        return 0;
    }

    KboFaRules fa_rules;
    kbo_fa_rules_load(&fa_rules);

    uint8_t* player = (uint8_t*)player_ptr;
    if (fa_rules.exclude_foreign_players && kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t signing_year = kbo_find_league_year_from_id(league_id);
    if (signing_year < 1982u || signing_year > 2300u) {
        signing_year = today >= 19820000u ? today / 10000u : 0u;
    }
    if (signing_year < 1982u || signing_year > 2300u) {
        return 0;
    }
    if (today == 0u) {
        today = signing_year * 10000u + 101u;
    }

    KboFaMarketClassification row;
    if (!kbo_fa_compensation_build_market_row(player, &row, league_id, signing_year, today, &fa_rules)) {
        return 0;
    }

    if (!kbo_fa_rules_case_is_compensable(&fa_rules, row.case_label)
            || (fa_rules.exclude_foreign_players && row.foreign_player)
            || row.original_team_id == 0u
            || row.original_team_id == signing_team_id
            || !kbo_fa_rules_grade_is_compensable(&fa_rules, row.grade)
            || row.fa_grade_salary <= 0) {
        static LONG skip_log_count = 0;
        LONG slot = InterlockedIncrement(&skip_log_count);
        if (slot <= 80) {
            append_logf(
                "KBO FA compensation skipped player=%u signing_team=%u original_team=%u case=%s grade=%s salary=%d foreign=%u reason=not_compensable",
                row.player_id,
                signing_team_id,
                row.original_team_id,
                row.case_label,
                row.grade,
                row.fa_grade_salary,
                (uint32_t)row.foreign_player);
        }
        return 0;
    }

    uint32_t cash_with_player = 0u;
    uint32_t cash_only = 0u;
    uint32_t protect_count = 0u;
    uint8_t requires_player = 0u;
    kbo_fa_rules_calculate_compensation(
        &fa_rules,
        row.grade,
        row.fa_grade_salary,
        &cash_with_player,
        &cash_only,
        &protect_count,
        &requires_player);
    if (cash_only == 0u) {
        return 0;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        append_log_line("KBO FA compensation skipped reason=allocation_failed");
        return 0;
    }

    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    if (kbo_fa_compensation_find_existing(
            records,
            record_count,
            row.player_id,
            signing_year,
            row.original_team_id,
            signing_team_id) >= 0) {
        HeapFree(GetProcessHeap(), 0, records);
        return 0;
    }

    if (record_count >= KBO_FA_COMPENSATION_MAX) {
        append_logf("KBO FA compensation skipped reason=ledger_full player=%u path=%s", row.player_id, path);
        HeapFree(GetProcessHeap(), 0, records);
        return 0;
    }

    KboFaCompensationRecord* rec = &records[record_count++];
    memset(rec, 0, sizeof(*rec));
    rec->player_id = row.player_id;
    rec->signed_on_yyyymmdd = today;
    rec->season = signing_year;
    rec->league_id = league_id;
    rec->original_team_id = row.original_team_id;
    rec->signing_team_id = signing_team_id;
    snprintf(rec->grade, sizeof(rec->grade), "%s", row.grade);
    rec->previous_salary = row.fa_grade_salary;
    rec->cash_with_player = cash_with_player;
    rec->cash_only = cash_only;
    rec->protect_count = protect_count;
    rec->requires_player_compensation = requires_player;
    rec->status = KBO_FA_COMPENSATION_STATUS_PENDING;
    snprintf(rec->case_label, sizeof(rec->case_label), "%s", row.case_label);
    snprintf(rec->player_name, sizeof(rec->player_name), "%s", row.player_name);
    snprintf(rec->source, sizeof(rec->source), "%s", source != NULL ? source : "fa_signing");

    int persisted = kbo_persist_fa_compensation_records(records, record_count);
    if (persisted) {
        kbo_fa_compensation_record_history_and_news(rec);
        append_logf(
            "KBO FA compensation recorded player=%u name=%s grade=%s signing_team=%u original_team=%u salary=%d cash_with_player=%u cash_only=%u protect=%u ledger=%s",
            rec->player_id,
            rec->player_name,
            rec->grade,
            rec->signing_team_id,
            rec->original_team_id,
            rec->previous_salary,
            rec->cash_with_player,
            rec->cash_only,
            rec->protect_count,
            path);
    }

    HeapFree(GetProcessHeap(), 0, records);
    return persisted;
}

static const char* kbo_fa_compensation_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FA_COMPENSATION_STATUS_RECORDED:
        return "Recorded";
    case KBO_FA_COMPENSATION_STATUS_PENDING:
    default:
        return "Pending";
    }
}
