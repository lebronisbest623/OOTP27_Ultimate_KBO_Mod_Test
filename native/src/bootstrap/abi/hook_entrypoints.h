#ifndef KBOFIX_SRC_BOOTSTRAP_HOOK_ENTRYPOINTS_H_
#define KBOFIX_SRC_BOOTSTRAP_HOOK_ENTRYPOINTS_H_

#include <stdint.h>

__declspec(noinline) void ootp_kbo_military_service_entry_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) void ootp_kbo_military_status_update_wrapper(
    uintptr_t player_ptr, uintptr_t original_func_ptr);
__declspec(noinline) uint8_t ootp_kbo_team_add_player_guard_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);
__declspec(noinline) void ootp_kbo_amateur_assignment_batch_probe(
    uintptr_t player_list_ptr,
    int32_t player_count,
    uintptr_t source_team_ptr);
__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr, int32_t team_id, uint16_t year_hint, uintptr_t original_func_ptr);
__declspec(noinline) uint8_t ootp_kbo_player_offer_eligibility_wrapper(
    uintptr_t player_ptr, int32_t team_id, int32_t flag, uintptr_t original_func_ptr);
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
__declspec(noinline) void ootp_kbo_foreign_fa_demand_baseline_prepare_wrapper(
    uintptr_t financials_ptr, uintptr_t player_ptr, uint32_t source_rva);
__declspec(noinline) uint8_t ootp_kbo_player_action_eligibility_wrapper(
    uintptr_t action_context, int32_t action_id, uint8_t strict_check, uintptr_t original_func_ptr);
__declspec(noinline) int ootp_kbo_fa_signing_branch_wrapper(
    uintptr_t player_ptr, uintptr_t team_ptr);
__declspec(noinline) void ootp_kbo_fa_signing_success_post_wrapper(
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
__declspec(noinline) uint8_t ootp_kbo_intl_established_fa_generation_filter_allows_wrapper(
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
__declspec(noinline) uint8_t ootp_kbo_roster_move_active_trace_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, uintptr_t arg3, uintptr_t arg4,
    uintptr_t arg5, uintptr_t arg6, uintptr_t arg7, uintptr_t arg8);
__declspec(noinline) uint8_t ootp_kbo_roster_move_secondary_trace_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, uintptr_t arg3, uintptr_t arg4,
    uintptr_t arg5, uintptr_t arg6, uintptr_t arg7, uintptr_t arg8);
__declspec(noinline) uint8_t ootp_kbo_roster_move_assignment_trace_wrapper(
    uintptr_t team_ptr, uintptr_t player_ptr, uintptr_t arg3, uintptr_t arg4,
    uintptr_t arg5, uintptr_t arg6, uintptr_t arg7, uintptr_t arg8);
__declspec(noinline) double ootp_kbo_player_eval_double_trace_wrapper(
    uintptr_t player_ptr,
    uint8_t arg2,
    uint8_t arg3,
    uint8_t arg4);
__declspec(noinline) uintptr_t ootp_kbo_player_eval_cache_trace_wrapper(
    uintptr_t player_ptr,
    int32_t arg2,
    int32_t arg3,
    uint16_t arg4);
__declspec(noinline) int32_t ootp_kbo_ai_player_quality_trace_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uint8_t arg3,
    uint8_t arg4);
__declspec(noinline) int32_t ootp_kbo_ai_roster_role_check_trace_wrapper(
    uintptr_t player_ptr,
    int32_t arg2);
__declspec(noinline) int32_t ootp_kbo_ai_roster_post_sort_gate_score_trace_wrapper(
    uintptr_t player_ptr);
__declspec(noinline) uint8_t ootp_kbo_ai_roster_eligibility_trace_wrapper(
    uintptr_t player_ptr,
    uint8_t arg2,
    uint32_t arg3,
    uint8_t arg4,
    int32_t arg5,
    uint8_t arg6);
__declspec(noinline) uint8_t ootp_kbo_ai_roster_availability_trace_wrapper(
    uintptr_t player_ptr,
    int32_t arg2);
__declspec(noinline) void ootp_kbo_ai_roster_f65_update_trace_wrapper(
    uintptr_t context_ptr,
    uintptr_t player_ptr,
    int32_t arg3);
__declspec(noinline) int32_t ootp_kbo_ai_team_player_fit_trace_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    int32_t arg4,
    uint8_t arg5);
__declspec(noinline) uintptr_t ootp_kbo_player_team_status_lookup_trace_wrapper(
    uintptr_t player_ptr,
    int32_t team_id);
__declspec(noinline) uintptr_t ootp_kbo_player_default_status_lookup_trace_wrapper(
    uintptr_t player_ptr);
__declspec(noinline) uint8_t ootp_kbo_pointer_vector_push_trace_wrapper(
    uintptr_t vector_ptr,
    uintptr_t value_ptr);
__declspec(noinline) void ootp_kbo_pointer_vector_sort_trace_wrapper(
    uintptr_t vector_ptr,
    uintptr_t comparator_ptr,
    uintptr_t sort_arg);
__declspec(noinline) int32_t ootp_kbo_ai_roster_priority_compare_wrapper(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg);
__declspec(noinline) int32_t ootp_kbo_ai_roster_type_compare_wrapper(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg);
__declspec(noinline) int32_t ootp_kbo_ai_roster_score_compare_wrapper(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg);
__declspec(noinline) uintptr_t ootp_kbo_ai_roster_select_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint);
__declspec(noinline) void ootp_kbo_ai_roster_primary_apply_flow_trace_wrapper(
    uintptr_t context_ptr);
__declspec(noinline) void ootp_kbo_ai_roster_secondary_main_flow_trace_wrapper(
    uintptr_t context_ptr);
__declspec(noinline) void ootp_kbo_ai_roster_secondary_alt_flow_trace_wrapper(
    uintptr_t context_ptr);
__declspec(noinline) void ootp_kbo_ai_roster_mark_selected_trace_wrapper(
    uintptr_t context_ptr,
    uintptr_t player_ptr,
    uintptr_t slot_block_ptr);
__declspec(noinline) void ootp_kbo_ai_roster_selection_reconcile_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr);
__declspec(noinline) void ootp_kbo_ai_roster_apply_selection_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code);
__declspec(noinline) void ootp_kbo_season_phase_write_probe(
    uintptr_t league_ptr,
    uint32_t value,
    uint32_t site_rva);
__declspec(noinline) int ootp_kbo_seed_single_division_allstar_candidate_teams(
    uintptr_t league_ptr,
    void* left_team_vector,
    void* right_team_vector,
    uintptr_t vector_push_back_ptr);
__declspec(noinline) int ootp_kbo_allstar_candidate_push_filter(
    uint32_t side_index,
    uintptr_t player_ptr,
    void* candidate_vector,
    uintptr_t vector_push_back_ptr);
__declspec(noinline) void ootp_kbo_enable_allstar_setting(uintptr_t league_ptr);
__declspec(noinline) void ootp_kbo_prepare_allstar_events(uintptr_t league_ptr);
__declspec(noinline) void ootp_kbo_prepare_allstar_voting_begin(uintptr_t league_ptr, uintptr_t allstar_team_setup_ptr);
__declspec(noinline) int ootp_kbo_allow_single_division_allstar_prep(uintptr_t league_ptr);
__declspec(noinline) int ootp_kbo_allow_single_division_allstar_roster(uintptr_t league_ptr);
__declspec(noinline) int ootp_kbo_allow_single_division_allstar_team_setup(uintptr_t league_ptr);
__declspec(noinline) int ootp_kbo_capture_allstar_schedule_import_league(uintptr_t league_ptr);

#endif
