#include "../internal/cbt_internal.h"

#include "../../core/files/save_paths/core_save_paths.h"
#include "../../core/news/ledger/core_news_ledger.h"
#include "../../hotkey_window/api/hotkey_window_refresh.h"

#define KBO_CBT_NEWS_LEDGER_DOMAIN "cbt"

static int kbo_cbt_news_marker_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("cbt_news_markers.txt", out, out_size);
}

static int kbo_cbt_news_marker_exists(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0;
    }
    if (kbo_custom_news_ledger_completed(KBO_CBT_NEWS_LEDGER_DOMAIN, key)) {
        return 1;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_news_marker_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD high = 0u;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > 1024u * 1024u) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0u;
    int exists = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read == size) {
        buffer[size] = '\0';
        const size_t key_len = strlen(key);
        const char* cursor = buffer;
        while (*cursor != '\0') {
            while (*cursor == '\r' || *cursor == '\n') {
                cursor++;
            }
            const char* line = cursor;
            while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n') {
                cursor++;
            }
            if ((size_t)(cursor - line) == key_len && memcmp(line, key, key_len) == 0) {
                exists = 1;
                break;
            }
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    if (exists) {
        kbo_custom_news_ledger_record_completed(
            KBO_CBT_NEWS_LEDGER_DOMAIN,
            key,
            "legacy_marker_backfill",
            "cbt_news_marker_exists");
    }
    return exists;
}

