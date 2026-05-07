/* ---- Forward declarations ---- */
__declspec(noinline) void ootp_kbo_military_service_entry_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_military_status_update_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr, int32_t team_id, uint16_t year_hint, uintptr_t original_func_ptr);
__declspec(noinline) uint8_t ootp_kbo_player_offer_eligibility_wrapper(
    uintptr_t player_ptr, int32_t team_id, int32_t flag, uintptr_t original_func_ptr);
__declspec(noinline) int ootp_kbo_global_handle_callback_probe_wrapper(
    uintptr_t sender, intptr_t callback_id, uintptr_t value, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_fa_submit_offer_probe_wrapper(
    uintptr_t screen_ptr, uintptr_t original_func_ptr);
__declspec(noinline) int ootp_kbo_fa_offer_screen_callback_probe_wrapper(
    uintptr_t screen_ptr, uintptr_t sender_ptr, uintptr_t callback_id, uintptr_t value, uintptr_t original_func_ptr);
__declspec(noinline) int ootp_kbo_fa_contract_offer_callback_probe_wrapper(
    uintptr_t offer_ptr, uintptr_t sender_ptr, uintptr_t callback_id, uintptr_t value, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_fa_offer_player_demand_floor_probe(
    uintptr_t player_ptr, uintptr_t screen_ptr, uint32_t source_rva);
__declspec(noinline) int32_t ootp_kbo_no_minor_demand_write_floor_probe(
    uintptr_t player_ptr, int32_t proposed_demand, uint32_t source_rva, int32_t salary_floor_hint);
__declspec(noinline) uint8_t ootp_kbo_player_action_eligibility_wrapper(
    uintptr_t action_context, int32_t action_id, uint8_t strict_check, uintptr_t original_func_ptr);
static void start_kbo_no_minor_contract_demand_floor_scanner_thread(void);
__declspec(noinline) int ootp_kbo_fa_signing_branch_wrapper(
    uintptr_t player_ptr, uintptr_t team_ptr);
__declspec(noinline) int ootp_kbo_trade_check_foreign_policy_probe(
    uintptr_t trade_ptr,
    int32_t side);
__declspec(noinline) int32_t ootp_kbo_ai_fa_status_candidate_insert_wrapper(
    uintptr_t frame_ptr, uintptr_t player_ptr, int32_t insert_index, uintptr_t candidate_array);
__declspec(noinline) int32_t ootp_kbo_intl_established_fa_count_wrapper(
    int32_t original_count, uintptr_t league_ptr);
__declspec(noinline) void ootp_kbo_intl_established_fa_player_probe_wrapper(
    uintptr_t player_ptr, uintptr_t league_ptr);
__declspec(noinline) int32_t ootp_kbo_active_foreign_hitter_count_wrapper(
    uintptr_t team_ptr, uintptr_t original_func_ptr);
__declspec(noinline) int32_t ootp_kbo_active_foreign_pitcher_count_wrapper(
    uintptr_t team_ptr, uintptr_t original_func_ptr);
__declspec(noinline) uint8_t ootp_kbo_callup_foreign_hitter_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit);
__declspec(noinline) uint8_t ootp_kbo_callup_foreign_pitcher_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit);
__declspec(noinline) uint8_t ootp_kbo_callup_foreign_total_limit_allows_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, int32_t active_count, int32_t limit);
static int memory_range_readable(const void* address, SIZE_T size);
static uintptr_t get_ootp_global_database(void);
static int kbo_player_pointer_plausible(uintptr_t player_ptr);
static int kbo_player_is_foreign_for_kbo_rights(uint8_t* player);
static int kbo_custom_foreign_policy_enabled(void);
static int kbo_custom_foreign_policy_team_allows_final_signing(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
static void kbo_record_recent_custom_foreign_policy_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today);
static int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_date);
static const char* kbo_foreign_injury_slot_label(uint8_t slot_type);
static int find_kbo_global_player_vector(uintptr_t* out_vector, int32_t* out_count, uint32_t* out_offset);
static int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size);
static int kbo_get_current_save_path(char* out, size_t out_size);
static void kbo_copy_player_display_name(uint8_t* player, char* out, size_t out_size);
static uint8_t* find_kbo_team_by_csv_id_any_league(const char* team_id, int allow_deleted);
static uint8_t* find_kbo_team_by_numeric_id_any_league(uint32_t team_id, int allow_deleted);
static int create_kbo_native_live_news_with_body(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title,
    const char* body);
static int create_kbo_native_live_news(
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t league_id,
    uint32_t message_type,
    const char* title);
static int insert_kbo_player_history_sql(
    uint32_t player_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const char* text,
    const char* source);
static int insert_kbo_roster_transaction_sql(
    uint32_t league_id,
    uint32_t team_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t transaction_type,
    const char* league_text,
    const char* team_text,
    const char* source);
static void start_kbo_foreign_waiver_scanner_thread(void);
static void start_kbo_foreign_injury_replacement_thread(void);
static void start_kbo_fa_requalification_thread(void);
static void kbo_flush_pending_foreign_priority_events(const char* source);
static int kbo_get_current_yyyymmdd(uint32_t* out_date);
static int kbo_enforce_foreign_waiver_signability(
    uintptr_t player_ptr,
    int32_t requesting_team_id,
    uint16_t year_hint,
    int original_signability,
    uintptr_t caller_rva);
static int start_kbo_custom_event_monitor(void);
static int start_kbo_season_phase_monitor(void);
static int install_kbo_season_phase_write_probe_patches(void);
static int kbo_schedule_foreign_priority_custom_events(const char* source);
__declspec(noinline) void ootp_kbo_season_phase_write_probe(
    uintptr_t league_ptr,
    uint32_t value,
    uint32_t site_rva);
static void kbo_hub_copy_player_display_name(uint8_t* player, char* out, size_t out_size);
__declspec(noinline) int ootp_kbo_seed_single_division_allstar_candidate_teams(
    uintptr_t league_ptr,
    void* left_team_vector,
    void* right_team_vector,
    uintptr_t vector_push_back_ptr);
__declspec(noinline) void ootp_kbo_enable_allstar_setting(uintptr_t league_ptr);
__declspec(noinline) void ootp_kbo_prepare_allstar_events(uintptr_t league_ptr);
__declspec(noinline) void ootp_kbo_prepare_allstar_voting_begin(uintptr_t league_ptr, uintptr_t allstar_team_setup_ptr);

