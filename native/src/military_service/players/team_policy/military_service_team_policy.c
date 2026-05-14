#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../../core/core_flags/localappdata/localappdata_reader.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/logging/core_log.h"
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_string.h"
#include "military_service_team_policy.h"

#define KBO_TEAM_POLICY_FALLBACK_SANGMU_TEAM_ID 21u
#define KBO_TEAM_POLICY_LIVE_ID_REFRESH_MS 1000u

static volatile LONG g_kbo_military_policy_sang_team_id = (LONG)KBO_TEAM_POLICY_FALLBACK_SANGMU_TEAM_ID;
static volatile LONG g_kbo_military_policy_live_sang_team_id = 0;
static volatile LONG g_kbo_military_policy_live_kpb_team_id = 0;
static volatile LONG g_kbo_military_policy_live_refresh_tick = 0;
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

static uint32_t kbo_military_service_team_id_from_csv(const char* csv_id)
{
    uint8_t* team = find_kbo_team_by_csv_id_any_league(csv_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0u;
    }
    return *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
}

static void kbo_note_live_military_service_team_id(const char* csv_id, uint32_t team_id)
{
    if (csv_id == NULL || team_id == 0u) {
        return;
    }

    volatile LONG* slot = NULL;
    if (_stricmp(csv_id, "SANG") == 0) {
        slot = &g_kbo_military_policy_live_sang_team_id;
    } else if (_stricmp(csv_id, "KPB") == 0) {
        slot = &g_kbo_military_policy_live_kpb_team_id;
    }
    if (slot == NULL) {
        return;
    }

    LONG old = InterlockedExchange(slot, (LONG)team_id);
    if ((uint32_t)old != team_id) {
        append_logf("KBO military service team live id resolved csv=%s team=%u", csv_id, team_id);
    }
}

static void kbo_refresh_live_military_service_team_ids(void)
{
    DWORD now = GetTickCount();
    DWORD last = (DWORD)InterlockedCompareExchange(&g_kbo_military_policy_live_refresh_tick, 0, 0);
    if (last != 0u && now - last <= KBO_TEAM_POLICY_LIVE_ID_REFRESH_MS) {
        return;
    }

    uint32_t sang_id = kbo_military_service_team_id_from_csv("SANG");
    uint32_t kpb_id = kbo_military_service_team_id_from_csv("KPB");
    if (sang_id != 0u) {
        kbo_note_live_military_service_team_id("SANG", sang_id);
    }
    if (kpb_id != 0u) {
        kbo_note_live_military_service_team_id("KPB", kpb_id);
    }
    InterlockedExchange(&g_kbo_military_policy_live_refresh_tick, (LONG)now);
}

int kbo_team_id_is_military_service_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0;
    }

    kbo_load_military_service_team_policy_override_once();

    uint32_t cached_sang_id = (uint32_t)InterlockedCompareExchange(&g_kbo_military_policy_sang_team_id, 0, 0);
    if (team_id == cached_sang_id) {
        return 1;
    }

    kbo_refresh_live_military_service_team_ids();

    uint32_t live_sang_id = (uint32_t)InterlockedCompareExchange(&g_kbo_military_policy_live_sang_team_id, 0, 0);
    uint32_t live_kpb_id = (uint32_t)InterlockedCompareExchange(&g_kbo_military_policy_live_kpb_team_id, 0, 0);
    return (live_sang_id != 0u && team_id == live_sang_id)
        || (live_kpb_id != 0u && team_id == live_kpb_id);
}

int kbo_team_ptr_is_military_service_team(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    if (kbo_team_id_is_military_service_team(team_id)) {
        return 1;
    }

    if (team_has_ootp_string_text(team, "SANG")) {
        kbo_note_live_military_service_team_id("SANG", team_id);
        return 1;
    }
    if (team_has_ootp_string_text(team, "KPB")) {
        kbo_note_live_military_service_team_id("KPB", team_id);
        return 1;
    }
    return 0;
}
