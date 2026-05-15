#include "../ui_asian_games_view_internal.h"

int kbo_webview_asian_games_schedule(KboAsianGamesScheduleSeed* out)
{
    uint32_t year = 0u;
    uint32_t month = 0u;
    uint32_t day = 0u;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        kbo_current_year_relaxed(&year);
    }
    if (year < 1982u || year > 2200u) {
        return 0;
    }
    return kbo_get_next_asian_games_schedule(year, out);
}

void kbo_webview_format_asian_games_date(uint32_t yyyymmdd, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (yyyymmdd == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    snprintf(
        out,
        out_size,
        "%04u-%02u-%02u",
        yyyymmdd / 10000u,
        (yyyymmdd / 100u) % 100u,
        yyyymmdd % 100u);
}

void kbo_webview_format_asian_games_date_range(uint32_t start, uint32_t end, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    if (start == 0u && end == 0u) {
        snprintf(out, out_size, "-");
        return;
    }
    if (start == 0u || end == 0u || start == end) {
        kbo_webview_format_asian_games_date(start != 0u ? start : end, out, out_size);
        return;
    }

    char start_text[16] = {0};
    char end_text[16] = {0};
    kbo_webview_format_asian_games_date(start, start_text, sizeof(start_text));
    kbo_webview_format_asian_games_date(end, end_text, sizeof(end_text));
    snprintf(out, out_size, "%s - %s", start_text, end_text);
}

const char* kbo_webview_asian_games_tournament_status_class(const KboAsianGamesScheduleSeed* schedule)
{
    if (schedule == NULL || schedule->status[0] == '\0') {
        return "roServing";
    }
    if (ascii_equals_ignore_case(schedule->status, "official")
            || ascii_equals_ignore_case(schedule->status, "confirmed")) {
        return "roReady";
    }
    if (ascii_equals_ignore_case(schedule->status, "provisional")) {
        return "roSoon";
    }
    if (ascii_equals_ignore_case(schedule->status, "projected")) {
        return "roMuted";
    }
    return "roServing";
}

const char* kbo_webview_asian_games_tournament_phase(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today,
    const char** out_class)
{
    if (out_class != NULL) {
        *out_class = "roServing";
    }
    if (schedule == NULL || schedule->year == 0u || today == 0u) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "미정";
    }
    if (schedule->tournament_start != 0u
            && schedule->tournament_end != 0u
            && today >= schedule->tournament_start
            && today <= schedule->tournament_end) {
        if (out_class != NULL) { *out_class = "roOrange"; }
        return "진행 중";
    }
    if (schedule->final_date != 0u && today > schedule->final_date) {
        if (out_class != NULL) { *out_class = "roMuted"; }
        return "완료";
    }
    if (schedule->selection_date != 0u
            && schedule->tournament_start != 0u
            && today >= schedule->selection_date
            && today < schedule->tournament_start) {
        if (out_class != NULL) { *out_class = "roSoon"; }
        return "대표팀 창";
    }
    return "예정";
}

int kbo_webview_asian_games_tournament_is_announced(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t current_year)
{
    if (schedule == NULL || schedule->year == 0u) {
        return 0;
    }
    if (!ascii_equals_ignore_case(schedule->status, "projected")) {
        return 1;
    }
    return current_year != 0u && schedule->year <= current_year + 6u;
}

int kbo_webview_asian_games_tournament_is_scheduled(
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today,
    uint32_t current_year)
{
    if (!kbo_webview_asian_games_tournament_is_announced(schedule, current_year)) {
        return 0;
    }
    if (today != 0u && schedule->final_date != 0u && today > schedule->final_date) {
        return 0;
    }
    return schedule != NULL && schedule->year >= current_year;
}

