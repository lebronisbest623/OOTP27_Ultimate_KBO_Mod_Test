static void kbo_webview_append_military_applicant_summary(
    KboWindowTextBuffer* buffer,
    int application_active,
    int pending,
    int pitchers,
    int catchers,
    int infielders,
    int outfielders,
    int other,
    uint32_t anchor,
    uint32_t announcement)
{
    char anchor_text[16] = "-";
    char announcement_text[16] = "-";
    kbo_military_format_yyyymmdd(anchor, anchor_text, sizeof(anchor_text));
    kbo_military_format_yyyymmdd(announcement, announcement_text, sizeof(announcement_text));

    char summary_text[320] = {0};
    snprintf(
        summary_text,
        sizeof(summary_text),
        "View: Applicants - Status: %s - POS: P %d / C %d / INF %d / OF %d / Other %d - %d Applicants - Period: %s to %s",
        application_active ? "Open" : "Closed",
        pitchers,
        catchers,
        infielders,
        outfielders,
        other,
        pending,
        anchor_text,
        announcement_text);
    kbo_webview_append_roster_top_bar(buffer, summary_text);
}
