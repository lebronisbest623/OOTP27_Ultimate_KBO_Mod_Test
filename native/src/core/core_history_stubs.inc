/* Core legacy special-history stubs. */

static void flush_pending_special_player_history_sql(const char* source)
{
    (void)source;
}

static void append_special_player_history_csv(
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
