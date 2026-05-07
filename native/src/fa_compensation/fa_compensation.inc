/* KBO FA compensation obligation ledger. */

#define KBO_FA_COMPENSATION_MAX 1024
#define KBO_FA_COMPENSATION_STATUS_PENDING 0u
#define KBO_FA_COMPENSATION_STATUS_RECORDED 1u
#define KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED 2u
#define KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED 3u
#define KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED 4u
#define KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED 5u
#define KBO_FA_COMPENSATION_PROTECTED_LIST_MAX 128

static volatile LONG g_kbo_fa_compensation_ledger_lock = 0;

static void kbo_fa_compensation_lock_ledger(const char* source)
{
    DWORD start = GetTickCount();
    while (InterlockedCompareExchange(&g_kbo_fa_compensation_ledger_lock, 1, 0) != 0) {
        if ((GetTickCount() - start) > 5000u) {
            append_logf(
                "KBO FA compensation ledger lock waiting source=%s wait_ms=%lu",
                source != NULL ? source : "",
                (unsigned long)(GetTickCount() - start));
            start = GetTickCount();
        }
        Sleep(1);
    }
}

static void kbo_fa_compensation_unlock_ledger(void)
{
    InterlockedExchange(&g_kbo_fa_compensation_ledger_lock, 0);
}

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

typedef struct KboFaProtectedCandidate {
    uint32_t player_id;
    uint32_t team_id;
    uint16_t age;
    uint8_t role;
    int32_t score;
    char reason[96];
    char player_name[96];
} KboFaProtectedCandidate;

typedef struct KboFaCompensationProtectionDebugRow {
    uint32_t fa_player_id;
    uint32_t season;
    char grade[12];
    uint32_t original_team_id;
    uint32_t signing_team_id;
    uint32_t due_on_yyyymmdd;
    uint32_t generated_on_yyyymmdd;
    char list_type[24];
    uint32_t rank;
    uint32_t player_id;
    char player_name[96];
    int32_t score;
    uint16_t age;
    uint8_t role;
    char reason[96];
    char source[48];
} KboFaCompensationProtectionDebugRow;

typedef struct KboFaCompensationDecisionRow {
    uint32_t fa_player_id;
    uint32_t season;
    char grade[12];
    uint32_t original_team_id;
    uint32_t signing_team_id;
    uint32_t signed_on_yyyymmdd;
    uint32_t due_on_yyyymmdd;
    uint32_t decided_on_yyyymmdd;
    char action[24];
    uint32_t selected_player_id;
    char selected_player_name[96];
    int32_t selected_player_score;
    int unprotected_candidate_count;
    uint32_t cash_with_player;
    uint32_t cash_only;
    char source[48];
} KboFaCompensationDecisionRow;

static int kbo_get_fa_compensation_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation.csv", out, out_size);
}

static int kbo_get_fa_compensation_protected_lists_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation_protected_lists.csv", out, out_size);
}

static int kbo_get_fa_compensation_decisions_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation_decisions.csv", out, out_size);
}

static int kbo_get_fa_compensation_protection_debug_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    return kbo_get_save_scoped_data_file("fa_compensation_protection_debug.csv", out, out_size);
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
    kbo_fa_market_overlay_filing_history_cases(&history_row, 1, &history, 1);

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

static int kbo_fa_protected_candidate_compare_desc(const void* left, const void* right)
{
    const KboFaProtectedCandidate* a = (const KboFaProtectedCandidate*)left;
    const KboFaProtectedCandidate* b = (const KboFaProtectedCandidate*)right;
    if (a->score != b->score) {
        return a->score > b->score ? -1 : 1;
    }
    if (a->age != b->age) {
        return a->age < b->age ? -1 : 1;
    }
    if (a->player_id == b->player_id) {
        return 0;
    }
    return a->player_id < b->player_id ? -1 : 1;
}

static int kbo_fa_role_bucket(uint8_t role);
static int kbo_fa_team_role_count(uint32_t team_id, int role_bucket);

static int32_t kbo_fa_protection_candidate_score(
    uint8_t* player,
    uint32_t signing_team_id,
    char* reason,
    size_t reason_size)
{
    if (reason != NULL && reason_size > 0) {
        reason[0] = '\0';
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return -2147483647;
    }

    int32_t base = kbo_foreign_waiver_value_score(player);
    int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    uint8_t role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
    int role_bucket = kbo_fa_role_bucket(role);
    int team_role_count = kbo_fa_team_role_count(signing_team_id, role_bucket);

    int32_t score = base;
    const char* age_tag = "prime";
    if (age <= 22u) {
        score += 1200;
        age_tag = "prospect";
    } else if (age <= 25u) {
        score += 850;
        age_tag = "young";
    } else if (age <= 29u) {
        score += 350;
        age_tag = "prime";
    } else if (age >= 36u && overall < 45) {
        score -= 900;
        age_tag = "old_low_ovr";
    } else if (age >= 34u) {
        score -= 350;
        age_tag = "aging";
    }

    const char* core_tag = "depth";
    if (overall >= 60 || career >= 60) {
        score += 1000;
        core_tag = "core";
    } else if (overall >= 50 || career >= 50) {
        score += 450;
        core_tag = "regular";
    }
    if (talent > overall + 15) {
        score += 650;
        core_tag = "upside";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        salary_tag = "unknown_salary";
    } else if (salary <= 70000000 && (age <= 27u || talent >= overall + 10)) {
        score += 450;
        salary_tag = "cheap_control";
    } else if (salary >= 800000000 && age >= 33u && overall < 55) {
        score -= 1000;
        salary_tag = "bad_contract";
    } else if (salary >= 500000000 && age >= 32u) {
        score -= 450;
        salary_tag = "costly_vet";
    } else if (salary >= 300000000 && overall >= 60) {
        score += 200;
        salary_tag = "paid_core";
    }

    const char* scarcity_tag = "normal_role";
    if ((role_bucket == 1 && team_role_count <= 2)
            || (role_bucket == 2 && team_role_count <= 5)
            || (role_bucket == 3 && team_role_count <= 6)
            || (role_bucket == 0 && team_role_count <= 12)) {
        score += 650;
        scarcity_tag = "scarce_role";
    } else if ((role_bucket == 1 && team_role_count >= 5)
            || (role_bucket == 2 && team_role_count >= 11)
            || (role_bucket == 3 && team_role_count >= 11)
            || (role_bucket == 0 && team_role_count >= 24)) {
        score -= 200;
        scarcity_tag = "deep_role";
    }

    if (reason != NULL && reason_size > 0) {
        snprintf(
            reason,
            reason_size,
            "%s/%s/%s/%s/base%d",
            age_tag,
            core_tag,
            salary_tag,
            scarcity_tag,
            base);
    }
    return score;
}