static void kbo_cbt_news_persist_marker(const char* key, const char* source)
{
    if (key == NULL || key[0] == '\0' || kbo_cbt_news_marker_exists(key)) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_cbt_news_marker_path(path, sizeof(path))) {
        append_logf(
            "KBO CBT news marker skipped source=%s key=%s reason=path_unavailable",
            source != NULL ? source : "",
            key);
        return;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf(
            "KBO CBT news marker skipped source=%s key=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            key,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    char line[160] = {0};
    int len = snprintf(line, sizeof(line), "%s\r\n", key);
    if (len > 0) {
        DWORD written = 0u;
        if (WriteFile(file, line, (DWORD)len, &written, NULL) && written == (DWORD)len) {
            kbo_custom_news_ledger_record_completed(
                KBO_CBT_NEWS_LEDGER_DOMAIN,
                key,
                "legacy_marker_persist",
                source);
        }
    }
    CloseHandle(file);
}

static int kbo_cbt_find_team(const KboCbtTeamPayroll* teams, int count, uint32_t team_id)
{
    for (int i = 0; i < count; i++) {
        if (teams[i].team_id == team_id) {
            return i;
        }
    }
    return -1;
}

static uint32_t kbo_cbt_effective_top_n(uint32_t top_n)
{
    if (top_n == 0u) {
        return 1u;
    }
    if (top_n > KBO_CBT_SALARY_SCRATCH_MAX) {
        return KBO_CBT_SALARY_SCRATCH_MAX;
    }
    return top_n;
}

static void kbo_cbt_track_top_salary(KboCbtTeamPayroll* team, int32_t salary, uint32_t top_n)
{
    if (team == NULL || salary <= 0) {
        return;
    }

    if (team->domestic_count < (int)top_n) {
        team->domestic_salaries[team->domestic_count++] = salary;
        return;
    }

    int min_index = 0;
    int32_t min_salary = team->domestic_salaries[0];
    for (int i = 1; i < team->domestic_count; i++) {
        if (team->domestic_salaries[i] < min_salary) {
            min_salary = team->domestic_salaries[i];
            min_index = i;
        }
    }

    if (salary > min_salary) {
        team->domestic_salaries[min_index] = salary;
    }
}

static int32_t kbo_cbt_adjust_salary_for_exception(
    const KboFaSalarySnapshotGrade* grade,
    const KboCbtExceptionDesignation* exceptions,
    int exception_count,
    uint32_t season,
    int32_t* out_credit)
{
    if (out_credit != NULL) {
        *out_credit = 0;
    }
    if (grade == NULL || grade->salary <= 0 || grade->player_key[0] == '\0') {
        return grade != NULL ? grade->salary : 0;
    }
    if (kbo_cbt_exception_find_designation(
            exceptions,
            exception_count,
            season,
            grade->ranking_team_id,
            grade->player_key) < 0) {
        return grade->salary;
    }
    int32_t credit = grade->salary / 2;
    if (out_credit != NULL) {
        *out_credit = credit;
    }
    return grade->salary - credit;
}

static void kbo_cbt_compute_team_payrolls(
    const KboFaSalarySnapshotGrade* grades,
    int grade_count,
    uint32_t season,
    const KboCbtExceptionDesignation* exceptions,
    int exception_count,
    uint32_t top_n,
    KboCbtTeamPayroll* teams,
    int* team_count_out)
{
    *team_count_out = 0;

    uint32_t effective_top_n = kbo_cbt_effective_top_n(top_n);
    KboCbtTeamPayroll* scratch = (KboCbtTeamPayroll*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_TEAM_MAX * sizeof(KboCbtTeamPayroll));
    if (scratch == NULL) {
        append_log_line("KBO CBT skipped reason=team_scratch_alloc_failed");
        return;
    }

    int team_count = 0;

    for (int i = 0; i < grade_count; i++) {
        const KboFaSalarySnapshotGrade* g = &grades[i];
        if (g->ranking_team_id == 0u || g->foreign_flag || g->salary <= 0) {
            continue;
        }

        int ti = kbo_cbt_find_team(scratch, team_count, g->ranking_team_id);
        if (ti < 0) {
            if (team_count >= KBO_CBT_TEAM_MAX) {
                continue;
            }
            ti = team_count++;
            scratch[ti].team_id = g->ranking_team_id;
        }

        KboCbtTeamPayroll* tp = &scratch[ti];
        int32_t exception_credit = 0;
        int32_t adjusted_salary = kbo_cbt_adjust_salary_for_exception(
            g,
            exceptions,
            exception_count,
            season,
            &exception_credit);
        tp->exception_credit += exception_credit;
        kbo_cbt_track_top_salary(tp, adjusted_salary, effective_top_n);
    }

    for (int i = 0; i < team_count; i++) {
        KboCbtTeamPayroll* tp = &scratch[i];
        int32_t sum = 0;
        for (int j = 0; j < tp->domestic_count; j++) {
            sum += tp->domestic_salaries[j];
        }
        tp->top_n_sum = sum;
        teams[i] = *tp;
    }
    *team_count_out = team_count;
    HeapFree(GetProcessHeap(), 0, scratch);
}

void kbo_process_competitive_balance_tax(uint32_t season, const char* source)
{
    kbo_process_competitive_balance_tax_for_date(season, 0u, source);
}

void kbo_process_competitive_balance_tax_for_date(uint32_t season, uint32_t news_yyyymmdd, const char* source)
{
    if (season < 1982u || season > 2200u) {
        return;
    }

    if (read_kbo_localappdata_flag_file("disable_kbo_competitive_balance_tax.txt")) {
        append_logf("KBO CBT skipped season=%u source=%s reason=flag_disabled", season, source != NULL ? source : "");
        return;
    }

    /* One-shot schema probe so we can discover the draft pick table structure */
    static volatile LONG probe_done = 0;
    if (InterlockedCompareExchange(&probe_done, 1, 0) == 0) {
        uintptr_t global = get_ootp_global_database();
        if (global != 0 && memory_range_readable((void*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET), sizeof(uintptr_t))) {
            uintptr_t db = *(uintptr_t*)(global + OOTP27_GLOBAL_SQL_DATABASE_OFFSET);
            if (db != 0) {
                kbo_cbt_draft_probe_schema((void*)db);
            }
        }
    }

    KboCbtRules rules;
    kbo_cbt_rules_load(&rules);
    if (!rules.enabled) {
        append_logf("KBO CBT skipped season=%u source=%s reason=disabled", season, source != NULL ? source : "");
        return;
    }

    int32_t threshold = kbo_cbt_get_threshold(&rules, season);
    if (threshold <= 0) {
        append_logf("KBO CBT skipped season=%u source=%s reason=no_threshold", season, source != NULL ? source : "");
        return;
    }

    /* Load salary snapshot for this season */
    KboFaSalarySnapshotGrade* grades = (KboFaSalarySnapshotGrade*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_SALARY_SNAPSHOT_GRADE_MAX * sizeof(KboFaSalarySnapshotGrade));
    if (grades == NULL) {
        return;
    }

    int grade_count = kbo_fa_salary_snapshot_load_grade_rows(season, grades, KBO_FA_SALARY_SNAPSHOT_GRADE_MAX, NULL, 0);
    if (grade_count <= 0) {
        append_logf("KBO CBT skipped season=%u source=%s reason=no_salary_data", season, source != NULL ? source : "");
        HeapFree(GetProcessHeap(), 0, grades);
        return;
    }

    /* Compute per-team domestic payroll (top N) */
    KboCbtTeamPayroll* teams = (KboCbtTeamPayroll*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_TEAM_MAX * sizeof(KboCbtTeamPayroll));
    if (teams == NULL) {
        append_logf("KBO CBT skipped season=%u source=%s reason=team_alloc_failed", season, source != NULL ? source : "");
        HeapFree(GetProcessHeap(), 0, grades);
        return;
    }

    int team_count = 0;
    kbo_cbt_exception_auto_designate_missing(season, source != NULL ? source : "cbt_process");
    KboCbtExceptionDesignation exceptions[KBO_CBT_EXCEPTION_MAX];
    int exception_count = kbo_cbt_exception_load_designations(exceptions, KBO_CBT_EXCEPTION_MAX);
    kbo_cbt_compute_team_payrolls(grades, grade_count, season, exceptions, exception_count, rules.top_player_count, teams, &team_count);
    HeapFree(GetProcessHeap(), 0, grades);

    if (team_count == 0) {
        append_logf("KBO CBT skipped season=%u source=%s reason=no_teams", season, source != NULL ? source : "");
        HeapFree(GetProcessHeap(), 0, teams);
        return;
    }

    /* Load existing records to check consecutive history */
    KboCbtRecord* records = (KboCbtRecord*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_CBT_RECORDS_MAX * sizeof(KboCbtRecord));
    if (records == NULL) {
        HeapFree(GetProcessHeap(), 0, teams);
        return;
    }
    int record_count = kbo_cbt_load_records(records, KBO_CBT_RECORDS_MAX, NULL, 0);

    uint32_t year = season;
    uint32_t month = 4u;
    uint32_t day = 1u;
    kbo_cbt_news_date(season, news_yyyymmdd, &year, &month, &day);
    uint32_t league_id = kbo_resolve_kbo_league_id();

    int new_violations = 0;
    for (int i = 0; i < team_count; i++) {
        uint32_t team_id = teams[i].team_id;
        int32_t payroll = teams[i].top_n_sum;

        /* Skip if already processed this season for this team */
        if (kbo_cbt_find_record(records, record_count, season, team_id) >= 0) {
            continue;
        }

        int32_t overage = payroll - threshold;
        uint32_t prior_consecutive = kbo_cbt_get_consecutive_count(records, record_count, team_id, season);
        uint32_t consecutive_count = overage > 0 ? prior_consecutive + 1u : 0u;
        uint32_t tax_rate = kbo_cbt_get_tax_rate(&rules, consecutive_count);
        int32_t tax_amount = overage > 0
            ? (int32_t)((int64_t)overage * (int32_t)tax_rate / 100LL)
            : 0;

        KboCbtRecord rec = {0};
        rec.season           = season;
        rec.team_id          = team_id;
        rec.payroll          = payroll;
        rec.threshold        = threshold;
        rec.overage          = overage > 0 ? overage : 0;
        rec.tax_rate_pct     = tax_rate;
        rec.tax_amount       = tax_amount;
        rec.consecutive_count = consecutive_count;
        rec.processed_date   = year * 10000u + month * 100u + day;
        kbo_cbt_copy_team_name(team_id, rec.team_name, sizeof(rec.team_name));

        if (record_count < KBO_CBT_RECORDS_MAX) {
            records[record_count++] = rec;
        }

        if (overage > 0) {
            new_violations++;
            int draft_penalty = (int)consecutive_count >= (int)rules.draft_penalty_min_consecutive;
            append_logf(
                "KBO CBT violation season=%u team=%u payroll=%d threshold=%d overage=%d rate=%u%% tax=%d consecutive=%u draft_penalty=%d exception_credit=%d source=%s",
                season, team_id, payroll, threshold, overage,
                tax_rate, tax_amount, consecutive_count, draft_penalty,
                teams[i].exception_credit,
                source != NULL ? source : "");

            if (league_id != 0u) {
                kbo_cbt_insert_violation_news_v2(league_id, year, month, day, &rec, &rules);
            }
        } else {
            append_logf(
                "KBO CBT clean season=%u team=%u payroll=%d threshold=%d exception_credit=%d source=%s",
                season, team_id, payroll, threshold,
                teams[i].exception_credit,
                source != NULL ? source : "");
        }
    }

    kbo_cbt_save_records(records, record_count);
    if (league_id != 0u) {
        char summary_marker[64] = {0};
        snprintf(summary_marker, sizeof(summary_marker), "summary|%u|%u", season, league_id);
        if (kbo_cbt_news_marker_exists(summary_marker)) {
            append_logf(
                "KBO CBT opening-day news skipped season=%u league_id=%u reason=summary_marker_exists source=%s",
                season,
                league_id,
                source != NULL ? source : "");
        } else {
            int summary_created = kbo_cbt_insert_opening_day_summary_news(
                league_id,
                year,
                month,
                day,
                season,
                records,
                record_count,
                team_count,
                &rules);
            if (summary_created) {
                kbo_cbt_news_persist_marker(summary_marker, source);
            }
        }
    }
    kbo_request_hotkey_window_refresh("competitive_balance_tax_processed");
    HeapFree(GetProcessHeap(), 0, records);
    HeapFree(GetProcessHeap(), 0, teams);

    append_logf(
        "KBO CBT processed season=%u teams=%d violations=%d threshold=%d source=%s",
        season, team_count, new_violations, threshold,
        source != NULL ? source : "");
}
