#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_H_

void kbo_set_team_add_player_guard_trampoline(void* trampoline);
void kbo_clear_team_add_player_guard_trampoline(void);
void kbo_set_roster_move_active_trace_trampoline(void* trampoline);
void kbo_set_roster_move_secondary_trace_trampoline(void* trampoline);
void kbo_set_roster_move_assignment_trace_trampoline(void* trampoline);
void kbo_set_player_eval_double_trace_trampoline(void* trampoline);
void kbo_set_player_eval_cache_trace_trampoline(void* trampoline);
void kbo_set_ai_player_quality_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_role_check_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_post_sort_gate_score_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_eligibility_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_availability_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_f65_update_trace_trampoline(void* trampoline);
void kbo_set_ai_team_player_fit_trace_trampoline(void* trampoline);
void kbo_set_player_team_status_lookup_trace_trampoline(void* trampoline);
void kbo_set_player_team_status_by_id_lookup_fn(void* fn);
void kbo_set_pointer_vector_push_trace_trampoline(void* trampoline);
void kbo_set_pointer_vector_sort_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_priority_compare_trampoline(void* trampoline);
void kbo_set_ai_roster_type_compare_trampoline(void* trampoline);
void kbo_set_ai_roster_score_compare_trampoline(void* trampoline);
void kbo_set_ai_roster_select_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_primary_apply_flow_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_secondary_main_flow_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_secondary_alt_flow_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_mark_selected_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_selection_reconcile_trace_trampoline(void* trampoline);
void kbo_set_ai_roster_apply_selection_trace_trampoline(void* trampoline);
void kbo_set_player_clear_team_trace_trampoline(void* trampoline);
void kbo_set_player_set_team_trace_trampoline(void* trampoline);
uint8_t kbo_team_add_player_guard_call_original(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);

#endif