static int kbo_fa_role_bucket(uint8_t role)
{
    if (role == 1u) {
        return 0; /* Pitcher */
    }
    if (role == 2u) {
        return 1; /* Catcher */
    }
    if (role >= 3u && role <= 6u) {
        return 2; /* Infielder */
    }
    if (role >= 7u && role <= 9u) {
        return 3; /* Outfielder */
    }
    return 4;
}

static int kbo_fa_team_role_count(uint32_t team_id, int role_bucket)
{
    if (team_id == 0u || role_bucket < 0) {
        return 0;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int count = 0;
    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u) {
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (current_team_id != team_id && active_team_id != team_id) {
            continue;
        }
        if (kbo_fa_role_bucket(player[OOTP27_PLAYER_POSITION_ROLE_OFFSET]) == role_bucket) {
            count++;
        }
    }
    return count;
}

static int32_t kbo_fa_compensation_player_decision_score(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidate,
    char* reason,
    size_t reason_size)
{
    if (reason != NULL && reason_size > 0) {
        reason[0] = '\0';
    }
    if (rec == NULL || candidate == NULL || candidate->player_id == 0u) {
        return -2147483647;
    }

    uint8_t* player = kbo_find_player_by_id(candidate->player_id, NULL, NULL);
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return candidate->score - 100000;
    }

    int32_t base = kbo_foreign_waiver_value_score(player);
    int32_t salary = *(int32_t*)(player + OOTP27_PLAYER_CONTRACT_SALARY_Y1_OFFSET);
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int role_bucket = kbo_fa_role_bucket(candidate->role);
    int team_role_count = kbo_fa_team_role_count(rec->original_team_id, role_bucket);

    int32_t score = base;
    const char* age_tag = "prime";
    if (candidate->age <= 22u) {
        score += 1000;
        age_tag = "prospect";
    } else if (candidate->age <= 25u) {
        score += 700;
        age_tag = "young";
    } else if (candidate->age <= 29u) {
        score += 250;
        age_tag = "prime";
    } else if (candidate->age >= 35u) {
        score -= 900;
        age_tag = "old";
    } else if (candidate->age >= 32u) {
        score -= 350;
        age_tag = "aging";
    }

    const char* salary_tag = "salary_ok";
    if (salary <= 0) {
        score += 150;
        salary_tag = "unknown_salary";
    } else if (salary <= 70000000) {
        score += 500;
        salary_tag = "cheap";
    } else if (salary >= 600000000) {
        score -= 900;
        salary_tag = "expensive";
    } else if (salary >= 300000000) {
        score -= 350;
        salary_tag = "costly";
    }

    const char* need_tag = "neutral_need";
    if ((role_bucket == 1 && team_role_count < 2)
            || (role_bucket == 2 && team_role_count < 5)
            || (role_bucket == 3 && team_role_count < 7)
            || (role_bucket == 0 && team_role_count < 13)) {
        score += 700;
        need_tag = "team_need";
    } else if ((role_bucket == 1 && team_role_count > 4)
            || (role_bucket == 2 && team_role_count > 10)
            || (role_bucket == 3 && team_role_count > 10)
            || (role_bucket == 0 && team_role_count > 22)) {
        score -= 250;
        need_tag = "surplus_role";
    }

    if (talent > overall + 15) {
        score += 450;
    }

    if (reason != NULL && reason_size > 0) {
        snprintf(
            reason,
            reason_size,
            "%s/%s/%s/base%d",
            age_tag,
            salary_tag,
            need_tag,
            base);
    }
    return score;
}

static int kbo_fa_compensation_player_is_rookie_auto_protected(
    uint8_t* player,
    const KboFaCompensationRecord* rec)
{
    if (player == NULL || rec == NULL) {
        return 0;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
        return 0;
    }
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
    if (age > 23u) {
        return 0;
    }
    uint32_t draft_league_id = *(uint32_t*)(player + OOTP27_PLAYER_DRAFT_LEAGUE_ID_OFFSET);
    if (draft_league_id != rec->league_id) {
        return 0;
    }
    uint8_t draft_class = player[OOTP27_PLAYER_DRAFT_CLASS_OFFSET];
    uint8_t draft_subtype = player[OOTP27_PLAYER_DRAFT_SUBTYPE_OFFSET];
    uint8_t draft_extra = player[OOTP27_PLAYER_DRAFT_EXTRA_FLAG_OFFSET];
    uint8_t generation_context = player[OOTP27_PLAYER_GENERATION_CONTEXT_OFFSET];
    return draft_class != 0u || draft_subtype != 0u || draft_extra != 0u || generation_context != 0u;
}

