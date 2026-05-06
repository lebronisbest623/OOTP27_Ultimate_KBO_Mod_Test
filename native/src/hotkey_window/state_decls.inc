static void kbo_hub_ensure_valid_selection(void);
static void kbo_refresh_hotkey_window(void);
static int kbo_resolve_foreign_waiver_top_candidate_for_team(uint32_t team_id, uint32_t* out_player_id, uint32_t* out_current_team_id);
static int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain);
static int kbo_is_foreign_waiver_negotiation_window_open(void);
