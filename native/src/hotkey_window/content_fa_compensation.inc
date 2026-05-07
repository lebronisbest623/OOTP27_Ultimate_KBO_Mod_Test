static void kbo_build_fa_compensation_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data = out;
    buffer.capacity = out_size;
    buffer.length = 0;

    KboFaCompensationRecord* records = (KboFaCompensationRecord*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        (SIZE_T)KBO_FA_COMPENSATION_MAX * sizeof(KboFaCompensationRecord));
    if (records == NULL) {
        kbo_window_text_appendf(&buffer, "FA COMPENSATION\r\n\r\nCould not allocate compensation buffer.\r\n");
        return;
    }

    char path[MAX_PATH] = {0};
    int count = kbo_load_fa_compensation_records(records, KBO_FA_COMPENSATION_MAX, path, sizeof(path));

    kbo_window_text_appendf(&buffer, "FA COMPENSATION\r\n");
    kbo_window_text_appendf(&buffer, "Rows: %d. CSV: %s\r\n\r\n", count, path[0] != '\0' ? path : "-");
    kbo_window_text_appendf(&buffer, "DATE     PLAYER                   GRADE FROM TO   PREV SALARY  CASH+PLAYER  CASH ONLY   PROT STATUS\r\n");
    kbo_window_text_appendf(&buffer, "------------------------------------------------------------------------------------------------\r\n");

    for (int i = 0; i < count && i < 600; i++) {
        KboFaCompensationRecord* rec = &records[i];
        char original_team[16] = "-";
        char signing_team[16] = "-";
        char previous_salary[32] = "-";
        char cash_with_player[32] = "-";
        char cash_only[32] = "-";
        kbo_hub_copy_team_abbrev_by_id(rec->original_team_id, original_team, sizeof(original_team), "-");
        kbo_hub_copy_team_abbrev_by_id(rec->signing_team_id, signing_team, sizeof(signing_team), "-");
        kbo_fa_market_format_salary(rec->previous_salary, previous_salary, sizeof(previous_salary));
        kbo_fa_market_format_salary((int32_t)rec->cash_with_player, cash_with_player, sizeof(cash_with_player));
        kbo_fa_market_format_salary((int32_t)rec->cash_only, cash_only, sizeof(cash_only));
        kbo_window_text_appendf(
            &buffer,
            "%8u %-24.24s %-5.5s %-4.4s %-4.4s %12s %12s %12s %4u %-8.8s\r\n",
            rec->signed_on_yyyymmdd,
            rec->player_name,
            kbo_fa_market_display_grade(rec->grade),
            original_team,
            signing_team,
            previous_salary,
            cash_with_player,
            cash_only,
            rec->protect_count,
            kbo_fa_compensation_status_label(rec->status));
    }
    if (count == 0) {
        kbo_window_text_appendf(&buffer, "No KBO FA compensation obligations recorded.\r\n");
    } else if (count > 600) {
        kbo_window_text_appendf(&buffer, "\r\nOutput truncated. Open the CSV for the full ledger.\r\n");
    }

    HeapFree(GetProcessHeap(), 0, records);
}