static int kbo_build_fa_compensation_protected_candidates(
    const KboFaCompensationRecord* rec,
    KboFaProtectedCandidate* candidates,
    int max_candidates,
    KboFaProtectedCandidate* auto_protected,
    int max_auto_protected)
{
    if (rec == NULL || candidates == NULL || max_candidates <= 0
            || rec->signing_team_id == 0u || rec->protect_count == 0u) {
        return 0;
    }
    memset(candidates, 0, (SIZE_T)max_candidates * sizeof(candidates[0]));
    if (auto_protected != NULL && max_auto_protected > 0) {
        memset(auto_protected, 0, (SIZE_T)max_auto_protected * sizeof(auto_protected[0]));
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        return 0;
    }

    int count = 0;
    int auto_count = 0;
    for (int32_t i = 0; i < player_count && count < max_candidates; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (player_id == 0u || (current_team_id != rec->signing_team_id && active_team_id != rec->signing_team_id)) {
            continue;
        }
        if (player[OOTP27_PLAYER_RETIRED_FLAG_OFFSET] != 0u) {
            continue;
        }

        const char* auto_reason = NULL;
        if (player_id == rec->player_id) {
            auto_reason = "signed_fa";
        } else if (kbo_player_is_foreign_for_kbo_rights(player)) {
            auto_reason = "foreign";
        } else if (player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u) {
            auto_reason = "military";
        } else if (kbo_fa_compensation_player_is_rookie_auto_protected(player, rec)) {
            auto_reason = "rookie_draftee";
        }
        if (auto_reason != NULL) {
            if (auto_protected != NULL && auto_count < max_auto_protected) {
                KboFaProtectedCandidate* automatic = &auto_protected[auto_count++];
                automatic->player_id = player_id;
                automatic->team_id = current_team_id != 0u ? current_team_id : active_team_id;
                automatic->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
                automatic->role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
                automatic->score = kbo_fa_protection_candidate_score(player, rec->signing_team_id, automatic->reason, sizeof(automatic->reason));
                snprintf(automatic->reason, sizeof(automatic->reason), "%s", auto_reason);
                kbo_copy_player_display_name(player, automatic->player_name, sizeof(automatic->player_name));
                if (automatic->player_name[0] == '\0' || strcmp(automatic->player_name, "Unknown player") == 0) {
                    snprintf(automatic->player_name, sizeof(automatic->player_name), "Player #%u", player_id);
                }
            }
            continue;
        }

        KboFaProtectedCandidate* candidate = &candidates[count++];
        candidate->player_id = player_id;
        candidate->team_id = current_team_id != 0u ? current_team_id : active_team_id;
        candidate->age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        candidate->role = player[OOTP27_PLAYER_POSITION_ROLE_OFFSET];
        candidate->score = kbo_fa_protection_candidate_score(player, rec->signing_team_id, candidate->reason, sizeof(candidate->reason));
        kbo_copy_player_display_name(player, candidate->player_name, sizeof(candidate->player_name));
        if (candidate->player_name[0] == '\0' || strcmp(candidate->player_name, "Unknown player") == 0) {
            snprintf(candidate->player_name, sizeof(candidate->player_name), "Player #%u", player_id);
        }
    }

    qsort(candidates, (size_t)count, sizeof(candidates[0]), kbo_fa_protected_candidate_compare_desc);
    return count;
}

static int kbo_persist_fa_compensation_protected_list(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const char* source)
{
    if (rec == NULL || candidates == NULL || candidate_count <= 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protected_lists_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA protected list persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,signed_on,due_on,generated_on,"
            "protect_count,protected_count,protected_player_ids,protected_player_names,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    int protected_count = candidate_count;
    if (protected_count > (int)rec->protect_count) {
        protected_count = (int)rec->protect_count;
    }
    char prefix[256] = {0};
    int len = snprintf(
        prefix,
        sizeof(prefix),
        "%u,%u,%s,%u,%u,%u,%u,%u,%u,%d,",
        rec->player_id,
        rec->season,
        rec->grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        generated_yyyymmdd,
        rec->protect_count,
        protected_count);
    if (len > 0 && len < (int)sizeof(prefix)) {
        WriteFile(file, prefix, (DWORD)len, &written, NULL);
    }

    char ids[1024] = {0};
    char names[4096] = {0};
    for (int i = 0; i < protected_count; i++) {
        char chunk[32] = {0};
        snprintf(chunk, sizeof(chunk), "%s%u", i == 0 ? "" : ";", candidates[i].player_id);
        strncat(ids, chunk, sizeof(ids) - strlen(ids) - 1u);
        snprintf(chunk, sizeof(chunk), "%s", i == 0 ? "" : ";");
        strncat(names, chunk, sizeof(names) - strlen(names) - 1u);
        strncat(names, candidates[i].player_name, sizeof(names) - strlen(names) - 1u);
    }
    kbo_fa_market_write_csv_text(file, ids);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_market_write_csv_text(file, names);
    WriteFile(file, ",", 1, &written, NULL);
    kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_protected_list_ai");
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);

    append_logf(
        "KBO FA protected list generated fa_player=%u signing_team=%u original_team=%u grade=%s protected=%d/%u due=%u path=%s",
        rec->player_id,
        rec->signing_team_id,
        rec->original_team_id,
        rec->grade,
        protected_count,
        rec->protect_count,
        due_yyyymmdd,
        path);
    return 1;
}

static int kbo_persist_fa_compensation_protection_debug(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t generated_yyyymmdd,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    const KboFaProtectedCandidate* auto_protected,
    int auto_protected_count,
    const char* source)
{
    if (rec == NULL || candidates == NULL || candidate_count < 0) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protection_debug_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA protection debug persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,due_on,generated_on,"
            "list_type,rank,player_id,player_name,score,age,role,reason,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    int protected_count = candidate_count > (int)rec->protect_count ? (int)rec->protect_count : candidate_count;
    for (int i = 0; i < candidate_count; i++) {
        const KboFaProtectedCandidate* candidate = &candidates[i];
        const char* list_type = i < protected_count ? "protected" : "unprotected";
        char prefix[256] = {0};
        int len = snprintf(
            prefix,
            sizeof(prefix),
            "%u,%u,%s,%u,%u,%u,%u,%s,%d,%u,",
            rec->player_id,
            rec->season,
            rec->grade,
            rec->original_team_id,
            rec->signing_team_id,
            due_yyyymmdd,
            generated_yyyymmdd,
            list_type,
            i + 1,
            candidate->player_id);
        if (len > 0 && len < (int)sizeof(prefix)) {
            WriteFile(file, prefix, (DWORD)len, &written, NULL);
        }
        kbo_fa_market_write_csv_text(file, candidate->player_name);
        char suffix[128] = {0};
        len = snprintf(
            suffix,
            sizeof(suffix),
            ",%d,%u,%u,%s,",
            candidate->score,
            (uint32_t)candidate->age,
            (uint32_t)candidate->role,
            candidate->reason);
        if (len > 0 && len < (int)sizeof(suffix)) {
            WriteFile(file, suffix, (DWORD)len, &written, NULL);
        }
        kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_protection_debug");
        WriteFile(file, "\r\n", 2, &written, NULL);
    }

    if (auto_protected != NULL && auto_protected_count > 0) {
        for (int i = 0; i < auto_protected_count; i++) {
            const KboFaProtectedCandidate* automatic = &auto_protected[i];
            char prefix[256] = {0};
            int len = snprintf(
                prefix,
                sizeof(prefix),
                "%u,%u,%s,%u,%u,%u,%u,auto_protected,%d,%u,",
                rec->player_id,
                rec->season,
                rec->grade,
                rec->original_team_id,
                rec->signing_team_id,
                due_yyyymmdd,
                generated_yyyymmdd,
                i + 1,
                automatic->player_id);
            if (len > 0 && len < (int)sizeof(prefix)) {
                WriteFile(file, prefix, (DWORD)len, &written, NULL);
            }
            kbo_fa_market_write_csv_text(file, automatic->player_name);
            char suffix[128] = {0};
            len = snprintf(
                suffix,
                sizeof(suffix),
                ",%d,%u,%u,%s,",
                automatic->score,
                (uint32_t)automatic->age,
                (uint32_t)automatic->role,
                automatic->reason);
            if (len > 0 && len < (int)sizeof(suffix)) {
                WriteFile(file, suffix, (DWORD)len, &written, NULL);
            }
            kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_protection_debug");
            WriteFile(file, "\r\n", 2, &written, NULL);
        }
    }

    CloseHandle(file);
    append_logf(
        "KBO FA protection debug written fa_player=%u protected=%d unprotected=%d auto=%d path=%s",
        rec->player_id,
        protected_count,
        candidate_count - protected_count,
        auto_protected_count,
        path);
    return 1;
}

