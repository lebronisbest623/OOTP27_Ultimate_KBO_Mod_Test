#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"
#include "../../core/logging/core_log.h"
#include "../common/csv/foreign_csv_parse.h"
#include "foreign_waiver_announcements.h"
#include "../common/paths/foreign_waiver_paths.h"
#include "../common/policy/foreign_waiver_policy.h"
#include "foreign_waiver_results.h"
#include "../rights/query/foreign_waiver_rights_query.h"

static uint32_t g_kbo_foreign_waiver_last_result_announcement = 0;

static int kbo_build_foreign_waiver_result_body(char* out, size_t out_size, uint32_t today_yyyymmdd, const char* source)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    int retained = 0;
    int skipped = 0;
    int ai_retained = 0;
    int ai_skipped = 0;
    int user_retained = 0;
    int user_skipped = 0;
    int active_rights = 0;
    int decision_breakdown_available = 0;

    while (InterlockedCompareExchange(&g_kbo_foreign_waiver_rights_lock, 1, 0) != 0) {
        Sleep(0);
    }
    for (int i = 0; i < g_kbo_foreign_waiver_rights_count; i++) {
        if (kbo_is_foreign_waiver_right_active(&g_kbo_foreign_waiver_rights[i], today_yyyymmdd)) {
            active_rights++;
        }
    }
    InterlockedExchange(&g_kbo_foreign_waiver_rights_lock, 0);

    char decision_path[MAX_PATH] = {0};
    if (get_kbo_foreign_waiver_decisions_path(decision_path, sizeof(decision_path))) {
        HANDLE file = CreateFileA(decision_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD size = GetFileSize(file, NULL);
            if (size != INVALID_FILE_SIZE && size > 0 && size < 262144u) {
                char* input = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
                DWORD read = 0;
                if (input != NULL && ReadFile(file, input, size, &read, NULL) && read > 0) {
                    input[read] = '\0';
                    char* cursor = input;
                    while (*cursor != '\0') {
                        char* next = strchr(cursor, '\n');
                        if (next == NULL) {
                            next = cursor + strlen(cursor);
                        }

                        char line[256] = {0};
                        size_t len = (size_t)(next - cursor);
                        while (len > 0u && (cursor[len - 1u] == '\r' || cursor[len - 1u] == '\n')) {
                            len--;
                        }
                        if (len >= sizeof(line)) {
                            len = sizeof(line) - 1u;
                        }
                        memcpy(line, cursor, len);

                        const char* p = line;
                        uint32_t decision_date = 0u;
                        uint32_t window_start = 0u;
                        uint32_t window_end = 0u;
                        if (line[0] >= '0' && line[0] <= '9'
                                && parse_u32_from_csv_field(&p, &decision_date)
                                && parse_u32_from_csv_field(&p, &window_start)
                                && parse_u32_from_csv_field(&p, &window_end)
                                && window_end == today_yyyymmdd) {
                            char source_name[16] = {0};
                            char action_name[16] = {0};
                            while (*p == ',' || *p == ' ' || *p == '\t') { p++; }
                            size_t si = 0u;
                            while (*p != '\0' && *p != ',' && si + 1u < sizeof(source_name)) {
                                source_name[si++] = *p++;
                            }
                            source_name[si] = '\0';
                            if (*p == ',') { p++; }
                            size_t ai = 0u;
                            while (*p != '\0' && *p != ',' && ai + 1u < sizeof(action_name)) {
                                action_name[ai++] = *p++;
                            }
                            action_name[ai] = '\0';

                            if (_stricmp(action_name, "RETAIN") == 0) {
                                retained++;
                                decision_breakdown_available = 1;
                                if (_stricmp(source_name, "ai") == 0) { ai_retained++; }
                                else if (_stricmp(source_name, "user") == 0) { user_retained++; }
                            } else if (_stricmp(action_name, "SKIP") == 0) {
                                skipped++;
                                decision_breakdown_available = 1;
                                if (_stricmp(source_name, "ai") == 0) { ai_skipped++; }
                                else if (_stricmp(source_name, "user") == 0) { user_skipped++; }
                            }
                        }

                        if (*next == '\0') {
                            break;
                        }
                        cursor = next + 1;
                    }
                }
                if (input != NULL) {
                    HeapFree(GetProcessHeap(), 0, input);
                }
            }
            CloseHandle(file);
        }
    }

    if (retained == 0 && skipped == 0) {
        char rights_path[MAX_PATH] = {0};
        if (kbo_get_foreign_waiver_rights_path(rights_path, sizeof(rights_path))) {
            HANDLE file = CreateFileA(rights_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (file != INVALID_HANDLE_VALUE) {
                DWORD size = GetFileSize(file, NULL);
                if (size != INVALID_FILE_SIZE && size > 0u && size < 65536u) {
                    char input[65536] = {0};
                    DWORD read = 0;
                    if (ReadFile(file, input, size, &read, NULL) && read > 0u) {
                        input[read] = '\0';
                        const char* cursor = input;
                        while (*cursor != '\0') {
                            uint32_t player_id = 0u;
                            uint32_t team_id = 0u;
                            uint32_t league_id = 0u;
                            uint32_t retained_on = 0u;
                            uint32_t expires_on = 0u;
                            const char* p = cursor;
                            if (cursor[0] >= '0' && cursor[0] <= '9'
                                    && parse_u32_from_csv_field(&p, &player_id)
                                    && parse_u32_from_csv_field(&p, &team_id)
                                    && parse_u32_from_csv_field(&p, &league_id)
                                    && parse_u32_from_csv_field(&p, &retained_on)
                                    && parse_u32_from_csv_field(&p, &expires_on)
                                    && retained_on <= today_yyyymmdd
                                    && expires_on >= today_yyyymmdd) {
                                retained++;
                            }
                            while (*cursor != '\0' && *cursor != '\n') {
                                cursor++;
                            }
                            if (*cursor == '\n') {
                                cursor++;
                            }
                        }
                    }
                }
                CloseHandle(file);
            }
        }
    }

    char retained_text[16] = {0};
    char skipped_text[16] = {0};
    char ai_retained_text[16] = {0};
    char ai_skipped_text[16] = {0};
    char user_retained_text[16] = {0};
    char user_skipped_text[16] = {0};
    char active_rights_text[16] = {0};
    snprintf(retained_text, sizeof(retained_text), "%d", retained);
    snprintf(skipped_text, sizeof(skipped_text), "%d", skipped);
    snprintf(ai_retained_text, sizeof(ai_retained_text), "%d", ai_retained);
    snprintf(ai_skipped_text, sizeof(ai_skipped_text), "%d", ai_skipped);
    snprintf(user_retained_text, sizeof(user_retained_text), "%d", user_retained);
    snprintf(user_skipped_text, sizeof(user_skipped_text), "%d", user_skipped);
    snprintf(active_rights_text, sizeof(active_rights_text), "%d", active_rights);
    KboNewsTemplateVar vars[] = {
        { "retained", retained_text },
        { "skipped", skipped_text },
        { "ai_retained", ai_retained_text },
        { "ai_skipped", ai_skipped_text },
        { "user_retained", user_retained_text },
        { "user_skipped", user_skipped_text },
        { "active_rights", active_rights_text },
    };
    return kbo_news_template_render_key(
        decision_breakdown_available
            ? "foreign_waiver.results.body_with_breakdown"
            : "foreign_waiver.results.body_without_breakdown",
        vars,
        (int)(sizeof(vars) / sizeof(vars[0])),
        out,
        out_size,
        source);
}

