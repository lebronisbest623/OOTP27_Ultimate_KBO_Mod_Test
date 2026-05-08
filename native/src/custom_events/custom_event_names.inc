/* Custom-event name matching and calendar predicates. Included from native/src/custom_events.inc. */

static int kbo_custom_event_name_is_open(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_open_event_title);
}

static int kbo_custom_event_name_is_close(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_close_event_title);
}

static int kbo_custom_event_name_is_military_selection(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_military_selection_event_title);
}

static int kbo_custom_event_name_is_asian_games_selection(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_selection_event_title);
}

static int kbo_custom_event_name_is_asian_games_departure(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_departure_event_title);
}

static int kbo_custom_event_name_is_asian_games_final(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_final_event_title);
}
