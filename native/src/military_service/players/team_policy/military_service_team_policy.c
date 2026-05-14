#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "military_service_team_policy.h"
#include "military_service_team_policy_parse.h"

#define KBO_MILITARY_SERVICE_TEAM_POLICY_FILE "military_service_teams.csv"
#define KBO_MILITARY_SERVICE_TEAM_MAX 16
#define KBO_MILITARY_SERVICE_TEAM_REFRESH_MS 1000u

typedef struct KboMilitaryServiceTeamPolicyEntry {
    char team_csv_id[16];
    volatile LONG team_id;
} KboMilitaryServiceTeamPolicyEntry;

static KboMilitaryServiceTeamPolicyEntry g_kbo_military_service_teams[KBO_MILITARY_SERVICE_TEAM_MAX];
static volatile LONG g_kbo_military_service_team_count = 0;
static volatile LONG g_kbo_military_policy_loaded_state = 0;
static volatile LONG g_kbo_military_policy_refresh_tick = 0;

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

static int kbo_military_service_read_legacy_team_id_file(uint32_t* out_value, char* out_path, size_t out_path_size)
{
    if (out_value != NULL) {
        *out_value = 0u;
    }
    if (out_path != NULL && out_path_size > 0u) {
        out_path[0] = '\0';
    }
    if (out_value == NULL) {
        return 0;
    }

    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file("kbo_sangmu_team_id.txt", path, sizeof(path))) {
        return 0;
    }
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }

    char buffer[64] = {0};
    size_t read = fread(buffer, 1, sizeof(buffer) - 1u, file);
    fclose(file);
    buffer[read] = '\0';

    unsigned long value = strtoul(buffer, NULL, 10);
    if (value == 0ul || value > 1000000ul) {
        append_logf("KBO military service team legacy id ignored value=%s source=%s", buffer, path);
        return 0;
    }

    *out_value = (uint32_t)value;
    if (out_path != NULL && out_path_size > 0u) {
        snprintf(out_path, out_path_size, "%s", path);
    }
    return 1;
}

static int kbo_military_service_add_team_csv_id(const char* team_csv_id)
{
    if (team_csv_id == NULL || team_csv_id[0] == '\0') {
        return 0;
    }

    LONG count = InterlockedCompareExchange(&g_kbo_military_service_team_count, 0, 0);
    for (LONG i = 0; i < count; i++) {
        if (_stricmp(g_kbo_military_service_teams[i].team_csv_id, team_csv_id) == 0) {
            return 1;
        }
    }
    if (count >= KBO_MILITARY_SERVICE_TEAM_MAX) {
        append_logf("KBO military service team policy ignored extra row team=%s max=%d", team_csv_id, KBO_MILITARY_SERVICE_TEAM_MAX);
        return 0;
    }

    KboMilitaryServiceTeamPolicyEntry* entry = &g_kbo_military_service_teams[count];
    snprintf(entry->team_csv_id, sizeof(entry->team_csv_id), "%s", team_csv_id);
    InterlockedExchange(&entry->team_id, 0);
    InterlockedExchange(&g_kbo_military_service_team_count, count + 1);
    return 1;
}

static int kbo_military_service_add_legacy_team_id(uint32_t team_id, const char* source)
{
    if (team_id == 0u) {
        return 0;
    }

    LONG count = InterlockedCompareExchange(&g_kbo_military_service_team_count, 0, 0);
    if (count >= KBO_MILITARY_SERVICE_TEAM_MAX) {
        return 0;
    }

    KboMilitaryServiceTeamPolicyEntry* entry = &g_kbo_military_service_teams[count];
    snprintf(entry->team_csv_id, sizeof(entry->team_csv_id), "%s", source != NULL ? source : "legacy");
    InterlockedExchange(&entry->team_id, (LONG)team_id);
    InterlockedExchange(&g_kbo_military_service_team_count, count + 1);
    return 1;
}

