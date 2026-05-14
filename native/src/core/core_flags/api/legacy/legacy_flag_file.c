#include "../flags_api.h"

#include "../../keys/flag_key.h"
#include "../../localappdata/localappdata_reader.h"
#include "../../../files/save_paths/core_save_paths.h"
#include "../../../sync/spin_lock.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static int kbo_legacy_localappdata_flag_file_exists(const char* file_name)
{
    if (file_name == NULL || file_name[0] == '\0') {
        return 0;
    }

    char path[32768] = {0};
    if (!kbo_get_global_data_file(file_name, path, sizeof(path))) {
        return 0;
    }

    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static int kbo_legacy_localappdata_flag_key_exists(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return 0;
    }

    char file_name[160] = {0};
    int written = snprintf(file_name, sizeof(file_name), "%s.txt", key);
    if (written <= 0 || written >= (int)sizeof(file_name)) {
        return 0;
    }

    return kbo_legacy_localappdata_flag_file_exists(file_name);
}

int read_kbo_localappdata_flag_file(const char* file_name)
{
    char key[128] = {0};
    if (!kbo_flag_key_from_file_name(file_name, key, sizeof(key))) {
        return 0;
    }

    typedef struct KboFlagCacheEntry {
        char key[128];
        int value;
        DWORD tick;
        int valid;
    } KboFlagCacheEntry;
    static KboFlagCacheEntry cache[128];
    static volatile LONG cache_lock = 0;

    DWORD now = GetTickCount();
    kbo_spin_lock(&cache_lock);
    for (int i = 0; i < (int)(sizeof(cache) / sizeof(cache[0])); i++) {
        if (cache[i].valid && strcmp(cache[i].key, key) == 0 && now - cache[i].tick < 2000u) {
            int cached = cache[i].value;
            kbo_spin_unlock(&cache_lock);
            return cached;
        }
    }
    kbo_spin_unlock(&cache_lock);

    const char* legacy_json_key = kbo_flag_legacy_json_key_for_key(key);

    int value = 0;
    int found = kbo_read_localappdata_json_flag_value(
        key,
        legacy_json_key != NULL ? legacy_json_key : file_name,
        &value);
    if (found) {
        value = value ? 1 : 0;
    } else {
        value = kbo_legacy_localappdata_flag_file_exists(file_name);
        if (!value && legacy_json_key != NULL) {
            value = kbo_legacy_localappdata_flag_key_exists(legacy_json_key);
        }
    }

    kbo_spin_lock(&cache_lock);
    int slot = -1;
    for (int i = 0; i < (int)(sizeof(cache) / sizeof(cache[0])); i++) {
        if (cache[i].valid && strcmp(cache[i].key, key) == 0) {
            slot = i;
            break;
        }
        if (slot < 0 && !cache[i].valid) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = (int)(now % (DWORD)(sizeof(cache) / sizeof(cache[0])));
    }
    snprintf(cache[slot].key, sizeof(cache[slot].key), "%s", key);
    cache[slot].value = value;
    cache[slot].tick = now;
    cache[slot].valid = 1;
    kbo_spin_unlock(&cache_lock);
    return value;
}
