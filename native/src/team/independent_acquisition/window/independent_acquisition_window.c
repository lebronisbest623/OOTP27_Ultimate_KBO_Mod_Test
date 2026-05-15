#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "independent_acquisition_window.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../custom_events/runtime/dates/custom_event_dates.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../foreign/common/policy/foreign_waiver_policy.h"

static volatile LONG g_kbo_independent_team_acquisition_open_date = 0;

#define KBO_INDEPENDENT_TEAM_ACQUISITION_WINDOW_FILE "independent_acquisition_window.txt"

static int kbo_independent_team_acquisition_window_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file(
        KBO_INDEPENDENT_TEAM_ACQUISITION_WINDOW_FILE,
        out,
        out_size);
}

static void kbo_independent_team_acquisition_persist_open_date(
    uint32_t event_yyyymmdd,
    const char* source)
{
    char path[MAX_PATH] = {0};
    if (event_yyyymmdd == 0u
            || !kbo_independent_team_acquisition_window_path(path, sizeof(path))) {
        return;
    }

    char text[32] = {0};
    snprintf(text, sizeof(text), "%u\r\n", event_yyyymmdd);
    HANDLE file = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        kbo_log_runtimef(
            "KBO independent futures acquisition window persist skipped source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    DWORD written = 0u;
    DWORD len = (DWORD)strlen(text);
    if (!WriteFile(file, text, len, &written, NULL) || written != len) {
        kbo_log_runtimef(
            "KBO independent futures acquisition window persist failed source=%s date=%u gle=%lu path=%s",
            source != NULL ? source : "",
            event_yyyymmdd,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

static uint32_t kbo_independent_team_acquisition_load_open_date(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_independent_team_acquisition_window_path(path, sizeof(path))) {
        return 0u;
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
        return 0u;
    }

    char text[32] = {0};
    DWORD read = 0u;
    int ok = ReadFile(file, text, sizeof(text) - 1u, &read, NULL) && read > 0u;
    CloseHandle(file);
    if (!ok) {
        return 0u;
    }

    unsigned long value = strtoul(text, NULL, 10);
    if (value < 19820101ul || value > 22001231ul) {
        return 0u;
    }
    return (uint32_t)value;
}

static uint32_t kbo_independent_team_acquisition_event_league_id(void)
{
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    return league_id;
}

static int kbo_independent_team_acquisition_build_news_text(
    char* title,
    size_t title_size,
    char* body,
    size_t body_size,
    const char* source)
{
    if (title == NULL || title_size == 0u || body == NULL || body_size == 0u) {
        return 0;
    }

    title[0] = '\0';
    body[0] = '\0';
    if (!kbo_news_template_render_key(
            "independent_acquisition.open.title",
            NULL,
            0,
            title,
            title_size,
            source != NULL ? source : "independent_team_acquisition")) {
        snprintf(title, title_size, "Futures Independent Club Signing Window Opens");
    }
    if (!kbo_news_template_render_key(
            "independent_acquisition.open.news.body",
            NULL,
            0,
            body,
            body_size,
            source != NULL ? source : "independent_team_acquisition")) {
        snprintf(
            body,
            body_size,
            "KBO opened the signing window for independent clubs playing in the Futures League.\n\n"
            "The window is scheduled from the opening day of the independent club's league, "
            "and player acquisitions will be handled under the separate independent club process.");
    }

    return title[0] != '\0' && body[0] != '\0';
}

static int kbo_emit_independent_team_acquisition_open_news(
    uint32_t event_yyyymmdd,
    const char* source)
{
    uint32_t league_id = kbo_independent_team_acquisition_event_league_id();
    if (event_yyyymmdd == 0u || league_id == 0u) {
        kbo_log_runtimef(
            "KBO independent futures acquisition news skipped source=%s date=%u league_id=%u reason=invalid_context",
            source != NULL ? source : "",
            event_yyyymmdd,
            league_id);
        return 0;
    }

    char title[180] = {0};
    char body[2048] = {0};
    if (!kbo_independent_team_acquisition_build_news_text(
            title,
            sizeof(title),
            body,
            sizeof(body),
            source)) {
        kbo_log_runtimef(
            "KBO independent futures acquisition news skipped source=%s date=%u league_id=%u reason=text_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd,
            league_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    kbo_log_runtimef(
        "KBO independent futures acquisition news source=%s title=%s date=%u league_id=%u created=%d",
        source != NULL ? source : "",
        title,
        event_yyyymmdd,
        league_id,
        created);
    return created;
}

int kbo_handle_independent_team_acquisition_open_event(
    uint32_t event_yyyymmdd,
    const char* source)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    LONG previous = InterlockedExchange(
        &g_kbo_independent_team_acquisition_open_date,
        (LONG)event_yyyymmdd);
    kbo_independent_team_acquisition_persist_open_date(event_yyyymmdd, source);
    uint32_t news_yyyymmdd = kbo_custom_event_effective_news_date(event_yyyymmdd);
    kbo_log_runtimef(
        "KBO independent futures acquisition window opened source=%s event_date=%u news_date=%u previous=%u",
        source != NULL ? source : "",
        event_yyyymmdd,
        news_yyyymmdd,
        (uint32_t)previous);
    return kbo_emit_independent_team_acquisition_open_news(news_yyyymmdd, source) ? 1 : 0;
}

uint32_t kbo_independent_team_acquisition_window_open_date(void)
{
    uint32_t cached = (uint32_t)InterlockedCompareExchange(
        &g_kbo_independent_team_acquisition_open_date,
        0,
        0);
    if (cached != 0u) {
        return cached;
    }

    uint32_t loaded = kbo_independent_team_acquisition_load_open_date();
    if (loaded != 0u) {
        InterlockedCompareExchange(
            &g_kbo_independent_team_acquisition_open_date,
            (LONG)loaded,
            0);
    }
    return loaded;
}