static int kbo_load_fa_compensation_protection_debug_rows(
    KboFaCompensationProtectionDebugRow* rows,
    int max_rows)
{
    if (rows == NULL || max_rows <= 0) {
        return 0;
    }
    memset(rows, 0, (SIZE_T)max_rows * sizeof(rows[0]));

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_protection_debug_path(path, sizeof(path))) {
        return 0;
    }
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }
    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 2u * 1024u * 1024u) {
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
    while (*cursor != '\0' && count < max_rows) {
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
                KboFaCompensationProtectionDebugRow* row = &rows[count];
                row->fa_player_id = kbo_fa_compensation_parse_u32(fields[0]);
                row->season = kbo_fa_compensation_parse_u32(fields[1]);
                kbo_fa_compensation_copy_token(fields[2], row->grade, sizeof(row->grade));
                row->original_team_id = kbo_fa_compensation_parse_u32(fields[3]);
                row->signing_team_id = kbo_fa_compensation_parse_u32(fields[4]);
                row->due_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[5]);
                row->generated_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[6]);
                kbo_fa_compensation_copy_token(fields[7], row->list_type, sizeof(row->list_type));
                row->rank = kbo_fa_compensation_parse_u32(fields[8]);
                row->player_id = kbo_fa_compensation_parse_u32(fields[9]);
                kbo_fa_compensation_copy_token(fields[10], row->player_name, sizeof(row->player_name));
                row->score = kbo_fa_compensation_parse_i32(fields[11]);
                row->age = (uint16_t)kbo_fa_compensation_parse_u32(fields[12]);
                row->role = (uint8_t)kbo_fa_compensation_parse_u32(fields[13]);
                kbo_fa_compensation_copy_token(fields[14], row->reason, sizeof(row->reason));
                kbo_fa_compensation_copy_token(fields[15], row->source, sizeof(row->source));
                if (row->fa_player_id != 0u && row->player_id != 0u) {
                    count++;
                } else {
                    memset(row, 0, sizeof(*row));
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

static int kbo_persist_fa_compensation_player_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const KboFaProtectedCandidate* selected,
    int unprotected_candidate_count,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA compensation decision persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,signed_on,due_on,decided_on,"
            "action,selected_player_id,selected_player_name,selected_player_score,unprotected_candidate_count,"
            "cash_with_player,cash_only,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char prefix[256] = {0};
    int len = snprintf(
        prefix,
        sizeof(prefix),
        "%u,%u,%s,%u,%u,%u,%u,%u,PLAYER,%u,",
        rec->player_id,
        rec->season,
        rec->grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        decided_yyyymmdd,
        selected->player_id);
    if (len > 0 && len < (int)sizeof(prefix)) {
        WriteFile(file, prefix, (DWORD)len, &written, NULL);
    }
    kbo_fa_market_write_csv_text(file, selected->player_name);

    char suffix[160] = {0};
    len = snprintf(
        suffix,
        sizeof(suffix),
        ",%d,%d,%u,%u,",
        selected->score,
        unprotected_candidate_count,
        rec->cash_with_player,
        rec->cash_only);
    if (len > 0 && len < (int)sizeof(suffix)) {
        WriteFile(file, suffix, (DWORD)len, &written, NULL);
    }
    kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_player_ai");
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);

    append_logf(
        "KBO FA compensation player selected fa_player=%u original_team=%u signing_team=%u selected=%u name=%s score=%d unprotected=%d decision=%s",
        rec->player_id,
        rec->original_team_id,
        rec->signing_team_id,
        selected->player_id,
        selected->player_name,
        selected->score,
        unprotected_candidate_count,
        path);
    return 1;
}

static int kbo_persist_fa_compensation_cash_only_decision(
    const KboFaCompensationRecord* rec,
    uint32_t due_yyyymmdd,
    uint32_t decided_yyyymmdd,
    const char* source)
{
    if (rec == NULL || rec->player_id == 0u || rec->cash_only == 0u) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
        return 0;
    }
    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    int exists = GetFileAttributesExA(path, GetFileExInfoStandard, &attrs);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO FA compensation cash-only decision persist failed reason=open gle=%lu path=%s", GetLastError(), path);
        return 0;
    }

    DWORD written = 0;
    if (!exists || (attrs.nFileSizeHigh == 0u && attrs.nFileSizeLow == 0u)) {
        const char* header =
            "fa_player_id,season,grade,original_team_id,signing_team_id,signed_on,due_on,decided_on,"
            "action,selected_player_id,selected_player_name,selected_player_score,unprotected_candidate_count,"
            "cash_with_player,cash_only,source\r\n";
        WriteFile(file, header, (DWORD)strlen(header), &written, NULL);
    }

    char row[512] = {0};
    int len = snprintf(
        row,
        sizeof(row),
        "%u,%u,%s,%u,%u,%u,%u,%u,CASH_ONLY,0,\"\",0,0,%u,%u,",
        rec->player_id,
        rec->season,
        rec->grade,
        rec->original_team_id,
        rec->signing_team_id,
        rec->signed_on_yyyymmdd,
        due_yyyymmdd,
        decided_yyyymmdd,
        rec->cash_with_player,
        rec->cash_only);
    if (len > 0 && len < (int)sizeof(row)) {
        WriteFile(file, row, (DWORD)len, &written, NULL);
    }
    kbo_fa_market_write_csv_text(file, source != NULL ? source : "fa_compensation_cash_only_ai");
    WriteFile(file, "\r\n", 2, &written, NULL);
    CloseHandle(file);

    append_logf(
        "KBO FA compensation cash-only selected fa_player=%u original_team=%u signing_team=%u cash_only=%u decision=%s",
        rec->player_id,
        rec->original_team_id,
        rec->signing_team_id,
        rec->cash_only,
        path);
    return 1;
}

