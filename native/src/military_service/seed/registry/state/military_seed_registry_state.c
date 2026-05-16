#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../core/files/atomic/core_atomic_file.h"
#include "../../../../core/logging/core_log.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../../team/lookup/team_lookup.h"
#include "../../../players/state/military_player_state.h"
#include "../military_seed_registry.h"
#include "../../../calendar/military_service_date.h"
#include "../../paths/military_seed_paths.h"

#include "../military_seed_registry_internal.h"
KboMilitaryServiceSeed g_kbo_military_service_seeds[KBO_MILITARY_SERVICE_SEED_MAX];
int g_kbo_military_service_seed_count = 0;
KboLock g_kbo_military_service_seed_lock = KBO_LOCK_INIT;
LONG g_kbo_military_service_seed_loaded = 0;
ULONGLONG g_kbo_military_service_seed_last_resolve_tick = 0;
char g_kbo_military_service_seed_loaded_key[MAX_PATH * 3];












uint8_t* kbo_military_find_team_from_seed_code(const char* team_code)
{
    if (team_code == NULL || team_code[0] == '\0') {
        return NULL;
    }

    if (team_code[0] >= '0' && team_code[0] <= '9') {
        return find_kbo_team_by_numeric_id_any_league((uint32_t)strtoul(team_code, NULL, 10), 0);
    }
    return find_kbo_team_by_csv_id_any_league(team_code, 0);
}


