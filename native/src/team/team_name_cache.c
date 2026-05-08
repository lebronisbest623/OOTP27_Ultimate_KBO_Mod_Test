#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "team_name_cache.h"
#include "../bootstrap/ootp_offsets.h"
#include "../core/core_log.h"
#include "../core/core_save_paths.h"
#include "../runtime_memory/runtime_memory.h"

#define KBO_NAME_ID_CACHE_MAX       300000u
#define KBO_NAME_CACHE_TEXT_BYTES   48u
#define KBO_NAMES_DAT_MAX_BYTES     (32u * 1024u * 1024u)

static char* g_kbo_name_cache_texts = NULL;
static char  g_kbo_name_cache_save_path[MAX_PATH] = {0};
static LONG  g_kbo_name_cache_loading = 0;

static uint32_t kbo_read_u32_le_bytes(const uint8_t* data)
{
    return (uint32_t)data[0]
        | ((uint32_t)data[1] << 8)
        | ((uint32_t)data[2] << 16)
        | ((uint32_t)data[3] << 24);
}

static int kbo_name_record_text_plausible(const uint8_t* text, uint32_t len)
{
    if (text == NULL || len == 0 || len >= KBO_NAME_CACHE_TEXT_BYTES) {
        return 0;
    }

    int has_letter = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = text[i];
        if (c == 0 || c < 0x20u) {
            return 0;
        }
        if (c >= 0x80u) {
            has_letter = 1;
            continue;
        }
        if (c > 0x7eu || c == '<' || c == '>' || c == '\\' || c == '/') {
            return 0;
        }
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            has_letter = 1;
        }
    }

    return has_letter;
}

static void kbo_name_cache_store(char* cache, uint32_t id, const uint8_t* text, uint32_t len)
{
    if (cache == NULL || id == 0 || id > KBO_NAME_ID_CACHE_MAX
            || !kbo_name_record_text_plausible(text, len)) {
        return;
    }

    char* slot = cache + ((SIZE_T)id * KBO_NAME_CACHE_TEXT_BYTES);
    if (slot[0] != '\0') {
        return;
    }

    SIZE_T copy_len = len;
    if (copy_len >= KBO_NAME_CACHE_TEXT_BYTES) {
        copy_len = KBO_NAME_CACHE_TEXT_BYTES - 1u;
    }
    memcpy(slot, text, copy_len);
    slot[copy_len] = '\0';
}