static int kbo_load_latest_fa_compensation_decision(
    uint32_t fa_player_id,
    KboFaCompensationDecisionRow* out)
{
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    if (fa_player_id == 0u || out == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_fa_compensation_decisions_path(path, sizeof(path))) {
        return 0;
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

    int found = 0;
    char* cursor = buffer;
    while (*cursor != '\0') {
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
            if (ok && kbo_fa_compensation_parse_u32(fields[0]) == fa_player_id) {
                out->fa_player_id = kbo_fa_compensation_parse_u32(fields[0]);
                out->season = kbo_fa_compensation_parse_u32(fields[1]);
                kbo_fa_compensation_copy_token(fields[2], out->grade, sizeof(out->grade));
                out->original_team_id = kbo_fa_compensation_parse_u32(fields[3]);
                out->signing_team_id = kbo_fa_compensation_parse_u32(fields[4]);
                out->signed_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[5]);
                out->due_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[6]);
                out->decided_on_yyyymmdd = kbo_fa_compensation_parse_u32(fields[7]);
                kbo_fa_compensation_copy_token(fields[8], out->action, sizeof(out->action));
                out->selected_player_id = kbo_fa_compensation_parse_u32(fields[9]);
                kbo_fa_compensation_copy_token(fields[10], out->selected_player_name, sizeof(out->selected_player_name));
                out->selected_player_score = kbo_fa_compensation_parse_i32(fields[11]);
                out->unprotected_candidate_count = (int)kbo_fa_compensation_parse_u32(fields[12]);
                out->cash_with_player = kbo_fa_compensation_parse_u32(fields[13]);
                out->cash_only = kbo_fa_compensation_parse_u32(fields[14]);
                kbo_fa_compensation_copy_token(fields[15], out->source, sizeof(out->source));
                found = out->selected_player_id != 0u || strcmp(out->action, "CASH_ONLY") == 0;
            }
        }
        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static void kbo_emit_fa_compensation_player_selected_news(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t decided_yyyymmdd)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || decided_yyyymmdd == 0u) {
        return;
    }

    uint32_t year = decided_yyyymmdd / 10000u;
    uint32_t month = (decided_yyyymmdd / 100u) % 100u;
    uint32_t day = decided_yyyymmdd % 100u;
    if (year < 1982u || month == 0u || day == 0u) {
        return;
    }

    char cash_text[32] = "-";
    kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_text, sizeof(cash_text));

    char selected_name[96] = {0};
    snprintf(
        selected_name,
        sizeof(selected_name),
        "%s",
        selected->player_name[0] != '\0' ? selected->player_name : "Compensation player");

    char history_text[512] = {0};
    snprintf(
        history_text,
        sizeof(history_text),
        "[G]Selected as KBO FA compensation player for %s; rights transfer from Team #%u to Team #%u is recorded with %s cash compensation.",
        rec->player_name[0] != '\0' ? rec->player_name : "FA signing",
        rec->signing_team_id,
        rec->original_team_id,
        cash_text);
    insert_kbo_player_history_sql(selected->player_id, year, month, day, history_text, "fa_compensation_player");

    char title[160] = {0};
    snprintf(title, sizeof(title), "KBO FA compensation player: %s", selected_name);

    char body[1200] = {0};
    snprintf(
        body,
        sizeof(body),
        "<%s:player#%u> has been selected by <Team #%u:team#%u> as the Grade %s KBO FA compensation player after <%s:player#%u> signed with <Team #%u:team#%u>.\n\nThe compensation package recorded by the KBO mod is %s plus one eligible player from outside <Team #%u:team#%u>'s protected list.",
        selected_name,
        selected->player_id,
        rec->original_team_id,
        rec->original_team_id,
        rec->grade,
        rec->player_name[0] != '\0' ? rec->player_name : "FA player",
        rec->player_id,
        rec->signing_team_id,
        rec->signing_team_id,
        cash_text,
        rec->signing_team_id,
        rec->signing_team_id);

    create_kbo_native_live_news_with_body(year, month, day, rec->league_id, 10u, title, body);
}

