#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core_news_templates.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../core_flags/api/flags_api.h"
#include "../../core_flags/json/json_bool_parser.h"
#include "../../files/save_paths/core_save_paths.h"
#include "../../logging/core_log.h"

static int kbo_news_template_file_exists(const char* path);
static int kbo_news_template_load_from_path(const char* path, const char* key, char* out, size_t out_size);

static int kbo_news_template_starts_with(const char* text, const char* prefix)
{
    if (text == NULL || prefix == NULL) {
        return 0;
    }
    size_t prefix_len = strlen(prefix);
    return strncmp(text, prefix, prefix_len) == 0;
}

static const char* kbo_news_template_file_for_key(const char* key)
{
    if (kbo_news_template_starts_with(key, "captain.")) {
        return "captain.json";
    }
    if (kbo_news_template_starts_with(key, "cbt.")) {
        return "competitive_balance_tax.json";
    }
    if (kbo_news_template_starts_with(key, "military.")) {
        return "military_service.json";
    }
    if (kbo_news_template_starts_with(key, "asian_games.")) {
        return "asian_games.json";
    }
    if (kbo_news_template_starts_with(key, "fa_compensation.")) {
        return "fa_compensation.json";
    }
    if (kbo_news_template_starts_with(key, "foreign_waiver.")) {
        return "foreign_waiver.json";
    }
    if (kbo_news_template_starts_with(key, "foreign_injury.")) {
        return "foreign_injury.json";
    }
    if (kbo_news_template_starts_with(key, "custom_event.")) {
        return "custom_events.json";
    }
    return NULL;
}

static const char* kbo_news_template_normalize_language_dir(const char* language_dir)
{
    return language_dir != NULL && strcmp(language_dir, "en") == 0 ? "en" : "ko";
}

static int kbo_text_resource_save_path_for_language(
    const char* resource_dir,
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    if (resource_dir == NULL || resource_dir[0] == '\0' || file_name == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    char relative[160] = {0};
    snprintf(
        relative,
        sizeof(relative),
        "%s\\%s\\%s",
        resource_dir,
        kbo_news_template_normalize_language_dir(language_dir),
        file_name);
    return kbo_get_save_scoped_data_file(relative, out, out_size);
}

static int kbo_news_template_save_split_path_for_language(
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    return kbo_text_resource_save_path_for_language(
        KBO_NEWS_TEMPLATES_DIR,
        language_dir,
        file_name,
        out,
        out_size);
}

static int kbo_news_template_save_split_path(const char* file_name, char* out, size_t out_size)
{
    return kbo_news_template_save_split_path_for_language(
        kbo_custom_news_language_dir(),
        file_name,
        out,
        out_size);
}

static int kbo_news_template_global_file_path(const char* file_name, char* out, size_t out_size)
{
    if (out == NULL || out_size < 2u) {
        return 0;
    }
    out[0] = '\0';
    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0u || got >= sizeof(local_app_data)) {
        return 0;
    }
    snprintf(out, out_size, "%s\\OOTP-KBO\\%s", local_app_data, file_name);
    return out[0] != '\0';
}

