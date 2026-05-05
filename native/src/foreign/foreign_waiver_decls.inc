/* Foreign waiver forward declarations. Included from native/src/foreign_waiver_ai.inc. */

static uint32_t read_u32_leading_number_from_file(const char* filename);
static int get_kbo_foreign_waiver_cmd_path(char* out, size_t out_size);
static int get_kbo_foreign_waiver_decisions_path(char* out, size_t out_size);
static int get_kbo_asian_quota_nation_ids_path(char* out, size_t out_size);
static int kbo_load_foreign_waiver_rights(void);
static int kbo_persist_foreign_waiver_rights(void);
static void kbo_prune_expired_foreign_waiver_rights(uint32_t today_yyyymmdd);
static int parse_u32_from_csv_field(const char** cursor, uint32_t* out_value);
static int kbo_player_is_foreign_for_kbo_rights(uint8_t* player);
static int kbo_player_is_asian_quota_candidate(uint8_t* player);
static int kbo_is_forced_foreign_candidate_id(uint32_t player_id);
static int32_t kbo_foreign_waiver_value_score(uint8_t* player);
static int32_t kbo_get_foreign_waiver_value_threshold(void);
static uint8_t* kbo_find_player_by_id(uint32_t player_id, uint32_t* out_current_team_id, uint32_t* out_current_league_id);
static uint32_t kbo_get_foreign_waiver_league_id(void);
static int kbo_get_foreign_waiver_current_yyyymmdd(uint32_t* out_date);
static int kbo_is_foreign_waiver_negotiation_window_open(void);
static uint32_t kbo_get_foreign_waiver_auto_target_team_id(void);
static int kbo_read_foreign_waiver_window(uint32_t* out_start, uint32_t* out_end);
static int kbo_get_foreign_waiver_window_status_text(char* out, size_t out_size);
static int kbo_set_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t league_id, uint32_t retained_on, uint32_t expires_on);
static int kbo_has_active_foreign_waiver_right(uint32_t team_id, uint32_t player_id, uint32_t today_yyyymmdd);
static int kbo_find_active_foreign_waiver_holder(uint32_t player_id, uint32_t today_yyyymmdd, uint32_t* out_team_id);
static int kbo_sync_active_foreign_waiver_right_to_memory(uint8_t* player, uint32_t player_id, uint32_t holder_team_id, uint32_t today_yyyymmdd);
static void kbo_record_recent_custom_foreign_policy_block(uint32_t player_id, uint32_t requester_team_id, uint32_t today);
static int kbo_recent_custom_foreign_policy_block_matches(uint32_t player_id, uint32_t today, uint32_t* out_requester_team_id);
static void kbo_count_team_asian_quota_probe(
    uint32_t team_id,
    uint32_t* out_foreign_count,
    uint32_t* out_asian_quota_count,
    uint32_t* out_non_asian_foreign_count);
static uint32_t kbo_effective_foreign_count_with_asian_quota(uint32_t asian_count, uint32_t non_asian_foreign_count);
static void kbo_count_active_foreign_for_asian_quota(
    uintptr_t team_ptr,
    uint32_t* out_asian_hitters,
    uint32_t* out_asian_pitchers,
    uint32_t* out_non_asian_hitters,
    uint32_t* out_non_asian_pitchers);
static const char* kbo_foreign_injury_slot_label(uint8_t slot_type);
static int kbo_foreign_injury_replacement_signing_exception_available(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_effective_count,
    uint32_t* out_effective_limit);
static int kbo_custom_foreign_policy_enabled(void);
static int kbo_custom_foreign_policy_team_allows_candidate(
    uint32_t team_id,
    uint8_t* candidate,
    uint32_t* out_effective_before,
    uint32_t* out_effective_after,
    uint32_t* out_effective_limit,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id);
static int kbo_custom_foreign_policy_can_override_original_block(uint8_t* candidate, uint32_t team_id);
static int32_t kbo_custom_foreign_policy_neutralized_count(
    uintptr_t team_ptr,
    int32_t original_count,
    int pitcher_count);
static uint8_t kbo_custom_foreign_policy_callup_allows(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t active_count,
    int32_t ootp_limit,
    int check_type);