static int kbo_transfer_fa_compensation_player_to_original_team(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    uint32_t transfer_yyyymmdd,
    const char* source)
{
    if (rec == NULL || selected == NULL || selected->player_id == 0u || rec->original_team_id == 0u) {
        return 0;
    }

    uint32_t before_current_team = 0u;
    uint32_t before_current_league = 0u;
    uint8_t* player = kbo_find_player_by_id(selected->player_id, &before_current_team, &before_current_league);
    uint8_t* destination_team = find_kbo_team_by_numeric_id_any_league(rec->original_team_id, 1);
    if (player == NULL || destination_team == NULL) {
        append_logf(
            "KBO FA compensation transfer failed reason=lookup fa_player=%u selected=%u player=%p team=%p original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            player,
            destination_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    uint32_t before_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (before_current_team != rec->signing_team_id && before_active_team != rec->signing_team_id) {
        append_logf(
            "KBO FA compensation transfer skipped reason=player_not_on_signing_team fa_player=%u selected=%u current=%u active=%u signing_team=%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            before_active_team,
            rec->signing_team_id,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int called_pre_change = 0;
    int called_register = 0;
    int called_attach = 0;
    kbo_assign_player_to_team_like_ootp(
        player,
        destination_team,
        rec->league_id,
        &called_pre_change,
        &called_register,
        &called_attach);

    uint32_t after_current_team = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    if (after_current_team != rec->original_team_id && after_active_team != rec->original_team_id) {
        append_logf(
            "KBO FA compensation transfer failed reason=post_verify fa_player=%u selected=%u current=%u->%u active=%u->%u original_team=%u source=%s",
            rec->player_id,
            selected->player_id,
            before_current_team,
            after_current_team,
            before_active_team,
            after_active_team,
            rec->original_team_id,
            source != NULL ? source : "");
        return 0;
    }

    int year = (int)(transfer_yyyymmdd / 10000u);
    int month = (int)((transfer_yyyymmdd / 100u) % 100u);
    int day = (int)(transfer_yyyymmdd % 100u);
    char history_text[256] = {0};
    snprintf(
        history_text,
        sizeof(history_text),
        "[G]Transferred to Team #%u as the KBO FA compensation player for %s.",
        rec->original_team_id,
        rec->player_name[0] != '\0' ? rec->player_name : "the signing");
    insert_kbo_player_history_sql(selected->player_id, year, month, day, history_text, "fa_compensation_transfer");

    append_logf(
        "KBO FA compensation player transferred fa_player=%u selected=%u name=%s signing_team=%u original_team=%u current=%u->%u active=%u->%u pre=%d register=%d attach=%d source=%s",
        rec->player_id,
        selected->player_id,
        selected->player_name,
        rec->signing_team_id,
        rec->original_team_id,
        before_current_team,
        after_current_team,
        before_active_team,
        after_active_team,
        called_pre_change,
        called_register,
        called_attach,
        source != NULL ? source : "");
    return 1;
}

static int kbo_select_fa_compensation_player_from_candidates(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* candidates,
    int candidate_count,
    KboFaProtectedCandidate* out_selected,
    int* out_unprotected_count)
{
    if (out_selected != NULL) {
        memset(out_selected, 0, sizeof(*out_selected));
    }
    if (out_unprotected_count != NULL) {
        *out_unprotected_count = 0;
    }
    if (rec == NULL || candidates == NULL || out_selected == NULL || candidate_count <= 0) {
        return 0;
    }

    int protected_count = (int)rec->protect_count;
    if (protected_count < 0) {
        protected_count = 0;
    }
    if (protected_count > candidate_count) {
        protected_count = candidate_count;
    }
    int unprotected_count = candidate_count - protected_count;
    if (out_unprotected_count != NULL) {
        *out_unprotected_count = unprotected_count;
    }
    if (unprotected_count <= 0) {
        return 0;
    }

    int best_index = -1;
    int32_t best_score = -2147483647;
    char best_reason[96] = {0};
    for (int i = protected_count; i < candidate_count; i++) {
        char reason[96] = {0};
        int32_t score = kbo_fa_compensation_player_decision_score(rec, &candidates[i], reason, sizeof(reason));
        if (best_index < 0
                || score > best_score
                || (score == best_score && candidates[i].age < candidates[best_index].age)
                || (score == best_score && candidates[i].age == candidates[best_index].age
                    && candidates[i].player_id < candidates[best_index].player_id)) {
            best_index = i;
            best_score = score;
            snprintf(best_reason, sizeof(best_reason), "%s", reason);
        }
    }
    if (best_index < 0) {
        return 0;
    }

    *out_selected = candidates[best_index];
    out_selected->score = best_score;
    snprintf(out_selected->reason, sizeof(out_selected->reason), "%s", best_reason[0] != '\0' ? best_reason : "decision_score");
    return out_selected->player_id != 0u;
}

static int kbo_fa_compensation_ai_prefers_cash_only(
    const KboFaCompensationRecord* rec,
    const KboFaProtectedCandidate* selected,
    int unprotected_count)
{
    /* TODO: Feed FA compensation cost into the FA signing AI itself.
       A compensated FA should carry an acquisition penalty before the contract is offered,
       so AI clubs do not sign marginal players when the true price includes cash and a
       protected-list loss. This belongs at the FA target/offer valuation hook, not here:
       this function only decides player-plus-cash versus cash-only after a signing exists. */
    if (rec == NULL || rec->cash_only == 0u) {
        return 0;
    }
    if (selected == NULL || selected->player_id == 0u || unprotected_count <= 0) {
        return 1;
    }

    uint32_t extra_cash = rec->cash_only > rec->cash_with_player
        ? rec->cash_only - rec->cash_with_player : 0u;
    if (selected->score < 65000) {
        return 1;
    }
    if (selected->score < 80000 && extra_cash >= rec->previous_salary) {
        return 1;
    }
    return 0;
}

static int kbo_manual_select_fa_compensation_player(
    uint32_t fa_player_id,
    uint32_t selected_player_id,
    const char* source)
{
    if (fa_player_id == 0u || selected_player_id == 0u) {
        return 0;
    }

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaCompensationProtectionDebugRow* rows = (KboFaCompensationProtectionDebugRow*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaCompensationProtectionDebugRow));
    if (records == NULL || rows == NULL) {
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (rows != NULL) { HeapFree(GetProcessHeap(), 0, rows); }
        return 0;
    }

    kbo_fa_compensation_lock_ledger(source != NULL ? source : "manual_compensation_select");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    KboFaCompensationRecord* rec = NULL;
    for (int i = 0; i < record_count; i++) {
        if (records[i].player_id == fa_player_id) {
            rec = &records[i];
            break;
        }
    }
    if (rec == NULL || !rec->requires_player_compensation || rec->protect_count == 0u
            || rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED) {
        append_logf(
            "KBO FA manual compensation select skipped reason=record fa_player=%u selected=%u status=%u source=%s",
            fa_player_id,
            selected_player_id,
            rec != NULL ? (uint32_t)rec->status : 999u,
            source != NULL ? source : "");
        kbo_fa_compensation_unlock_ledger();
        HeapFree(GetProcessHeap(), 0, rows);
        HeapFree(GetProcessHeap(), 0, records);
        return 0;
    }

    int row_count = kbo_load_fa_compensation_protection_debug_rows(rows, KBO_FA_COMPENSATION_PROTECTED_LIST_MAX);
    KboFaProtectedCandidate selected;
    memset(&selected, 0, sizeof(selected));
    int unprotected_count = 0;
    for (int i = 0; i < row_count; i++) {
        if (rows[i].fa_player_id != fa_player_id) {
            continue;
        }
        if (strcmp(rows[i].list_type, "unprotected") == 0) {
            unprotected_count++;
            if (rows[i].player_id == selected_player_id) {
                selected.player_id = rows[i].player_id;
                selected.team_id = rows[i].signing_team_id;
                selected.age = rows[i].age;
                selected.role = rows[i].role;
                selected.score = rows[i].score;
                snprintf(selected.reason, sizeof(selected.reason), "%s", rows[i].reason);
                snprintf(selected.player_name, sizeof(selected.player_name), "%s", rows[i].player_name);
            }
        }
    }
    if (selected.player_id == 0u) {
        append_logf(
            "KBO FA manual compensation select skipped reason=not_unprotected fa_player=%u selected=%u source=%s",
            fa_player_id,
            selected_player_id,
            source != NULL ? source : "");
        kbo_fa_compensation_unlock_ledger();
        HeapFree(GetProcessHeap(), 0, rows);
        HeapFree(GetProcessHeap(), 0, records);
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        today = rec->signed_on_yyyymmdd;
    }
    KboFaRules rules;
    kbo_fa_rules_load(&rules);
    uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules.protected_list_due_days);
    int ok = kbo_persist_fa_compensation_player_decision(
        rec,
        due,
        today,
        &selected,
        unprotected_count,
        source != NULL ? source : "hub_manual_compensation_select");
    if (ok) {
        rec->status = KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED;
        kbo_persist_fa_compensation_records(records, record_count);
    }

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, rows);
    HeapFree(GetProcessHeap(), 0, records);
    return ok;
}

static int kbo_manual_select_fa_compensation_cash_only(uint32_t fa_player_id, const char* source)
{
    if (fa_player_id == 0u) {
        return 0;
    }
    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today)) {
        return 0;
    }
    KboFaRules rules;
    kbo_fa_rules_load(&rules);

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        return 0;
    }
    kbo_fa_compensation_lock_ledger(source != NULL ? source : "manual_cash_only_select");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int ok = 0;
    for (int i = 0; i < record_count; i++) {
        KboFaCompensationRecord* rec = &records[i];
        if (rec->player_id != fa_player_id) {
            continue;
        }
        if (!rec->requires_player_compensation
                || rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED
                || rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED) {
            break;
        }
        uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules.protected_list_due_days);
        ok = kbo_persist_fa_compensation_cash_only_decision(
            rec,
            due,
            today,
            source != NULL ? source : "hub_cash_only_select");
        if (ok) {
            rec->status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED;
            kbo_persist_fa_compensation_records(records, record_count);
        }
        break;
    }

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, records);
    return ok;
}

