#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/files/atomic/core_atomic_file.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/core_league_context_parts/api/league_context_lookup.h"
#include "../../../core/news/live/core_live_news.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_name_cache.h"
#include "../../common/dates/foreign_waiver_date.h"
#include "../../common/player_eval/foreign_waiver_player_eval.h"
#include "../../common/policy/foreign_waiver_policy.h"
#include "../paths/foreign_injury_paths.h"

#include "../internal/foreign_injury_internal.h"
#ifndef KBO_FOREIGN_INJURY_SLOT_REGULAR
#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#endif
#ifndef KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2
#endif

#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4

typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
} KboForeignInjuryReplacement;

KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX] = {{0}};
int  g_kbo_foreign_injury_replacement_count = 0;
LONG g_kbo_foreign_injury_replacement_lock = 0;
char g_kbo_foreign_injury_replacement_loaded_path[MAX_PATH] = {0};

int kbo_persist_foreign_injury_replacements_locked(void);
int kbo_find_foreign_injury_replacement_locked(uint32_t injured_player_id, int include_closed);

/* Foreign injury replacement labels, slot helpers, and lock helpers. Included from native/KBOFix.c. */

const char* kbo_foreign_injury_slot_label(uint8_t slot_type)
{
    return slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular";
}

const char* kbo_foreign_injury_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FOREIGN_INJURY_STATUS_OPEN:    return "Open";
    case KBO_FOREIGN_INJURY_STATUS_ACTIVE:  return "Active";
    case KBO_FOREIGN_INJURY_STATUS_PENDING: return "Decision due";
    case KBO_FOREIGN_INJURY_STATUS_CLOSED:  return "Closed";
    default:                                return "Unknown";
    }
}

/* Foreign injury replacement CSV loading. Included from native/KBOFix.c. */

/* Foreign injury replacement seed import. Included from native/KBOFix.c. */

/* Foreign injury replacement CSV persistence. Included from native/KBOFix.c. */

/* Foreign injury replacement lazy-load orchestration. Included from native/KBOFix.c. */

/* Foreign injury replacement lookup and counting helpers. Included from native/KBOFix.c. */

/* Foreign injury replacement native news emission. Included from native/KBOFix.c. */

LONG g_kbo_foreign_injury_replacement_thread_started = 0;