int kbo_announce_foreign_waiver_results(uint32_t event_yyyymmdd, const char* source)
{
    if (event_yyyymmdd == 0u || g_kbo_foreign_waiver_last_result_announcement == event_yyyymmdd
            || kbo_foreign_waiver_announcement_recorded(event_yyyymmdd)) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        return 0;
    }

    char body[1024] = {0};
    kbo_load_foreign_waiver_rights();
    if (!kbo_build_foreign_waiver_result_body(body, sizeof(body), event_yyyymmdd, source)) {
        append_logf(
            "foreign reserve rights: result announcement skipped source=%s date=%u reason=body_template_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd);
        return 0;
    }

    char title[160] = {0};
    if (!kbo_news_template_render_key("foreign_waiver.results.title", NULL, 0, title, sizeof(title), source)) {
        append_logf(
            "foreign reserve rights: result announcement skipped source=%s date=%u reason=title_template_unavailable",
            source != NULL ? source : "",
            event_yyyymmdd);
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
    kbo_record_foreign_waiver_announcement(event_yyyymmdd);
    kbo_record_foreign_waiver_announcement_body(event_yyyymmdd, source, body);
    g_kbo_foreign_waiver_last_result_announcement = event_yyyymmdd;
    append_logf(
        "foreign reserve rights: result announcement source=%s date=%u created=%d mode=file_recorded_native_news_disabled body=%s",
        source != NULL ? source : "",
        event_yyyymmdd,
        created,
        body);
    return 1;
}
