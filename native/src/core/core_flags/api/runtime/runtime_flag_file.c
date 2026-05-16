#include "../flags_api.h"

#include "../../keys/flag_key.h"
#include "../../localappdata/localappdata_reader.h"
#include "../../../sync/spin_lock.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

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
    static KboSpinLock cache_lock = KBO_SPIN_LOCK_INIT;

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

    int value = 0;
    int found = kbo_read_localappdata_json_flag_value(key, &value);
    if (found) {
        value = value ? 1 : 0;
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