static int kbo_text_resource_global_path_for_language(
    const char* resource_dir,
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    if (resource_dir == NULL || resource_dir[0] == '\0' || file_name == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    char relative[160] = {0};
    snprintf(
        relative,
        sizeof(relative),
        "%s\\%s\\%s",
        resource_dir,
        kbo_news_template_normalize_language_dir(language_dir),
        file_name);
    return kbo_news_template_global_file_path(relative, out, out_size);
}

static int kbo_news_template_global_split_path_for_language(
    const char* language_dir,
    const char* file_name,
    char* out,
    size_t out_size)
{
    return kbo_text_resource_global_path_for_language(
        KBO_NEWS_TEMPLATES_DIR,
        language_dir,
        file_name,
        out,
        out_size);
}

static int kbo_news_template_global_split_path(const char* file_name, char* out, size_t out_size)
{
    return kbo_news_template_global_split_path_for_language(
        kbo_custom_news_language_dir(),
        file_name,
        out,
        out_size);
}

static int kbo_news_template_try_load_from_existing_path(
    const char* path,
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size)
{
    if (path == NULL || !kbo_news_template_file_exists(path)) {
        return 0;
    }
    if (!kbo_news_template_load_from_path(path, key, out, out_size)) {
        return 0;
    }
    if (source_path != NULL && source_path_size > 0u) {
        snprintf(source_path, source_path_size, "%s", path);
    }
    return out[0] != '\0';
}

static int kbo_news_template_file_exists(const char* path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void kbo_news_text_append(char* out, size_t out_size, const char* text)
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

void kbo_news_text_appendf(char* out, size_t out_size, const char* fmt, ...)
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

static void kbo_news_text_append_char(char* out, size_t out_size, char ch)
{
    char text[2] = { ch, '\0' };
    kbo_news_text_append(out, out_size, text);
}

int kbo_news_text_ends_with_newline(const char* text)
{
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    size_t len = strlen(text);
    return len > 0u && (text[len - 1u] == '\n' || text[len - 1u] == '\r');
}

static char* kbo_news_template_read_json_file(const char* path, DWORD* out_size)
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

    DWORD high = 0u;
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

    DWORD read = 0u;
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

static int kbo_news_template_load_from_path(const char* path, const char* key, char* out, size_t out_size)
{
    if (path == NULL || key == NULL || out == NULL || out_size == 0u) {
        return 0;
    }

    DWORD json_size = 0u;
    char* json = kbo_news_template_read_json_file(path, &json_size);
    if (json == NULL) {
        return 0;
    }

    const char* start = NULL;
    const char* end = NULL;
    int ok = kbo_find_json_value_span(json, json_size, key, &start, &end)
        && kbo_json_string_value_at(start, end, out, out_size);

    HeapFree(GetProcessHeap(), 0, json);
    return ok;
}

static int kbo_news_template_load_int_from_path(const char* path, const char* key, int* out_value)
{
    if (path == NULL || key == NULL || out_value == NULL) {
        return 0;
    }

    DWORD json_size = 0u;
    char* json = kbo_news_template_read_json_file(path, &json_size);
    if (json == NULL) {
        return 0;
    }

    int ok = kbo_find_int_value_in_json(json, json_size, key, out_value);

    HeapFree(GetProcessHeap(), 0, json);
    return ok;
}

static int kbo_news_template_try_load_int(const char* key, int* out_value)
{
    if (key == NULL || out_value == NULL) {
        return 0;
    }
    *out_value = 0;

    const char* split_file = kbo_news_template_file_for_key(key);
    if (split_file != NULL) {
        char save_split_path[MAX_PATH] = {0};
        char global_split_path[MAX_PATH] = {0};
        if (kbo_news_template_save_split_path(split_file, save_split_path, sizeof(save_split_path))
                && kbo_news_template_file_exists(save_split_path)
                && kbo_news_template_load_int_from_path(save_split_path, key, out_value)) {
            return 1;
        }
        if (kbo_news_template_global_split_path(split_file, global_split_path, sizeof(global_split_path))
                && kbo_news_template_file_exists(global_split_path)
                && kbo_news_template_load_int_from_path(global_split_path, key, out_value)) {
            return 1;
        }
    }
    return 0;
}

int kbo_news_template_load_for_language(
    const char* language_dir,
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    if (out == NULL || out_size == 0u || key == NULL || key[0] == '\0') {
        return 0;
    }
    out[0] = '\0';
    if (source_path != NULL && source_path_size > 0u) {
        source_path[0] = '\0';
    }

    const char* split_file = kbo_news_template_file_for_key(key);
    char save_split_path[MAX_PATH] = {0};
    char global_split_path[MAX_PATH] = {0};
    int has_save_split_path = 0;
    int has_global_split_path = 0;
    const char* normalized_language_dir = kbo_news_template_normalize_language_dir(language_dir);
    if (split_file != NULL) {
        has_save_split_path = kbo_news_template_save_split_path_for_language(
            normalized_language_dir,
            split_file,
            save_split_path,
            sizeof(save_split_path));
        has_global_split_path = kbo_news_template_global_split_path_for_language(
            normalized_language_dir,
            split_file,
            global_split_path,
            sizeof(global_split_path));
        if (has_save_split_path && kbo_news_template_file_exists(save_split_path)) {
            if (kbo_news_template_try_load_from_existing_path(
                    save_split_path,
                    key,
                    out,
                    out_size,
                    source_path,
                    source_path_size)) {
                return 1;
            }
            append_logf(
                "KBO news template missing or invalid source=%s scope=save_split key=%s path=%s",
                source != NULL ? source : "",
                key,
                save_split_path);
        }
        if (has_global_split_path && kbo_news_template_file_exists(global_split_path)) {
            if (kbo_news_template_try_load_from_existing_path(
                    global_split_path,
                    key,
                    out,
                    out_size,
                    source_path,
                    source_path_size)) {
                return 1;
            }
            append_logf(
                "KBO news template missing or invalid source=%s scope=global_split key=%s path=%s",
                source != NULL ? source : "",
                key,
                global_split_path);
        }
    }

    append_logf(
        "KBO news template unavailable source=%s key=%s lang=%s save_split=%s global_split=%s",
        source != NULL ? source : "",
        key,
        normalized_language_dir,
        has_save_split_path ? save_split_path : "",
        has_global_split_path ? global_split_path : "");
    return 0;
}

int kbo_text_resource_load_for_language(
    const char* resource_dir,
    const char* language_dir,
    const char* file_name,
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    if (out == NULL || out_size == 0u || resource_dir == NULL || resource_dir[0] == '\0'
            || file_name == NULL || file_name[0] == '\0' || key == NULL || key[0] == '\0') {
        return 0;
    }
    out[0] = '\0';
    if (source_path != NULL && source_path_size > 0u) {
        source_path[0] = '\0';
    }

    char save_path[MAX_PATH] = {0};
    char global_path[MAX_PATH] = {0};
    const char* normalized_language_dir = kbo_news_template_normalize_language_dir(language_dir);
    int has_save_path = kbo_text_resource_save_path_for_language(
        resource_dir,
        normalized_language_dir,
        file_name,
        save_path,
        sizeof(save_path));
    int has_global_path = kbo_text_resource_global_path_for_language(
        resource_dir,
        normalized_language_dir,
        file_name,
        global_path,
        sizeof(global_path));

    if (has_save_path && kbo_news_template_file_exists(save_path)) {
        if (kbo_news_template_try_load_from_existing_path(
                save_path,
                key,
                out,
                out_size,
                source_path,
                source_path_size)) {
            return 1;
        }
        append_logf(
            "KBO text resource missing or invalid source=%s scope=save key=%s path=%s",
            source != NULL ? source : "",
            key,
            save_path);
    }
    if (has_global_path && kbo_news_template_file_exists(global_path)) {
        if (kbo_news_template_try_load_from_existing_path(
                global_path,
                key,
                out,
                out_size,
                source_path,
                source_path_size)) {
            return 1;
        }
        append_logf(
            "KBO text resource missing or invalid source=%s scope=global key=%s path=%s",
            source != NULL ? source : "",
            key,
            global_path);
    }
    return 0;
}

int kbo_news_template_load(
    const char* key,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    return kbo_news_template_load_for_language(
        kbo_custom_news_language_dir(),
        key,
        out,
        out_size,
        source_path,
        source_path_size,
        source);
}

static int kbo_news_template_token_equals(const char* token, size_t token_len, const char* expected)
{
    return token != NULL
        && expected != NULL
        && strlen(expected) == token_len
        && memcmp(token, expected, token_len) == 0;
}

static const char* kbo_news_template_find_var(
    const KboNewsTemplateVar* vars,
    int var_count,
    const char* token,
    size_t token_len)
{
    if (vars == NULL || var_count <= 0 || token == NULL) {
        return NULL;
    }
    for (int i = 0; i < var_count; i++) {
        if (vars[i].key != NULL && kbo_news_template_token_equals(token, token_len, vars[i].key)) {
            return vars[i].value != NULL ? vars[i].value : "";
        }
    }
    return NULL;
}

static uint32_t kbo_news_template_hash_text(uint32_t hash, const char* text)
{
    if (text == NULL) {
        text = "";
    }
    while (*text != '\0') {
        hash ^= (uint8_t)*text;
        hash *= 16777619u;
        text++;
    }
    return hash;
}

static uint32_t kbo_news_template_variant_seed(const char* key, const KboNewsTemplateVar* vars, int var_count)
{
    uint32_t hash = 2166136261u;
    hash = kbo_news_template_hash_text(hash, key);
    hash = kbo_news_template_hash_text(hash, "|");
    if (vars == NULL || var_count <= 0) {
        return hash;
    }
    for (int i = 0; i < var_count; i++) {
        hash = kbo_news_template_hash_text(hash, vars[i].key);
        hash = kbo_news_template_hash_text(hash, "=");
        hash = kbo_news_template_hash_text(hash, vars[i].value);
        hash = kbo_news_template_hash_text(hash, ";");
    }
    return hash;
}

static int kbo_news_template_variant_key(const char* key, const KboNewsTemplateVar* vars, int var_count, char* out, size_t out_size)
{
    if (key == NULL || out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';

    char count_key[192] = {0};
    snprintf(count_key, sizeof(count_key), "%s.variants", key);

    int variant_count = 0;
    if (!kbo_news_template_try_load_int(count_key, &variant_count) || variant_count <= 1) {
        return 0;
    }
    if (variant_count > 16) {
        variant_count = 16;
    }

    uint32_t seed = kbo_news_template_variant_seed(key, vars, var_count);
    int variant = (int)(seed % (uint32_t)variant_count) + 1;
    snprintf(out, out_size, "%s.v%d", key, variant);
    return out[0] != '\0';
}

int kbo_news_template_render(
    const char* tmpl,
    const KboNewsTemplateVar* vars,
    int var_count,
    char* out,
    size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (tmpl == NULL) {
        return 0;
    }

    for (const char* p = tmpl; *p != '\0'; p++) {
        if (*p != '{') {
            kbo_news_text_append_char(out, out_size, *p);
            continue;
        }

        const char* close = strchr(p + 1, '}');
        if (close == NULL) {
            kbo_news_text_append_char(out, out_size, *p);
            continue;
        }

        const char* value = kbo_news_template_find_var(vars, var_count, p + 1, (size_t)(close - (p + 1)));
        if (value != NULL) {
            kbo_news_text_append(out, out_size, value);
        } else {
            kbo_news_text_append_char(out, out_size, '{');
            for (const char* q = p + 1; q < close; q++) {
                kbo_news_text_append_char(out, out_size, *q);
            }
            kbo_news_text_append_char(out, out_size, '}');
        }
        p = close;
    }
    return 1;
}

int kbo_news_template_render_key(
    const char* key,
    const KboNewsTemplateVar* vars,
    int var_count,
    char* out,
    size_t out_size,
    const char* source)
{
    char tmpl[4096] = {0};
    char variant_key[192] = {0};
    if (kbo_news_template_variant_key(key, vars, var_count, variant_key, sizeof(variant_key))
            && kbo_news_template_load(variant_key, tmpl, sizeof(tmpl), NULL, 0u, source)) {
        return kbo_news_template_render(tmpl, vars, var_count, out, out_size);
    }
    if (!kbo_news_template_load(key, tmpl, sizeof(tmpl), NULL, 0u, source)) {
        if (out != NULL && out_size > 0u) {
            out[0] = '\0';
        }
        return 0;
    }
    return kbo_news_template_render(tmpl, vars, var_count, out, out_size);
}
