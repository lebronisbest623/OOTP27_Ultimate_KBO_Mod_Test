static void kbo_build_overview_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    uint8_t* sang = find_kbo_team_by_csv_id_any_league("SANG", 0);
    uint8_t* kpb  = find_kbo_team_by_csv_id_any_league("KPB",  0);
    uint32_t sang_id = sang != NULL ? *(uint32_t*)(sang + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    uint32_t kpb_id  = kpb  != NULL ? *(uint32_t*)(kpb  + OOTP27_KBO_TEAM_ID_OFFSET) : 0;
    int sang_count = kbo_hub_count_service_players(sang_id, NULL, NULL);
    int kpb_count  = kbo_hub_count_service_players(kpb_id,  NULL, NULL);

    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", kbo_hub_text("\xec\x9a\x94\xec\x95\xbd", "OVERVIEW"));

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80", "SERVICE TEAMS"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xec\x83\x81\xeb\xac\xb4 \xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91: %d\xeb\xaa\x85\r\n", "  Sangmu serving: %d\r\n"),
        sang_count);
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xea\xb2\xbd\xec\xb0\xb0 \xeb\xb3\xb5\xeb\xac\xb4 \xec\xa4\x91: %d\xeb\xaa\x85\r\n", "  Police serving: %d\r\n"),
        kpb_count);
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xec\xb6\x94\xec\xa0\x81 \xec\xa4\x91\xec\x9d\xb8 \xeb\xb3\xb5\xeb\xac\xb4 \xeb\xb0\xb0\xec\xa0\x95: %ld\xea\xb1\xb4\r\n\r\n", "  Tracked assignments: %ld\r\n\r\n"),
        g_active_military_loan_count);

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xed\x8c\xa8\xec\xb9\x98 \xec\x83\x81\xed\x83\x9c", "PATCH STATUS"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  KBO Fix: %s\r\n", "  KBO Fix: %s\r\n"),
        kbo_fix_enabled() ? kbo_hub_text("\xec\xbc\x9c\xec\xa7\x90", "enabled") : kbo_hub_text("\xea\xba\xbc\xec\xa7\x90", "disabled"));
    char foreign_waiver_status[220] = {0};
    if (kbo_get_foreign_waiver_window_status_text(foreign_waiver_status, sizeof(foreign_waiver_status))) {
        kbo_window_text_appendf(&buffer, "%s\r\n", foreign_waiver_status);
    }
}