static int kbo_load_name_cache_for_save(const char* save_path)
{
    if (save_path == NULL || save_path[0] == '\0') {
        return 0;
    }

    if (g_kbo_name_cache_texts != NULL
            && strcmp(g_kbo_name_cache_save_path, save_path) == 0) {
        return 1;
    }

    if (InterlockedCompareExchange(&g_kbo_name_cache_loading, 1, 0) != 0) {
        return g_kbo_name_cache_texts != NULL
            && strcmp(g_kbo_name_cache_save_path, save_path) == 0;
    }

    if (g_kbo_name_cache_texts != NULL) {
        HeapFree(GetProcessHeap(), 0, g_kbo_name_cache_texts);
        g_kbo_name_cache_texts = NULL;
        g_kbo_name_cache_save_path[0] = '\0';
    }

    char path[MAX_PATH] = {0};
    snprintf(path, sizeof(path), "%s\\names.dat", save_path);

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO names.dat cache skipped reason=open_failed gle=%lu path=%s", GetLastError(), path);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart <= 0
            || file_size.QuadPart > (LONGLONG)KBO_NAMES_DAT_MAX_BYTES) {
        append_logf("KBO names.dat cache skipped reason=bad_size size=%lld path=%s", file_size.QuadPart, path);
        CloseHandle(file);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    DWORD bytes_to_read = (DWORD)file_size.QuadPart;
    uint8_t* data = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, bytes_to_read);
    SIZE_T table_bytes = ((SIZE_T)KBO_NAME_ID_CACHE_MAX + 1u) * KBO_NAME_CACHE_TEXT_BYTES;
    char* cache = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, table_bytes);
    if (data == NULL || cache == NULL) {
        append_logf("KBO names.dat cache skipped reason=alloc_failed bytes=%lu table=%llu", bytes_to_read, (unsigned long long)table_bytes);
        if (data != NULL) { HeapFree(GetProcessHeap(), 0, data); }
        if (cache != NULL) { HeapFree(GetProcessHeap(), 0, cache); }
        CloseHandle(file);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    DWORD read = 0;
    BOOL ok = ReadFile(file, data, bytes_to_read, &read, NULL);
    CloseHandle(file);
    if (!ok || read != bytes_to_read) {
        append_logf("KBO names.dat cache skipped reason=read_failed gle=%lu read=%lu expected=%lu path=%s",
            GetLastError(), read, bytes_to_read, path);
        HeapFree(GetProcessHeap(), 0, data);
        HeapFree(GetProcessHeap(), 0, cache);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    uint32_t loaded = 0;
    for (DWORD i = 0; i + 13u < read; ) {
        uint8_t tag = data[i];
        if (tag != 0x27u && tag != 0x07u) {
            i++;
            continue;
        }

        uint32_t len = kbo_read_u32_le_bytes(data + i + 1u);
        if (len == 0 || len >= KBO_NAME_CACHE_TEXT_BYTES
                || i + 5u + len + 8u > read) {
            i++;
            continue;
        }

        uint32_t text_offset = i + 5u;
        uint32_t id_offset   = text_offset + len + 4u;
        if (data[text_offset + len] != 0u
                || data[text_offset + len + 1u] != 0u
                || data[text_offset + len + 2u] != 0u
                || data[text_offset + len + 3u] != 0u) {
            i++;
            continue;
        }

        uint32_t id = kbo_read_u32_le_bytes(data + id_offset);
        if (id > 0 && id <= KBO_NAME_ID_CACHE_MAX
                && kbo_name_record_text_plausible(data + text_offset, len)) {
            char* slot = cache + ((SIZE_T)id * KBO_NAME_CACHE_TEXT_BYTES);
            if (slot[0] == '\0') {
                kbo_name_cache_store(cache, id, data + text_offset, len);
                loaded++;
            }
            i = id_offset + sizeof(uint32_t);
            continue;
        }

        i++;
    }

    HeapFree(GetProcessHeap(), 0, data);
    if (loaded == 0) {
        HeapFree(GetProcessHeap(), 0, cache);
        append_logf("KBO names.dat cache skipped reason=no_records path=%s", path);
        InterlockedExchange(&g_kbo_name_cache_loading, 0);
        return 0;
    }

    g_kbo_name_cache_texts = cache;
    snprintf(g_kbo_name_cache_save_path, sizeof(g_kbo_name_cache_save_path), "%s", save_path);
    append_logf("KBO names.dat cache loaded entries=%u path=%s", loaded, path);

    InterlockedExchange(&g_kbo_name_cache_loading, 0);
    return 1;
}

static int kbo_lookup_name_id(uint32_t name_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (name_id == 0 || name_id > KBO_NAME_ID_CACHE_MAX) {
        return 0;
    }

    char save_path[MAX_PATH] = {0};
    if (!kbo_get_current_save_path(save_path, sizeof(save_path))
            || !kbo_load_name_cache_for_save(save_path)) {
        return 0;
    }

    char* slot = g_kbo_name_cache_texts + ((SIZE_T)name_id * KBO_NAME_CACHE_TEXT_BYTES);
    if (slot[0] == '\0') {
        return 0;
    }

    snprintf(out, out_size, "%s", slot);
    return 1;
}

static int kbo_player_uses_eastern_name_order(uint8_t* player)
{
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_NATION_ID_OFFSET, sizeof(uint32_t))) {
        return 0;
    }
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    return nation_id == 42u
        || nation_id == 43u
        || nation_id == 98u
        || nation_id == OOTP27_KBO_KOREA_NATION_ID;
}

void kbo_copy_player_display_name(uint8_t* player, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        snprintf(out, out_size, "Unknown player");
        return;
    }

    uint32_t given_name_id  = *(uint32_t*)(player + OOTP27_PLAYER_GIVEN_NAME_ID_OFFSET);
    uint32_t family_name_id = *(uint32_t*)(player + OOTP27_PLAYER_FAMILY_NAME_ID_OFFSET);

    char given[64]  = {0};
    char family[64] = {0};
    int has_given  = kbo_lookup_name_id(given_name_id, given, sizeof(given));
    int has_family = kbo_lookup_name_id(family_name_id, family, sizeof(family));

    if (has_family && has_given) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        if (kbo_player_uses_eastern_name_order(player)) {
            snprintf(out, out_size, "%s %s", family, given);
        } else {
            snprintf(out, out_size, "%s %s", given, family);
        }
#pragma GCC diagnostic pop
    } else if (has_given) {
        snprintf(out, out_size, "%s", given);
    } else if (has_family) {
        snprintf(out, out_size, "%s", family);
    } else {
        snprintf(out, out_size, "Unknown player");
    }
}
