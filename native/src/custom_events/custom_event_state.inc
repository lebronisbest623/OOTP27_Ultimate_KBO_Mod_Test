/* Custom-event shared state, titles, and cross-module declarations. Included from native/src/custom_events.inc. */

static LONG g_kbo_custom_event_monitor_started = 0;
static uint32_t g_kbo_foreign_priority_last_scheduled_date = 0;
static uint32_t g_kbo_military_selection_last_scheduled_date = 0;
static uint32_t g_kbo_asian_games_last_scheduled_year = 0;
static uint32_t g_kbo_custom_event_last_observed_league_year = 0;
static uint32_t g_kbo_foreign_priority_last_open_event_fired_date = 0;
static uint32_t g_kbo_foreign_priority_last_close_event_fired_date = 0;
static uint32_t g_kbo_asian_games_last_selection_fired_date = 0;
static uint32_t g_kbo_asian_games_last_departure_fired_date = 0;
static uint32_t g_kbo_asian_games_last_final_fired_date = 0;
static uintptr_t g_kbo_processed_event_ptrs[256] = {0};
static LONG g_kbo_processed_event_count = 0;

static const char g_kbo_foreign_priority_open_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Begins";
static const char g_kbo_foreign_priority_close_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Ends";
static const char g_kbo_military_selection_event_title[] =
    "[KBO] Military Service Selection";
static const char g_kbo_asian_games_selection_event_title[] =
    "[KBO] Asian Games Roster Selection";
static const char g_kbo_asian_games_departure_event_title[] =
    "[KBO] Asian Games Player Departure";
static const char g_kbo_asian_games_final_event_title[] =
    "[KBO] Asian Games Final";

static int kbo_announce_foreign_waiver_results(uint32_t event_yyyymmdd, const char* source);
static void kbo_copy_asian_games_team_link(uint32_t team_id, char* out, size_t out_size);
static void kbo_copy_asian_games_player_link(KboAsianGamesRosterEntry* entry, char* out, size_t out_size);

