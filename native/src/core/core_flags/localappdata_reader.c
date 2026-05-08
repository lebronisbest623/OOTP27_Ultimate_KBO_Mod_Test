#include "localappdata_reader.h"

#include "json_bool_parser.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

int kbo_read_localappdata_json_flag_value(const char* key, const char* legacy_key, int* out_value)
{
    if (key == NULL || key[0] == '\0' || out_value == NULL) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\OOTP-KBO\\kbo_flags.json", local_app_data);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size > KBO_FLAGS_JSON_MAX_BYTES) {
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
    if (ReadFile(file, buffer, size, &read, NULL)) {
        found = kbo_find_flag_value_in_json(buffer, read, key, legacy_key, out_value);
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static int kbo_get_localappdata_flags_json_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    snprintf(out, out_size, "%s\\OOTP-KBO\\kbo_flags.json", local_app_data);
    return 1;
}

int kbo_read_localappdata_json_int_value(const char* key, int* out_value)
{
    if (key == NULL || key[0] == '\0' || out_value == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_localappdata_flags_json_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size > KBO_FLAGS_JSON_MAX_BYTES) {
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
    if (ReadFile(file, buffer, size, &read, NULL)) {
        found = kbo_find_int_value_in_json(buffer, read, key, out_value);
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buffer);
    return found;
}

static const char* kbo_json_value_token_end(const char* value, const char* end)
{
    value = kbo_json_skip_ws(value, end);
    if (value >= end) {
        return value;
    }
    if (*value == '"') {
        const char* stop = kbo_json_find_string_end(value, end);
        return stop != NULL ? stop + 1 : value;
    }
    const char* p = value;
    while (p < end && *p != ',' && *p != '}') {
        p++;
    }
    while (p > value && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\r' || p[-1] == '\n')) {
        p--;
    }
    return p;
}

static int kbo_find_json_value_span(const char* json, DWORD json_size, const char* key, const char** out_start, const char** out_end)
{
    if (json == NULL || key == NULL || out_start == NULL || out_end == NULL) {
        return 0;
    }

    const char* p = json;
    const char* end = json + json_size;
    while (p < end) {
        if (*p != '"') {
            p++;
            continue;
        }

        const char* key_start = p + 1;
        const char* key_stop = kbo_json_find_string_end(p, end);
        if (key_stop == NULL) {
            return 0;
        }

        int matched = kbo_json_string_equals_key(key_start, key_stop, key);
        p = key_stop + 1;
        if (!matched) {
            continue;
        }

        const char* colon = kbo_json_skip_ws(p, end);
        if (colon >= end || *colon != ':') {
            continue;
        }
        const char* value_start = kbo_json_skip_ws(colon + 1, end);
        *out_start = value_start;
        *out_end = kbo_json_value_token_end(value_start, end);
        return 1;
    }

    return 0;
}

static int kbo_write_all_bytes_to_file(const char* path, const char* data, DWORD size)
{
    if (path == NULL || data == NULL) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s", path);
    char* slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
        CreateDirectoryA(dir, NULL);
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD written = 0;
    int ok = WriteFile(file, data, size, &written, NULL) && written == size;
    CloseHandle(file);
    return ok;
}

int kbo_write_localappdata_json_int_value(const char* key, int value)
{
    if (key == NULL || key[0] == '\0') {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_localappdata_flags_json_path(path, sizeof(path))) {
        return 0;
    }

    char value_text[32] = {0};
    snprintf(value_text, sizeof(value_text), "%d", value);
    size_t value_len = strlen(value_text);

    char* input = NULL;
    DWORD input_size = 0;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(file, NULL);
        if (size != INVALID_FILE_SIZE && size <= KBO_FLAGS_JSON_MAX_BYTES) {
            input = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
            if (input != NULL) {
                DWORD read = 0;
                if (ReadFile(file, input, size, &read, NULL)) {
                    input_size = read;
                } else {
                    HeapFree(GetProcessHeap(), 0, input);
                    input = NULL;
                }
            }
        }
        CloseHandle(file);
    }

    if (input == NULL || input_size == 0) {
        char fresh[160] = {0};
        int written = snprintf(fresh, sizeof(fresh), "{\r\n  \"%s\": %d\r\n}\r\n", key, value);
        int ok = written > 0 && kbo_write_all_bytes_to_file(path, fresh, (DWORD)written);
        if (input != NULL) {
            HeapFree(GetProcessHeap(), 0, input);
        }
        return ok;
    }

    const char* value_start = NULL;
    const char* value_end = NULL;
    char* output = NULL;
    DWORD output_size = 0;

    if (kbo_find_json_value_span(input, input_size, key, &value_start, &value_end)
            && value_start != NULL && value_end != NULL && value_end >= value_start) {
        size_t prefix = (size_t)(value_start - input);
        size_t suffix = (size_t)((input + input_size) - value_end);
        output_size = (DWORD)(prefix + value_len + suffix);
        output = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)output_size + 1u);
        if (output != NULL) {
            memcpy(output, input, prefix);
            memcpy(output + prefix, value_text, value_len);
            memcpy(output + prefix + value_len, value_end, suffix);
        }
    } else {
        const char* end = input + input_size;
        const char* close = end;
        while (close > input && close[-1] != '}') {
            close--;
        }
        if (close > input && close[-1] == '}') {
            close--;
        } else {
            close = NULL;
        }

        if (close == NULL) {
            char fresh[160] = {0};
            int written = snprintf(fresh, sizeof(fresh), "{\r\n  \"%s\": %d\r\n}\r\n", key, value);
            int ok = written > 0 && kbo_write_all_bytes_to_file(path, fresh, (DWORD)written);
            HeapFree(GetProcessHeap(), 0, input);
            return ok;
        }

        const char* object_start = input;
        while (object_start < close && *object_start != '{') {
            object_start++;
        }
        const char* body = object_start < close ? object_start + 1 : input;
        int has_body = 0;
        for (const char* p = body; p < close; p++) {
            if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                has_body = 1;
                break;
            }
        }

        char insert[192] = {0};
        int insert_len = snprintf(insert, sizeof(insert), "%s\r\n  \"%s\": %d\r\n",
            has_body ? "," : "", key, value);
        if (insert_len <= 0) {
            HeapFree(GetProcessHeap(), 0, input);
            return 0;
        }

        size_t prefix = (size_t)(close - input);
        size_t suffix = (size_t)((input + input_size) - close);
        output_size = (DWORD)(prefix + (size_t)insert_len + suffix);
        output = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)output_size + 1u);
        if (output != NULL) {
            memcpy(output, input, prefix);
            memcpy(output + prefix, insert, (size_t)insert_len);
            memcpy(output + prefix + (size_t)insert_len, close, suffix);
        }
    }

    int ok = output != NULL && kbo_write_all_bytes_to_file(path, output, output_size);
    if (output != NULL) {
        HeapFree(GetProcessHeap(), 0, output);
    }
    HeapFree(GetProcessHeap(), 0, input);
    return ok;
}
