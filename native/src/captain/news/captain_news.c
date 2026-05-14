#include "../internal/captain_selection_internal.h"

#include <stdarg.h>
#include <stdint.h>

#include "../../core/core_flags/api/flags_api.h"
#include "../../core/core_flags/json/json_bool_parser.h"
#include "../../core/news/live/core_live_news.h"

#define KBO_CAPTAIN_NEWS_TEMPLATE_DIR "news_templates"
#define KBO_CAPTAIN_NEWS_TEMPLATE_FILE "captain.json"

typedef struct KboCaptainNewsTemplates {
    char summary_title[192];
    char summary_intro[512];
    char summary_line[256];
    char summary_outro[512];
    char replacement_title[192];
    char replacement_body_departed[1024];
    char replacement_body_vacant[1024];
} KboCaptainNewsTemplates;

static int kbo_captain_news_file_exists(const char* path);
static int kbo_captain_news_load_templates_from_path(
    const char* path,
    uint32_t variant_seed,
    KboCaptainNewsTemplates* out);

static int kbo_captain_news_marker_path(char* out, size_t out_size)
{
    return kbo_get_save_scoped_data_file("captain_news_markers.txt", out, out_size);
}

static int kbo_captain_news_template_save_file_path(const char* file_name, char* out, size_t out_size)
{
    if (file_name == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    char relative[160] = {0};
    snprintf(
        relative,
        sizeof(relative),
        "%s\\%s\\%s",
        KBO_CAPTAIN_NEWS_TEMPLATE_DIR,
        kbo_custom_news_language_dir(),
        file_name);
    return kbo_get_save_scoped_data_file(relative, out, out_size);
}

static int kbo_captain_news_template_global_file_path(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\%s", local_app_data, file_name);
    return out[0] != '\0';
}

static int kbo_captain_news_template_global_split_path(char* out, size_t out_size)
{
    char relative[160] = {0};
    snprintf(
        relative,
        sizeof(relative),
        "%s\\%s\\%s",
        KBO_CAPTAIN_NEWS_TEMPLATE_DIR,
        kbo_custom_news_language_dir(),
        KBO_CAPTAIN_NEWS_TEMPLATE_FILE);
    return kbo_captain_news_template_global_file_path(relative, out, out_size);
}

static int kbo_captain_news_template_save_split_path(char* out, size_t out_size)
{
    return kbo_captain_news_template_save_file_path(KBO_CAPTAIN_NEWS_TEMPLATE_FILE, out, out_size);
}

static int kbo_captain_news_try_load_templates_from_existing_path(
    const char* path,
    uint32_t variant_seed,
    KboCaptainNewsTemplates* out,
    char* source_path,
    size_t source_path_size)
{
    if (path == NULL || !kbo_captain_news_file_exists(path)) {
        return 0;
    }
    if (!kbo_captain_news_load_templates_from_path(path, variant_seed, out)) {
        return 0;
    }
    if (source_path != NULL && source_path_size > 0u) {
        snprintf(source_path, source_path_size, "%s", path);
    }
    return 1;
}

static int kbo_captain_news_file_exists(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int kbo_captain_news_marker_exists(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_news_marker_path(path, sizeof(path))) {
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

    DWORD high = 0;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > (128u * 1024u)) {
        CloseHandle(file);
        return 0;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int found = 0;
    if (ReadFile(file, buffer, size, &read, NULL) && read > 0u) {
        buffer[read] = '\0';
        size_t key_len = strlen(key);
        char* cursor = buffer;
        char* end = buffer + read;
        while (cursor < end) {
            char* line_end = cursor;
            while (line_end < end && *line_end != '\r' && *line_end != '\n') {
                line_end++;
            }
            size_t line_len = (size_t)(line_end - cursor);
            if (line_len == key_len && memcmp(cursor, key, key_len) == 0) {
                found = 1;
                break;
            }
            while (line_end < end && (*line_end == '\r' || *line_end == '\n')) {
                line_end++;
            }
            cursor = line_end;
        }
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    CloseHandle(file);
    return found;
}

static void kbo_captain_news_persist_marker(const char* key, const char* source)
{
    if (key == NULL || key[0] == '\0' || kbo_captain_news_marker_exists(key)) {
        return;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_captain_news_marker_path(path, sizeof(path))) {
        append_logf(
            "KBO captain news marker skipped source=%s key=%s reason=path_unavailable",
            source != NULL ? source : "",
            key);
        return;
    }

    HANDLE file = CreateFileA(
        path,
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf(
            "KBO captain news marker skipped source=%s key=%s reason=open_failed gle=%lu path=%s",
            source != NULL ? source : "",
            key,
            (unsigned long)GetLastError(),
            path);
        return;
    }

    char line[256] = {0};
    int len = snprintf(line, sizeof(line), "%s\r\n", key);
    DWORD written = 0;
    if (len <= 0 || len >= (int)sizeof(line)
            || !WriteFile(file, line, (DWORD)len, &written, NULL)
            || written != (DWORD)len) {
        append_logf(
            "KBO captain news marker write failed source=%s key=%s gle=%lu path=%s",
            source != NULL ? source : "",
            key,
            (unsigned long)GetLastError(),
            path);
    }
    CloseHandle(file);
}

static void kbo_captain_news_append(char* out, size_t out_size, const char* fmt, ...)
{
    if (out == NULL || out_size == 0u || fmt == NULL) {
        return;
    }
    size_t used = strlen(out);
    if (used >= out_size - 1u) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(out + used, out_size - used, fmt, args);
    va_end(args);
    out[out_size - 1u] = '\0';
}

static void kbo_captain_news_append_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u || text == NULL) {
        return;
    }
    size_t used = strlen(out);
    if (used >= out_size - 1u) {
        return;
    }
    size_t remaining = out_size - used - 1u;
    size_t len = strlen(text);
    if (len > remaining) {
        len = remaining;
    }
    memcpy(out + used, text, len);
    out[used + len] = '\0';
}

static void kbo_captain_news_append_char(char* out, size_t out_size, char ch)
{
    char text[2] = {ch, '\0'};
    kbo_captain_news_append_text(out, out_size, text);
}

static const char* kbo_captain_news_player_name(const KboCaptainSelectionRow* row)
{
    if (row != NULL && row->player_name[0] != '\0') {
        return row->player_name;
    }
    return "the new captain";
}

static const char* kbo_captain_news_team_name(const KboCaptainSelectionRow* row)
{
    if (row != NULL && row->team_name[0] != '\0') {
        return row->team_name;
    }
    return "Team";
}

static void kbo_captain_news_append_team_link(
    char* out,
    size_t out_size,
    const KboCaptainSelectionRow* row)
{
    if (row != NULL && row->team_id != 0u) {
        kbo_captain_news_append(out, out_size, "<%s:team#%u>", kbo_captain_news_team_name(row), row->team_id);
    } else {
        kbo_captain_news_append(out, out_size, "%s", kbo_captain_news_team_name(row));
    }
}

static void kbo_captain_news_append_player_link(
    char* out,
    size_t out_size,
    const KboCaptainSelectionRow* row)
{
    if (row != NULL && row->player_id != 0u) {
        kbo_captain_news_append(out, out_size, "<%s:player#%u>", kbo_captain_news_player_name(row), row->player_id);
    } else {
        kbo_captain_news_append(out, out_size, "%s", kbo_captain_news_player_name(row));
    }
}

static int kbo_captain_news_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static int kbo_captain_news_json_put_char(char** cursor, size_t* remaining, char ch)
{
    if (cursor == NULL || *cursor == NULL || remaining == NULL || *remaining == 0u) {
        return 0;
    }
    **cursor = ch;
    (*cursor)++;
    (*remaining)--;
    **cursor = '\0';
    return 1;
}

static int kbo_captain_news_json_put_utf8(char** cursor, size_t* remaining, unsigned int codepoint)
{
    if (codepoint == 0u) {
        codepoint = '?';
    }
    if (codepoint <= 0x7Fu) {
        return kbo_captain_news_json_put_char(cursor, remaining, (char)codepoint);
    }
    if (codepoint <= 0x7FFu) {
        return kbo_captain_news_json_put_char(cursor, remaining, (char)(0xC0u | (codepoint >> 6)))
            && kbo_captain_news_json_put_char(cursor, remaining, (char)(0x80u | (codepoint & 0x3Fu)));
    }
    return kbo_captain_news_json_put_char(cursor, remaining, (char)(0xE0u | (codepoint >> 12)))
        && kbo_captain_news_json_put_char(cursor, remaining, (char)(0x80u | ((codepoint >> 6) & 0x3Fu)))
        && kbo_captain_news_json_put_char(cursor, remaining, (char)(0x80u | (codepoint & 0x3Fu)));
}

static int kbo_captain_news_json_string_value(const char* start, const char* end, char* out, size_t out_size)
{
    if (start == NULL || end == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    start = kbo_json_skip_ws(start, end);
    if (start >= end || *start != '"') {
        return 0;
    }

    const char* stop = kbo_json_find_string_end(start, end);
    if (stop == NULL || stop > end) {
        return 0;
    }

    char* cursor = out;
    size_t remaining = out_size - 1u;
    for (const char* p = start + 1; p < stop; p++) {
        if (*p != '\\') {
            if (!kbo_captain_news_json_put_char(&cursor, &remaining, *p)) {
                return 0;
            }
            continue;
        }

        p++;
        if (p >= stop) {
            return 0;
        }
        switch (*p) {
        case '"':
        case '\\':
        case '/':
            if (!kbo_captain_news_json_put_char(&cursor, &remaining, *p)) {
                return 0;
            }
            break;
        case 'b':
            if (!kbo_captain_news_json_put_char(&cursor, &remaining, '\b')) {
                return 0;
            }
            break;
        case 'f':
            if (!kbo_captain_news_json_put_char(&cursor, &remaining, '\f')) {
                return 0;
            }
            break;
        case 'n':
            if (!kbo_captain_news_json_put_char(&cursor, &remaining, '\n')) {
                return 0;
            }
            break;
        case 'r':
            if (!kbo_captain_news_json_put_char(&cursor, &remaining, '\r')) {
                return 0;
            }
            break;
        case 't':
            if (!kbo_captain_news_json_put_char(&cursor, &remaining, '\t')) {
                return 0;
            }
            break;
        case 'u': {
            if (stop - p < 5) {
                return 0;
            }
            unsigned int codepoint = 0u;
            for (int i = 1; i <= 4; i++) {
                int hex = kbo_captain_news_hex_value(p[i]);
                if (hex < 0) {
                    return 0;
                }
                codepoint = (codepoint << 4) | (unsigned int)hex;
            }
            if (!kbo_captain_news_json_put_utf8(&cursor, &remaining, codepoint)) {
                return 0;
            }
            p += 4;
            break;
        }
        default:
            return 0;
        }
    }
    *cursor = '\0';
    return 1;
}

static char* kbo_captain_news_read_json_file(const char* path, DWORD* out_size)
{
    if (path == NULL || path[0] == '\0' || out_size == NULL) {
        return NULL;
    }
    *out_size = 0u;

    HANDLE file = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    DWORD high = 0;
    DWORD size = GetFileSize(file, &high);
    if (size == INVALID_FILE_SIZE || high != 0u || size == 0u || size > KBO_FLAGS_JSON_MAX_BYTES) {
        CloseHandle(file);
        return NULL;
    }

    char* buffer = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buffer == NULL) {
        CloseHandle(file);
        return NULL;
    }

    DWORD read = 0;
    if (!ReadFile(file, buffer, size, &read, NULL) || read != size) {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);

    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

static int kbo_captain_news_read_template_value(
    const char* json,
    DWORD json_size,
    const char* key,
    uint32_t variant_seed,
    char* out,
    size_t out_size)
{
    const char* start = NULL;
    const char* end = NULL;
    char count_key[192] = {0};
    snprintf(count_key, sizeof(count_key), "%s.variants", key);
    int variant_count = 0;
    if (kbo_find_int_value_in_json(json, json_size, count_key, &variant_count) && variant_count > 1) {
        if (variant_count > 16) {
            variant_count = 16;
        }
        char variant_key[192] = {0};
        snprintf(variant_key, sizeof(variant_key), "%s.v%u", key, (variant_seed % (uint32_t)variant_count) + 1u);
        if (kbo_find_json_value_span(json, json_size, variant_key, &start, &end)
                && kbo_captain_news_json_string_value(start, end, out, out_size)) {
            return 1;
        }
    }

    start = NULL;
    end = NULL;
    return kbo_find_json_value_span(json, json_size, key, &start, &end)
        && kbo_captain_news_json_string_value(start, end, out, out_size);
}

static int kbo_captain_news_load_templates_from_path(const char* path, uint32_t variant_seed, KboCaptainNewsTemplates* out)
{
    if (path == NULL || out == NULL) {
        return 0;
    }

    DWORD json_size = 0u;
    char* json = kbo_captain_news_read_json_file(path, &json_size);
    if (json == NULL) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    int ok = kbo_captain_news_read_template_value(
            json,
            json_size,
            "captain.summary.title",
            variant_seed,
            out->summary_title,
            sizeof(out->summary_title))
        && kbo_captain_news_read_template_value(
            json,
            json_size,
            "captain.summary.intro",
            variant_seed,
            out->summary_intro,
            sizeof(out->summary_intro))
        && kbo_captain_news_read_template_value(
            json,
            json_size,
            "captain.summary.line",
            variant_seed,
            out->summary_line,
            sizeof(out->summary_line))
        && kbo_captain_news_read_template_value(
            json,
            json_size,
            "captain.summary.outro",
            variant_seed,
            out->summary_outro,
            sizeof(out->summary_outro))
        && kbo_captain_news_read_template_value(
            json,
            json_size,
            "captain.replacement.title",
            variant_seed,
            out->replacement_title,
            sizeof(out->replacement_title))
        && kbo_captain_news_read_template_value(
            json,
            json_size,
            "captain.replacement.body_departed",
            variant_seed,
            out->replacement_body_departed,
            sizeof(out->replacement_body_departed))
        && kbo_captain_news_read_template_value(
            json,
            json_size,
            "captain.replacement.body_vacant",
            variant_seed,
            out->replacement_body_vacant,
            sizeof(out->replacement_body_vacant));

    HeapFree(GetProcessHeap(), 0, json);
    return ok;
}

static int kbo_captain_news_load_templates(
    KboCaptainNewsTemplates* out,
    char* source_path,
    size_t source_path_size,
    uint32_t variant_seed,
    const char* source)
{
    if (out == NULL) {
        return 0;
    }
    if (source_path != NULL && source_path_size > 0u) {
        source_path[0] = '\0';
    }

    char save_split_path[MAX_PATH] = {0};
    char global_split_path[MAX_PATH] = {0};
    int has_save_split_path = kbo_captain_news_template_save_split_path(save_split_path, sizeof(save_split_path));
    int has_global_split_path = kbo_captain_news_template_global_split_path(global_split_path, sizeof(global_split_path));

    if (has_save_split_path && kbo_captain_news_file_exists(save_split_path)) {
        if (kbo_captain_news_try_load_templates_from_existing_path(
                save_split_path,
                variant_seed,
                out,
                source_path,
                source_path_size)) {
            return 1;
        }
        append_logf(
            "KBO captain news templates invalid source=%s scope=save_split path=%s",
            source != NULL ? source : "",
            save_split_path);
    }

    if (has_global_split_path && kbo_captain_news_file_exists(global_split_path)) {
        if (kbo_captain_news_try_load_templates_from_existing_path(
                global_split_path,
                variant_seed,
                out,
                source_path,
                source_path_size)) {
            return 1;
        }
        append_logf(
            "KBO captain news templates invalid source=%s scope=global_split path=%s",
            source != NULL ? source : "",
            global_split_path);
    }

    append_logf(
        "KBO captain news templates unavailable source=%s lang=%s save_split=%s global_split=%s",
        source != NULL ? source : "",
        kbo_custom_news_language_dir(),
        has_save_split_path ? save_split_path : "",
        has_global_split_path ? global_split_path : "");
    return 0;
}

static int kbo_captain_news_token_equals(const char* token, size_t token_len, const char* expected)
{
    return token != NULL
        && expected != NULL
        && strlen(expected) == token_len
        && memcmp(token, expected, token_len) == 0;
}

static void kbo_captain_news_append_token(
    char* out,
    size_t out_size,
    const char* token,
    size_t token_len,
    uint32_t season,
    const KboCaptainSelectionRow* row,
    const KboCaptainSelectionRow* old_row)
{
    if (kbo_captain_news_token_equals(token, token_len, "season")) {
        kbo_captain_news_append(out, out_size, "%u", season);
    } else if (kbo_captain_news_token_equals(token, token_len, "team_name")) {
        kbo_captain_news_append_text(out, out_size, kbo_captain_news_team_name(row));
    } else if (kbo_captain_news_token_equals(token, token_len, "team_link")) {
        kbo_captain_news_append_team_link(out, out_size, row);
    } else if (kbo_captain_news_token_equals(token, token_len, "player_name")
            || kbo_captain_news_token_equals(token, token_len, "new_player_name")) {
        kbo_captain_news_append_text(out, out_size, kbo_captain_news_player_name(row));
    } else if (kbo_captain_news_token_equals(token, token_len, "player_link")
            || kbo_captain_news_token_equals(token, token_len, "new_player_link")) {
        kbo_captain_news_append_player_link(out, out_size, row);
    } else if (kbo_captain_news_token_equals(token, token_len, "old_player_name")) {
        kbo_captain_news_append_text(out, out_size, kbo_captain_news_player_name(old_row));
    } else if (kbo_captain_news_token_equals(token, token_len, "old_player_link")) {
        kbo_captain_news_append_player_link(out, out_size, old_row);
    } else {
        kbo_captain_news_append_char(out, out_size, '{');
        for (size_t i = 0; i < token_len; i++) {
            kbo_captain_news_append_char(out, out_size, token[i]);
        }
        kbo_captain_news_append_char(out, out_size, '}');
    }
}

static void kbo_captain_news_render_template(
    const char* tmpl,
    uint32_t season,
    const KboCaptainSelectionRow* row,
    const KboCaptainSelectionRow* old_row,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (tmpl == NULL) {
        return;
    }

    for (const char* p = tmpl; *p != '\0'; p++) {
        if (*p != '{') {
            kbo_captain_news_append_char(out, out_size, *p);
            continue;
        }

        const char* close = strchr(p + 1, '}');
        if (close == NULL) {
            kbo_captain_news_append_char(out, out_size, *p);
            continue;
        }

        kbo_captain_news_append_token(
            out,
            out_size,
            p + 1,
            (size_t)(close - (p + 1)),
            season,
            row,
            old_row);
        p = close;
    }
}

static int kbo_captain_news_ends_with_newline(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    size_t len = strlen(text);
    return len > 0u && (text[len - 1u] == '\n' || text[len - 1u] == '\r');
}

int kbo_emit_captain_initial_selection_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source)
{
    if (date == 0u || season < 1982u || season > 2200u || league_id == 0u || rows == NULL || row_count <= 0) {
        return 0;
    }

    char marker[128] = {0};
    snprintf(marker, sizeof(marker), "summary|%u|%u", season, league_id);
    if (kbo_captain_news_marker_exists(marker)) {
        return 0;
    }

    KboCaptainNewsTemplates templates;
    char template_path[MAX_PATH] = {0};
    uint32_t variant_seed = season ^ league_id;
    if (!kbo_captain_news_load_templates(&templates, template_path, sizeof(template_path), variant_seed, source)) {
        append_logf(
            "KBO captain summary news skipped source=%s season=%u league_id=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }

    int listed = 0;
    char body[8192] = {0};
    char rendered[1024] = {0};
    kbo_captain_news_render_template(
        templates.summary_intro,
        season,
        NULL,
        NULL,
        rendered,
        sizeof(rendered));
    if (rendered[0] != '\0') {
        kbo_captain_news_append_text(body, sizeof(body), rendered);
        kbo_captain_news_append(body, sizeof(body), "\n\n");
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id == 0u || rows[i].player_id == 0u) {
            continue;
        }
        kbo_captain_news_render_template(
            templates.summary_line,
            season,
            &rows[i],
            NULL,
            rendered,
            sizeof(rendered));
        kbo_captain_news_append_text(body, sizeof(body), rendered);
        if (!kbo_captain_news_ends_with_newline(rendered)) {
            kbo_captain_news_append(body, sizeof(body), "\n");
        }
        listed++;
    }
    if (listed <= 0) {
        append_logf(
            "KBO captain summary news skipped source=%s season=%u league_id=%u reason=no_listed_captains",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }
    kbo_captain_news_render_template(
        templates.summary_outro,
        season,
        NULL,
        NULL,
        rendered,
        sizeof(rendered));
    if (rendered[0] != '\0') {
        if (!kbo_captain_news_ends_with_newline(body)) {
            kbo_captain_news_append(body, sizeof(body), "\n");
        }
        kbo_captain_news_append(body, sizeof(body), "\n");
        kbo_captain_news_append_text(body, sizeof(body), rendered);
    }

    char title[160] = {0};
    kbo_captain_news_render_template(
        templates.summary_title,
        season,
        NULL,
        NULL,
        title,
        sizeof(title));
    int created = create_kbo_native_live_news_with_body(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_captain_news_persist_marker(marker, source);
    }
    append_logf(
        "KBO captain summary news source=%s date=%u season=%u league_id=%u listed=%d created=%d template=%s",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        listed,
        created,
        template_path);
    return created;
}

int kbo_emit_captain_replacement_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* old_row,
    const KboCaptainSelectionRow* new_row,
    const char* source)
{
    if (date == 0u || season < 1982u || season > 2200u || league_id == 0u
            || new_row == NULL || new_row->team_id == 0u || new_row->player_id == 0u) {
        return 0;
    }

    uint32_t old_player_id = old_row != NULL ? old_row->player_id : 0u;
    char marker[160] = {0};
    snprintf(
        marker,
        sizeof(marker),
        "replacement|%u|%u|%u|%u|%u",
        season,
        league_id,
        new_row->team_id,
        old_player_id,
        new_row->player_id);
    if (kbo_captain_news_marker_exists(marker)) {
        return 0;
    }

    KboCaptainNewsTemplates templates;
    char template_path[MAX_PATH] = {0};
    uint32_t variant_seed = season ^ league_id ^ new_row->team_id ^ old_player_id ^ new_row->player_id;
    if (!kbo_captain_news_load_templates(&templates, template_path, sizeof(template_path), variant_seed, source)) {
        append_logf(
            "KBO captain replacement news skipped source=%s season=%u league_id=%u team=%u old_player=%u new_player=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id,
            new_row->team_id,
            old_player_id,
            new_row->player_id);
        return 0;
    }

    char title[180] = {0};
    kbo_captain_news_render_template(
        templates.replacement_title,
        season,
        new_row,
        old_row,
        title,
        sizeof(title));

    char body[2048] = {0};
    kbo_captain_news_render_template(
        old_row != NULL && old_row->player_id != 0u
            ? templates.replacement_body_departed
            : templates.replacement_body_vacant,
        season,
        new_row,
        old_row,
        body,
        sizeof(body));

    int created = create_kbo_native_live_news_with_body(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_captain_news_persist_marker(marker, source);
    }
    append_logf(
        "KBO captain replacement news source=%s date=%u season=%u league_id=%u team=%u old_player=%u new_player=%u created=%d template=%s",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        new_row->team_id,
        old_player_id,
        new_row->player_id,
        created,
        template_path);
    return created;
}
