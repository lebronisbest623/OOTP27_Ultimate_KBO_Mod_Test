/* Asian Games replacement, departure, and return lifecycle assembler. */

static int kbo_emit_asian_games_replacement_news(
    uint32_t event_yyyymmdd,
    KboAsianGamesRosterEntry* old_entry,
    KboAsianGamesRosterEntry* new_entry,
    const char* source);

#include "asian_games_lifecycle_roster.inc"
#include "asian_games_lifecycle_replacement.inc"
#include "asian_games_lifecycle_departure.inc"
#include "asian_games_lifecycle_final.inc"
