#include "core_history_stubs.h"

/* Core legacy special-history stubs. */

void flush_pending_special_player_history_sql(const char* source)
{
    (void)source;
}

void append_special_player_history_csv(
    uint32_t player_id,
    uint16_t season,
    const char* history_date,
    const char* event_type,
    const char* history_text)
{
    (void)player_id;
    (void)season;
    (void)history_date;
    (void)event_type;
    (void)history_text;
}