static int kbo_military_service_load_team_csv_policy(void)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_global_data_file(KBO_MILITARY_SERVICE_TEAM_POLICY_FILE, path, sizeof(path))) {
        return -1;
    }

    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    int loaded_rows = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        KboMilitaryServiceTeamPolicyRow row;
        if (!kbo_parse_military_service_team_policy_line(line, &row)) {
            continue;
        }
        if (!row.enabled) {
            continue;
        }
        if (kbo_military_service_add_team_csv_id(row.team_csv_id)) {
            loaded_rows++;
        }
    }
    fclose(file);

    append_logf("KBO military service team policy loaded rows=%d source=%s", loaded_rows, path);
    return loaded_rows;
}

static void kbo_military_service_load_legacy_policy(void)
{
    uint32_t team_id = 0u;
    const char* source = NULL;
    if (kbo_military_service_read_json_sangmu_team_id(&team_id)) {
        source = "kbo_team_policy.json";
    }

    char legacy_path[MAX_PATH] = {0};
    uint32_t legacy_file_team_id = 0u;
    if (kbo_military_service_read_legacy_team_id_file(
            &legacy_file_team_id,
            legacy_path,
            sizeof(legacy_path))) {
        team_id = legacy_file_team_id;
        source = legacy_path;
    }

    if (team_id != 0u) {
        kbo_military_service_add_legacy_team_id(team_id, "legacy");
        append_logf("KBO military service team legacy policy loaded team=%u source=%s", team_id, source != NULL ? source : "");
    } else {
        append_log_line("KBO military service team policy has no enabled teams");
    }
}

static void kbo_military_service_resolve_team_ids(int force)
{
    LONG count = InterlockedCompareExchange(&g_kbo_military_service_team_count, 0, 0);
    if (count <= 0) {
        return;
    }

    DWORD now = GetTickCount();
    DWORD last = (DWORD)InterlockedCompareExchange(&g_kbo_military_policy_refresh_tick, 0, 0);
    if (!force && last != 0u && now - last < KBO_MILITARY_SERVICE_TEAM_REFRESH_MS) {
        return;
    }
    InterlockedExchange(&g_kbo_military_policy_refresh_tick, (LONG)now);

    for (LONG i = 0; i < count; i++) {
        KboMilitaryServiceTeamPolicyEntry* entry = &g_kbo_military_service_teams[i];
        if (InterlockedCompareExchange(&entry->team_id, 0, 0) != 0
                || entry->team_csv_id[0] == '\0'
                || _stricmp(entry->team_csv_id, "legacy") == 0) {
            continue;
        }

        uint8_t* team = find_kbo_team_by_csv_id_any_league(entry->team_csv_id, 1);
        if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        if (team_id == 0u) {
            continue;
        }

        LONG old = InterlockedExchange(&entry->team_id, (LONG)team_id);
        if ((uint32_t)old != team_id) {
            append_logf("KBO military service team resolved csv=%s team=%u", entry->team_csv_id, team_id);
        }
    }
}

void kbo_load_military_service_team_policy_override_once(void)
{
    LONG state = InterlockedCompareExchange(&g_kbo_military_policy_loaded_state, 1, 0);
    if (state == 2) {
        return;
    }
    if (state != 0) {
        while (InterlockedCompareExchange(&g_kbo_military_policy_loaded_state, 0, 0) != 2) {
            Sleep(0);
        }
        return;
    }

    if (kbo_military_service_load_team_csv_policy() < 0) {
        kbo_military_service_load_legacy_policy();
    }
    kbo_military_service_resolve_team_ids(1);
    InterlockedExchange(&g_kbo_military_policy_loaded_state, 2);
}

int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    kbo_load_military_service_team_policy_override_once();
    kbo_military_service_resolve_team_ids(0);

    LONG count = InterlockedCompareExchange(&g_kbo_military_service_team_count, 0, 0);
    for (LONG i = 0; i < count; i++) {
        uint32_t service_team_id = (uint32_t)InterlockedCompareExchange(
            &g_kbo_military_service_teams[i].team_id,
            0,
            0);
        if (service_team_id != 0u && team_id == service_team_id) {
            return 1;
        }
    }
    return 0;
}