static int kbo_submit_fa_compensation_protected_list_record(
    KboFaCompensationRecord* rec,
    uint32_t today,
    const KboFaRules* rules,
    KboFaProtectedCandidate* candidates,
    KboFaProtectedCandidate* auto_protected,
    const char* source)
{
    if (rec == NULL || rules == NULL || candidates == NULL || auto_protected == NULL
            || !rec->requires_player_compensation
            || rec->protect_count == 0u
            || rec->signed_on_yyyymmdd == 0u) {
        return 0;
    }
    if (rec->status != KBO_FA_COMPENSATION_STATUS_PENDING
            && rec->status != KBO_FA_COMPENSATION_STATUS_RECORDED) {
        return 0;
    }

    uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules->protected_list_due_days);
    if (due == 0u) {
        return 0;
    }

    int candidate_count = kbo_build_fa_compensation_protected_candidates(
        rec,
        candidates,
        KBO_FA_COMPENSATION_PROTECTED_LIST_MAX,
        auto_protected,
        KBO_FA_COMPENSATION_PROTECTED_LIST_MAX);
    if (candidate_count <= 0) {
        append_logf(
            "KBO FA protected list deferred reason=no_candidates fa_player=%u signing_team=%u due=%u",
            rec->player_id,
            rec->signing_team_id,
            due);
        return 0;
    }

    int auto_protected_count = 0;
    while (auto_protected_count < KBO_FA_COMPENSATION_PROTECTED_LIST_MAX
            && auto_protected[auto_protected_count].player_id != 0u) {
        auto_protected_count++;
    }
    kbo_persist_fa_compensation_protection_debug(
        rec,
        due,
        today,
        candidates,
        candidate_count,
        auto_protected,
        auto_protected_count,
        source);
    if (!kbo_persist_fa_compensation_protected_list(rec, due, today, candidates, candidate_count, source)) {
        return 0;
    }

    KboFaProtectedCandidate selected;
    int unprotected_count = 0;
    if (kbo_select_fa_compensation_player_from_candidates(
            rec,
            candidates,
            candidate_count,
            &selected,
            &unprotected_count)) {
        if (kbo_fa_compensation_ai_prefers_cash_only(rec, &selected, unprotected_count)
                && kbo_persist_fa_compensation_cash_only_decision(
                    rec,
                    due,
                    today,
                    source != NULL ? source : "fa_compensation_ai_cash_only")) {
            rec->status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED;
        } else if (kbo_persist_fa_compensation_player_decision(
                rec,
                due,
                today,
                &selected,
                unprotected_count,
                source != NULL ? source : "fa_compensation_ai_default")) {
            rec->status = KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED;
        } else {
            rec->status = KBO_FA_COMPENSATION_STATUS_RECORDED;
        }
    } else {
        if (kbo_persist_fa_compensation_cash_only_decision(
                rec,
                due,
                today,
                source != NULL ? source : "fa_compensation_ai_cash_only_no_player")) {
            rec->status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED;
        } else {
            rec->status = KBO_FA_COMPENSATION_STATUS_RECORDED;
        }
    }
    return 1;
}

