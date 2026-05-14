#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "military_service_team_policy.h"

#define KBO_TEAM_POLICY_FALLBACK_SANGMU_TEAM_ID 21u

static volatile LONG g_kbo_military_policy_sang_team_id = (LONG)KBO_TEAM_POLICY_FALLBACK_SANGMU_TEAM_ID;
static volatile LONG g_kbo_military_policy_override_loaded = 0;

static int kbo_military_service_read_json_sangmu_team_id(uint32_t* out_value)
{
    if (out_value == NULL) {
        return 0;
    }
    int value = 0;
    if (kbo_read_localappdata_named_json_int_value("kbo_team_policy.json", "sangmu_team_id", &value)
            && value > 0
            && value <= 1000000) {
        *out_value = (uint32_t)value;
        return 1;
    }
    return 0;
}

void kbo_load_military_service_team_policy_override_once(void)
{
    if (InterlockedCompareExchange(&g_kbo_military_policy_override_loaded, 1, 0) != 0) {
        return;
    }

    uint32_t configured_sangmu_team_id = 0u;
    if (kbo_military_service_read_json_sangmu_team_id(&configured_sangmu_team_id)) {
        InterlockedExchange(&g_kbo_military_policy_sang_team_id, (LONG)configured_sangmu_team_id);
        append_logf("KBO military FA team policy fixed sangmu=%u source=kbo_team_policy.json", configured_sangmu_team_id);
    } else {
        append_logf("KBO military FA team policy fixed sangmu=%u source=fallback", KBO_TEAM_POLICY_FALLBACK_SANGMU_TEAM_ID);
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file("kbo_sangmu_team_id.txt", path, sizeof(path))) {
        return;
    }
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return;
    }

    char buffer[64] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[read] = '\0';

    unsigned long value = strtoul(buffer, NULL, 10);
    if (value > 0ul && value <= 1000000ul) {
        InterlockedExchange(&g_kbo_military_policy_sang_team_id, (LONG)value);
        append_logf("KBO military FA team policy fixed sangmu=%lu source=%s", value, path);
    } else {
        append_logf("KBO military FA team policy override ignored value=%s source=%s", buffer, path);
    }
}

int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    uint32_t cached_sang_id = (uint32_t)InterlockedCompareExchange(&g_kbo_military_policy_sang_team_id, 0, 0);

    return team_id == cached_sang_id;
}