void kbo_webview_append_asian_games_tournament_row(
    KboWindowTextBuffer* buffer,
    const KboAsianGamesScheduleSeed* schedule,
    uint32_t today)
{
    if (buffer == NULL || schedule == NULL || schedule->year == 0u) {
        return;
    }

    char host_text[128] = {0};
    if (schedule->host_city[0] != '\0' && schedule->host_country[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s, %s", schedule->host_city, schedule->host_country);
    } else if (schedule->host_city[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s", schedule->host_city);
    } else if (schedule->host_country[0] != '\0') {
        snprintf(host_text, sizeof(host_text), "%s", schedule->host_country);
    } else {
        snprintf(host_text, sizeof(host_text), "미정");
    }

    char tournament_text[40] = {0};
    char selection_text[16] = {0};
    char departure_text[16] = {0};
    char final_text[16] = {0};
    kbo_webview_format_asian_games_date_range(
        schedule->tournament_start,
        schedule->tournament_end,
        tournament_text,
        sizeof(tournament_text));
    kbo_webview_format_asian_games_date(schedule->selection_date, selection_text, sizeof(selection_text));
    kbo_webview_format_asian_games_date(schedule->departure_date, departure_text, sizeof(departure_text));
    kbo_webview_format_asian_games_date(schedule->final_date, final_text, sizeof(final_text));

    const char* phase_class = "";
    const char* phase = kbo_webview_asian_games_tournament_phase(schedule, today, &phase_class);
    const char* status_class = kbo_webview_asian_games_tournament_status_class(schedule);
    const char* status = kbo_asian_games_schedule_status_label(schedule);

    kbo_window_text_appendf(
        buffer,
        "<tr><td class='roPo'>%u</td><td class='roName'>",
        schedule->year);
    kbo_html_append_escaped(buffer, host_text);
    kbo_window_text_appendf(buffer, "</td><td class='roDate'>");
    kbo_html_append_escaped(buffer, tournament_text);
    kbo_window_text_appendf(buffer, "</td><td class='roClub'>");
    kbo_html_append_escaped(buffer, selection_text);
    kbo_window_text_appendf(buffer, "</td><td class='roTeam'>");
    kbo_html_append_escaped(buffer, departure_text);
    kbo_window_text_appendf(buffer, "</td><td class='roReturn'>");
    kbo_html_append_escaped(buffer, final_text);
    kbo_window_text_appendf(buffer, "</td><td class='roStatus %s'>", status_class);
    kbo_html_append_escaped(buffer, status);
    kbo_window_text_appendf(buffer, "</td><td class='roResult %s'>", phase_class);
    kbo_html_append_escaped(buffer, phase);
    kbo_window_text_appendf(buffer, "</td></tr>");
}

void kbo_webview_append_asian_games_tournaments_view(KboWindowTextBuffer* buffer)
{
    uint32_t current_year = 0u;
    uint32_t current_month = 0u;
    uint32_t current_day = 0u;
    if (!kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        kbo_current_year_relaxed(&current_year);
    }
    if (current_year < 2026u || current_year > 2200u) {
        current_year = 2026u;
    }

    uint32_t today = 0u;
    kbo_get_current_yyyymmdd(&today);

    KboAsianGamesScheduleSeed schedules[64];
    memset(schedules, 0, sizeof(schedules));
    int count = 0;
    uint32_t display_through_year = current_year + 6u;
    if (display_through_year > 2200u) {
        display_through_year = 2200u;
    }
    for (uint32_t year = current_year; year <= display_through_year && count < (int)(sizeof(schedules) / sizeof(schedules[0])); year++) {
        KboAsianGamesScheduleSeed schedule;
        if (kbo_get_asian_games_schedule_for_year(year, &schedule)
                && kbo_webview_asian_games_tournament_is_scheduled(&schedule, today, current_year)) {
            schedules[count++] = schedule;
        }
    }

    int official_count = 0;
    int provisional_count = 0;
    int projected_count = 0;
    for (int i = 0; i < count; i++) {
        if (ascii_equals_ignore_case(schedules[i].status, "official")
                || ascii_equals_ignore_case(schedules[i].status, "confirmed")) {
            official_count++;
        } else if (ascii_equals_ignore_case(schedules[i].status, "provisional")) {
            provisional_count++;
        } else if (ascii_equals_ignore_case(schedules[i].status, "projected")) {
            projected_count++;
        }
    }

    char summary_text[256] = {0};
    if (count > 0) {
        snprintf(
            summary_text,
            sizeof(summary_text),
            "보기: 대회 - 예정 %d개 - 확정: %d / 잠정: %d / 예상: %d",
            count,
            official_count,
            provisional_count,
            projected_count);
    } else {
        snprintf(summary_text, sizeof(summary_text), "보기: 대회 - 아시안게임");
    }

    kbo_window_text_appendf(buffer, "<div class='rights rosterRights'>");
    kbo_webview_append_roster_top_bar(buffer, summary_text);
    kbo_window_text_appendf(
        buffer,
        "<section class='tablewrap rosterTableWrap'><table class='ootpRosterTable agTournamentTable'><thead><tr>"
        "<th class='roPo' data-sort-type='number'>연도</th><th class='roName' data-sort-type='text'>개최지</th>"
        "<th class='roDate' data-sort-type='date'>대회</th><th class='roClub' data-sort-type='date'>명단 발표</th>"
        "<th class='roTeam' data-sort-type='date'>출국</th><th class='roReturn' data-sort-type='date'>결승</th>"
        "<th class='roStatus' data-sort-type='text'>상태</th><th class='roResult' data-sort-type='text'>단계</th>"
        "</tr></thead><tbody>");

    if (count <= 0) {
        kbo_window_text_appendf(buffer, "<tr><td colspan='8' class='roEmptyMessage'></td></tr>");
    } else {
        for (int i = 0; i < count; i++) {
            kbo_webview_append_asian_games_tournament_row(buffer, &schedules[i], today);
        }
    }
    kbo_window_text_appendf(buffer, "</tbody></table></section></div>");
}