static int kbo_manual_submit_fa_compensation_protected_list(uint32_t fa_player_id, const char* source)
{
    if (fa_player_id == 0u) {
        return 0;
    }
    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        return 0;
    }
    KboFaRules rules;
    kbo_fa_rules_load(&rules);

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaProtectedCandidate* candidates = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    KboFaProtectedCandidate* auto_protected = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    if (records == NULL || candidates == NULL || auto_protected == NULL) {
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        if (auto_protected != NULL) { HeapFree(GetProcessHeap(), 0, auto_protected); }
        return 0;
    }

    kbo_fa_compensation_lock_ledger(source != NULL ? source : "manual_protected_list_submit");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int ok = 0;
    for (int i = 0; i < record_count; i++) {
        if (records[i].player_id == fa_player_id) {
            ok = kbo_submit_fa_compensation_protected_list_record(
                &records[i],
                today,
                &rules,
                candidates,
                auto_protected,
                source != NULL ? source : "hub_protected_list_submit");
            break;
        }
    }
    if (ok) {
        kbo_persist_fa_compensation_records(records, record_count);
    }

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, auto_protected);
    HeapFree(GetProcessHeap(), 0, candidates);
    HeapFree(GetProcessHeap(), 0, records);
    return ok;
}

static int kbo_process_due_fa_compensation_protected_lists(const char* source)
{
    if (!kbo_fix_enabled() || read_kbo_localappdata_flag_file("disable_kbo_fa_compensation.txt")) {
        return 0;
    }

    uint32_t today = 0u;
    if (!kbo_get_current_yyyymmdd(&today) || today == 0u) {
        return 0;
    }

    KboFaRules rules;
    kbo_fa_rules_load(&rules);

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    KboFaProtectedCandidate* candidates = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    KboFaProtectedCandidate* auto_protected = (KboFaProtectedCandidate*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)KBO_FA_COMPENSATION_PROTECTED_LIST_MAX * sizeof(KboFaProtectedCandidate));
    if (records == NULL || candidates == NULL || auto_protected == NULL) {
        if (records != NULL) { HeapFree(GetProcessHeap(), 0, records); }
        if (candidates != NULL) { HeapFree(GetProcessHeap(), 0, candidates); }
        if (auto_protected != NULL) { HeapFree(GetProcessHeap(), 0, auto_protected); }
        return 0;
    }

    kbo_fa_compensation_lock_ledger(source != NULL ? source : "process_due");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    int generated = 0;
    for (int i = 0; i < record_count; i++) {
        KboFaCompensationRecord* rec = &records[i];
        if (rec->status == KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED) {
            KboFaCompensationDecisionRow decision;
            if (kbo_load_latest_fa_compensation_decision(rec->player_id, &decision)
                    && strcmp(decision.action, "CASH_ONLY") == 0) {
                append_logf(
                    "KBO FA compensation cash-only recorded fa_player=%u original_team=%u signing_team=%u cash_only=%u source=%s",
                    rec->player_id,
                    rec->original_team_id,
                    rec->signing_team_id,
                    rec->cash_only,
                    source != NULL ? source : "");
                rec->status = KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED;
                generated++;
            }
            continue;
        }
        if (rec->status == KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED) {
            KboFaCompensationDecisionRow decision;
            if (kbo_load_latest_fa_compensation_decision(rec->player_id, &decision)
                    && strcmp(decision.action, "PLAYER") == 0) {
                KboFaProtectedCandidate selected;
                memset(&selected, 0, sizeof(selected));
                selected.player_id = decision.selected_player_id;
                selected.team_id = rec->signing_team_id;
                selected.score = decision.selected_player_score;
                snprintf(selected.player_name, sizeof(selected.player_name), "%s", decision.selected_player_name);
                if (kbo_transfer_fa_compensation_player_to_original_team(rec, &selected, today, source)) {
                    kbo_emit_fa_compensation_player_selected_news(rec, &selected, today);
                    rec->status = KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED;
                    generated++;
                }
            }
            continue;
        }
        if (rec->status != KBO_FA_COMPENSATION_STATUS_PENDING
                || !rec->requires_player_compensation
                || rec->protect_count == 0u
                || rec->signed_on_yyyymmdd == 0u) {
            continue;
        }
        uint32_t due = kbo_add_days_yyyymmdd(rec->signed_on_yyyymmdd, rules.protected_list_due_days);
        if (due == 0u || today < due) {
            continue;
        }

        if (kbo_submit_fa_compensation_protected_list_record(
                rec,
                today,
                &rules,
                candidates,
                auto_protected,
                source)) {
            generated++;
        }
    }
    if (generated > 0) {
        kbo_persist_fa_compensation_records(records, record_count);
    }

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, auto_protected);
    HeapFree(GetProcessHeap(), 0, candidates);
    HeapFree(GetProcessHeap(), 0, records);
    return generated;
}

static void kbo_fa_compensation_record_history(const KboFaCompensationRecord* rec)
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

    uint32_t signing_year = 0u;
    uint32_t filing_original_team_id = 0u;
    uint32_t filing_league_id = 0u;
    uint32_t filing_season = 0u;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    if (kbo_fa_filing_find_latest_player(player_id, &filing_original_team_id, &filing_league_id, &filing_season)
            && filing_season >= 1982u
            && filing_season <= 2300u) {
        signing_year = filing_season;
        if (filing_league_id != 0u) {
            league_id = filing_league_id;
        }
    }
    if (signing_year == 0u) {
        signing_year = kbo_find_league_year_from_id_no_scan(league_id);
    }
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

    kbo_fa_compensation_lock_ledger(source != NULL ? source : "record_signing");
    char path[MAX_PATH] = {0};
    int record_count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));
    if (kbo_fa_compensation_find_existing(
            records,
            record_count,
            row.player_id,
            signing_year,
            row.original_team_id,
            signing_team_id) >= 0) {
        kbo_fa_compensation_unlock_ledger();
        HeapFree(GetProcessHeap(), 0, records);
        return 0;
    }

    if (record_count >= KBO_FA_COMPENSATION_MAX) {
        append_logf("KBO FA compensation skipped reason=ledger_full player=%u path=%s", row.player_id, path);
        kbo_fa_compensation_unlock_ledger();
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
        kbo_fa_compensation_record_history(rec);
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

    kbo_fa_compensation_unlock_ledger();
    HeapFree(GetProcessHeap(), 0, records);
    return persisted;
}

static const char* kbo_fa_compensation_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED:
        return "Cash only recorded";
    case KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED:
        return "Cash only selected";
    case KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED:
        return "Player transferred";
    case KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED:
        return "Player selected";
    case KBO_FA_COMPENSATION_STATUS_RECORDED:
        return "List submitted";
    case KBO_FA_COMPENSATION_STATUS_PENDING:
    default:
        return "Pending";
    }
}
