

#include "custom_event_state.h"

LONG g_kbo_custom_event_monitor_started = 0;
uint32_t g_kbo_foreign_priority_last_scheduled_date = 0;
uint32_t g_kbo_military_selection_last_scheduled_date = 0;
uint32_t g_kbo_asian_games_last_scheduled_year = 0;
uint32_t g_kbo_custom_event_last_observed_league_year = 0;
uint32_t g_kbo_foreign_priority_last_open_event_fired_date = 0;
uint32_t g_kbo_foreign_priority_last_close_event_fired_date = 0;
uint32_t g_kbo_asian_games_last_selection_fired_date = 0;
uint32_t g_kbo_asian_games_last_departure_fired_date = 0;
uint32_t g_kbo_asian_games_last_final_fired_date = 0;
uint32_t g_kbo_custom_event_last_phase_league_id = 0;
uintptr_t g_kbo_custom_event_last_phase_league_ptr = 0;
uint32_t g_kbo_custom_event_last_phase_date = 0;
uint32_t g_kbo_custom_event_last_phase_league_year = 0;
uint32_t g_kbo_custom_event_last_phase_year = 0;
uint8_t g_kbo_custom_event_last_seen_league_phase = 0xffu;
uint32_t g_kbo_custom_event_pending_offseason_transition_anchor = 0;
uint32_t g_kbo_custom_event_last_offseason_transition_anchor = 0;
uintptr_t g_kbo_processed_event_ptrs[256] = {0};
LONG g_kbo_processed_event_count = 0;

const char g_kbo_default_event_source[] = "custom_event_monitor";
const char g_kbo_foreign_priority_open_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Begins";
const char g_kbo_foreign_priority_close_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Ends";
const char g_kbo_military_selection_event_title[] =
    "[KBO] Military Service Selection";
const char g_kbo_asian_games_selection_event_title[] =
    "[KBO] Asian Games Roster Selection";
const char g_kbo_asian_games_departure_event_title[] =
    "[KBO] Asian Games Player Departure";
const char g_kbo_asian_games_final_event_title[] =
    "[KBO] Asian Games Final";

