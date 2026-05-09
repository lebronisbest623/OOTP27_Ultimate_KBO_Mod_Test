#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/assignment/roster_arrays/team_roster_arrays.h"
#include "../../common/csv/foreign_csv_parse.h"
#include "../../common/paths/foreign_waiver_paths.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../query/foreign_waiver_rights_query.h"

LONG g_kbo_foreign_waiver_rights_lock = 0;
KboForeignWaiverRetention g_kbo_foreign_waiver_rights[KBO_FOREIGN_WAIVER_RIGHTS_MAX] = {{0}};
int g_kbo_foreign_waiver_rights_count = 0;

/* Foreign reserve-right active-state helpers. */

int kbo_is_foreign_waiver_right_active(const KboForeignWaiverRetention* rec, uint32_t today)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->team_id == 0u
            || rec->retained_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd < rec->retained_on_yyyymmdd
            || today == 0u) {
        return 0;
    }
    return rec->retained_on_yyyymmdd <= today && today <= rec->expires_on_yyyymmdd;
}

int kbo_is_foreign_waiver_right_expired(const KboForeignWaiverRetention* rec, uint32_t today)
{
    if (rec == NULL
            || rec->player_id == 0u
            || rec->team_id == 0u
            || rec->retained_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd == 0u
            || rec->expires_on_yyyymmdd < rec->retained_on_yyyymmdd
            || today == 0u) {
        return 1;
    }
    return today > rec->expires_on_yyyymmdd;
}

