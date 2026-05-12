#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "../../amateur_player_quality/api/amateur_player_quality.h"
#include "../../bootstrap/abi/hook_entrypoints.h"
#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/logging/core_log.h"
#include "../../fa_compensation/history/fa_compensation_history.h"
#include "../../fa_filing/fa_filing.h"
#include "../../foreign/common/policy/foreign_waiver_policy.h"
#include "../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../foreign/injury/api/foreign_injury_labels.h"
#include "../../military_service/players/guards/military_team_add_guard.h"
#include "../../military_service/players/team_policy/military_service_team_policy.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../assignment/org_query/team_org_assignment_query.h"
#include "../lookup/team_lookup.h"
#include "team_add_player_guard.h"

typedef uint8_t (__fastcall *OotpKboTeamAddPlayerExFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8);
typedef double (__fastcall *OotpKboPlayerEvalDoubleFn)(
    uintptr_t player_ptr,
    uint8_t arg2,
    uint8_t arg3,
    uint8_t arg4);
typedef uintptr_t (__fastcall *OotpKboPlayerEvalCacheFn)(
    uintptr_t player_ptr,
    int32_t arg2,
    int32_t arg3,
    uint16_t arg4);
typedef int32_t (__fastcall *OotpKboAiPlayerQualityFn)(
    uintptr_t player_ptr,
    int32_t team_id,
    uint8_t arg3,
    uint8_t arg4);
typedef int32_t (__fastcall *OotpKboAiRosterRoleCheckFn)(
    uintptr_t player_ptr,
    int32_t arg2);
typedef int32_t (__fastcall *OotpKboAiRosterPostSortGateScoreFn)(
    uintptr_t player_ptr);
typedef uint8_t (__fastcall *OotpKboAiRosterEligibilityFn)(
    uintptr_t player_ptr,
    uint8_t arg2,
    uint32_t arg3,
    uint8_t arg4,
    int32_t arg5,
    uint8_t arg6);
typedef uint8_t (__fastcall *OotpKboAiRosterAvailabilityFn)(
    uintptr_t player_ptr,
    int32_t arg2);
typedef void (__fastcall *OotpKboAiRosterF65UpdateFn)(
    uintptr_t context_ptr,
    uintptr_t player_ptr,
    int32_t arg3);
typedef int32_t (__fastcall *OotpKboAiTeamPlayerFitFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    int32_t arg4,
    uint8_t arg5);
typedef uintptr_t (__fastcall *OotpKboPlayerTeamStatusLookupFn)(
    uintptr_t player_ptr,
    int32_t team_id);
typedef uint8_t (__fastcall *OotpKboPointerVectorPushFn)(
    uintptr_t vector_ptr,
    uintptr_t value_ptr);
typedef void (__fastcall *OotpKboPointerVectorSortFn)(
    uintptr_t vector_ptr,
    uintptr_t comparator_ptr,
    uintptr_t sort_arg);
typedef int32_t (__fastcall *OotpKboAiRosterPriorityCompareFn)(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg);
typedef int32_t (__fastcall *OotpKboAiRosterTypeCompareFn)(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg);
typedef int32_t (__fastcall *OotpKboAiRosterScoreCompareFn)(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg);
typedef uintptr_t (__fastcall *OotpKboAiRosterSelectFn)(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint);
typedef void (__fastcall *OotpKboAiRosterContextFlowFn)(
    uintptr_t context_ptr);
typedef void (__fastcall *OotpKboAiRosterMarkSelectedFn)(
    uintptr_t context_ptr,
    uintptr_t player_ptr,
    uintptr_t slot_block_ptr);
typedef void (__fastcall *OotpKboAiRosterSelectionReconcileFn)(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr);
typedef void (__fastcall *OotpKboAiRosterApplySelectionFn)(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code);
typedef void (__fastcall *OotpKboPlayerClearTeamTraceFn)(
    uintptr_t player_ptr,
    uint8_t loan_flag);
typedef void (__fastcall *OotpKboPlayerSetTeamTraceFn)(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t active_team_id,
    int32_t league_id,
    uint8_t loan_flag);

static OotpKboTeamAddPlayerExFn g_kbo_team_add_player_guard_trampoline = NULL;
static OotpKboTeamAddPlayerExFn g_kbo_roster_move_active_trace_trampoline = NULL;
static OotpKboTeamAddPlayerExFn g_kbo_roster_move_secondary_trace_trampoline = NULL;
static OotpKboTeamAddPlayerExFn g_kbo_roster_move_assignment_trace_trampoline = NULL;
static OotpKboPlayerEvalDoubleFn g_kbo_player_eval_double_trace_trampoline = NULL;
static OotpKboPlayerEvalCacheFn g_kbo_player_eval_cache_trace_trampoline = NULL;
static OotpKboAiPlayerQualityFn g_kbo_ai_player_quality_trace_trampoline = NULL;
static OotpKboAiRosterRoleCheckFn g_kbo_ai_roster_role_check_trace_trampoline = NULL;
static OotpKboAiRosterPostSortGateScoreFn g_kbo_ai_roster_post_sort_gate_score_trace_trampoline = NULL;
static OotpKboAiRosterEligibilityFn g_kbo_ai_roster_eligibility_trace_trampoline = NULL;
static OotpKboAiRosterAvailabilityFn g_kbo_ai_roster_availability_trace_trampoline = NULL;
static OotpKboAiRosterF65UpdateFn g_kbo_ai_roster_f65_update_trace_trampoline = NULL;
static OotpKboAiTeamPlayerFitFn g_kbo_ai_team_player_fit_trace_trampoline = NULL;
static OotpKboPlayerTeamStatusLookupFn g_kbo_player_team_status_lookup_trace_trampoline = NULL;
static OotpKboPlayerTeamStatusLookupFn g_kbo_player_team_status_by_id_lookup_fn = NULL;
static OotpKboPointerVectorPushFn g_kbo_pointer_vector_push_trace_trampoline = NULL;
static OotpKboPointerVectorSortFn g_kbo_pointer_vector_sort_trace_trampoline = NULL;
static OotpKboAiRosterPriorityCompareFn g_kbo_ai_roster_priority_compare_trampoline = NULL;
static OotpKboAiRosterTypeCompareFn g_kbo_ai_roster_type_compare_trampoline = NULL;
static OotpKboAiRosterScoreCompareFn g_kbo_ai_roster_score_compare_trampoline = NULL;
static OotpKboAiRosterSelectFn g_kbo_ai_roster_select_trace_trampoline = NULL;
static OotpKboAiRosterContextFlowFn g_kbo_ai_roster_primary_apply_flow_trace_trampoline = NULL;
static OotpKboAiRosterContextFlowFn g_kbo_ai_roster_secondary_main_flow_trace_trampoline = NULL;
static OotpKboAiRosterContextFlowFn g_kbo_ai_roster_secondary_alt_flow_trace_trampoline = NULL;
static OotpKboAiRosterMarkSelectedFn g_kbo_ai_roster_mark_selected_trace_trampoline = NULL;
static OotpKboAiRosterSelectionReconcileFn g_kbo_ai_roster_selection_reconcile_trace_trampoline = NULL;
static OotpKboAiRosterApplySelectionFn g_kbo_ai_roster_apply_selection_trace_trampoline = NULL;
static OotpKboPlayerClearTeamTraceFn g_kbo_player_clear_team_trace_trampoline = NULL;
static OotpKboPlayerSetTeamTraceFn g_kbo_player_set_team_trace_trampoline = NULL;
static volatile LONG g_kbo_team_add_amateur_verbose_cached = -1;
static volatile LONG g_kbo_team_add_amateur_verbose_tick = 0;
static volatile LONG g_kbo_team_add_retry_rejected_cached = -1;
static volatile LONG g_kbo_team_add_retry_rejected_tick = 0;
static volatile LONG g_kbo_foreign_eval_bias_disabled_cached = -1;
static volatile LONG g_kbo_ai_roster_foreign_sort_bias_disabled_cached = -1;
static volatile LONG g_kbo_ai_roster_foreign_f25_bias_disabled_cached = -1;
static volatile LONG g_kbo_ai_roster_foreign_role_bias_disabled_cached = -1;
static volatile LONG g_kbo_ai_roster_foreign_post_sort_bias_disabled_cached = -1;
static volatile LONG g_kbo_ai_roster_foreign_apply_rescue_disabled_cached = -1;
static volatile LONG g_kbo_ai_roster_select_live_tls_index = -1;

#define KBO_FOREIGN_ROSTER_EVAL_CALLER_RVA 0x00A91B7Du
#define KBO_AI_PLAYER_QUALITY_CALLER_A39879_RVA 0x00A39879u
#define KBO_AI_PLAYER_QUALITY_CALLER_A3993E_RVA 0x00A3993Eu
#define KBO_AI_PLAYER_QUALITY_CALLER_A39A6C_RVA 0x00A39A6Cu
#define KBO_AI_PLAYER_QUALITY_CALLER_A39C56_RVA 0x00A39C56u
#define KBO_AI_PLAYER_QUALITY_CALLER_A3AACE_RVA 0x00A3AACEu
#define KBO_AI_PLAYER_QUALITY_CALLER_A3AAEE_RVA 0x00A3AAEEu
#define KBO_AI_PLAYER_QUALITY_CALLER_A3B09E_RVA 0x00A3B09Eu
#define KBO_AI_PLAYER_QUALITY_CALLER_A3B0E5_RVA 0x00A3B0E5u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DD9A77_RVA 0x00DD9A77u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DD9AA4_RVA 0x00DD9AA4u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DD9C7E_RVA 0x00DD9C7Eu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA32C_RVA 0x00DDA32Cu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA440_RVA 0x00DDA440u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA4F0_RVA 0x00DDA4F0u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA5FE_RVA 0x00DDA5FEu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA6EE_RVA 0x00DDA6EEu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB04C_RVA 0x00DDB04Cu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB5C4_RVA 0x00DDB5C4u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB7A2_RVA 0x00DDB7A2u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDC915_RVA 0x00DDC915u
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDC9CF_RVA 0x00DDC9CFu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDCA1B_RVA 0x00DDCA1Bu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDCD5F_RVA 0x00DDCD5Fu
#define KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDCF94_RVA 0x00DDCF94u
#define KBO_AI_ROSTER_POST_SORT_GATE_SCORE_CALLER_DDC8FA_RVA 0x00DDC8FAu
#define KBO_AI_ROSTER_POST_SORT_GATE_SCORE_ACCEPT_DAYS 10
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA0E5_RVA 0x00DDA0E5u
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA1BA_RVA 0x00DDA1BAu
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA2D6_RVA 0x00DDA2D6u
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA471_RVA 0x00DDA471u
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA521_RVA 0x00DDA521u
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB0EB_RVA 0x00DDB0EBu
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB18A_RVA 0x00DDB18Au
#define KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB229_RVA 0x00DDB229u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_A3985D_RVA 0x00A3985Du
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_A3991E_RVA 0x00A3991Eu
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_A39D24_RVA 0x00A39D24u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_A3B2F2_RVA 0x00A3B2F2u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DD8C09_RVA 0x00DD8C09u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DD92CA_RVA 0x00DD92CAu
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DD96A2_RVA 0x00DD96A2u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DD98B5_RVA 0x00DD98B5u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA2F0_RVA 0x00DDA2F0u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA48B_RVA 0x00DDA48Bu
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA53B_RVA 0x00DDA53Bu
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA619_RVA 0x00DDA619u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DDB243_RVA 0x00DDB243u
#define KBO_AI_TEAM_PLAYER_FIT_CALLER_DDB309_RVA 0x00DDB309u
#define KBO_PLAYER_TEAM_STATUS_CALLER_A39DC3_RVA 0x00A39DC3u
#define KBO_PLAYER_TEAM_STATUS_CALLER_A39DDC_RVA 0x00A39DDCu
#define KBO_PLAYER_TEAM_STATUS_CALLER_A3B24C_RVA 0x00A3B24Cu
#define KBO_PLAYER_TEAM_STATUS_CALLER_A3B2CB_RVA 0x00A3B2CBu
#define KBO_PLAYER_TEAM_STATUS_CALLER_A3B3F6_RVA 0x00A3B3F6u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDA3FB_RVA 0x00DDA3FBu
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB058_RVA 0x00DDB058u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB0F7_RVA 0x00DDB0F7u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB196_RVA 0x00DDB196u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB250_RVA 0x00DDB250u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB25E_RVA 0x00DDB25Eu
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB316_RVA 0x00DDB316u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB324_RVA 0x00DDB324u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB5F5_RVA 0x00DDB5F5u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB694_RVA 0x00DDB694u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB6A2_RVA 0x00DDB6A2u
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB7CE_RVA 0x00DDB7CEu
#define KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB8C2_RVA 0x00DDB8C2u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DD971C_RVA 0x00DD971Cu
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDA49C_RVA 0x00DDA49Cu
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDA54C_RVA 0x00DDA54Cu
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDA638_RVA 0x00DDA638u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDAA7F_RVA 0x00DDAA7Fu
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB06A_RVA 0x00DDB06Au
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB109_RVA 0x00DDB109u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB1A8_RVA 0x00DDB1A8u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB270_RVA 0x00DDB270u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB336_RVA 0x00DDB336u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB425_RVA 0x00DDB425u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB510_RVA 0x00DDB510u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB62C_RVA 0x00DDB62Cu
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB6D9_RVA 0x00DDB6D9u
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB80A_RVA 0x00DDB80Au
#define KBO_POINTER_VECTOR_PUSH_CALLER_DDB8D9_RVA 0x00DDB8D9u
#define KBO_POINTER_VECTOR_SORT_CALLER_DDC4BF_RVA 0x00DDC4BFu
#define KBO_POINTER_VECTOR_SORT_CALLER_DDC76D_RVA 0x00DDC76Du
#define KBO_POINTER_VECTOR_SORT_CALLER_DDC8C5_RVA 0x00DDC8C5u
#define KBO_POINTER_VECTOR_SORT_CALLER_DDCA82_RVA 0x00DDCA82u
#define KBO_POINTER_VECTOR_SORT_CALLER_DDCB1F_RVA 0x00DDCB1Fu
#define KBO_POINTER_VECTOR_SORT_CALLER_DDCCE1_RVA 0x00DDCCE1u
#define KBO_POINTER_VECTOR_SORT_CALLER_DDCF1B_RVA 0x00DDCF1Bu
#define KBO_POINTER_VECTOR_SORT_CALLER_DDD055_RVA 0x00DDD055u
#define KBO_AI_ROSTER_SELECT_COMPARATOR_909EC0_RVA 0x00909EC0u
#define KBO_AI_ROSTER_SELECT_COMPARATOR_909FA0_RVA 0x00909FA0u
#define KBO_AI_ROSTER_SELECT_COMPARATOR_909F30_RVA 0x00909F30u
#define KBO_AI_ROSTER_SELECT_COMPARATOR_917380_RVA 0x00917380u
#define KBO_AI_ROSTER_SELECT_SCORE_COMPARATOR_RVA 0x0091E920u
#define KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET 0x108u
#define KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET 0x578u
#define KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET 0x4f8u
#define KBO_AI_ROSTER_CONTEXT_RULES_PTR_OFFSET 0x5b8u
#define KBO_AI_ROSTER_CONTEXT_LEAGUE_INFO_PTR_OFFSET 0xd8u
#define KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET 0x7b0u
#define KBO_AI_ROSTER_CONTEXT_SECONDARY_SLOT_OFFSET 0x7b2u
#define KBO_AI_ROSTER_SLOT_BLOCK_FLAG_OFFSET 0x187fu
#define KBO_AI_ROSTER_SLOT_BLOCK_DEPTH_ID_TABLE_OFFSET 0x1864u
#define KBO_AI_ROSTER_SLOT_BLOCK_DEPTH_ID_COUNT 6
#define KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET 0x3ca8u
#define KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID_OFFSET 0x3df8u
#define KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID2_OFFSET 0x3dfcu
#define KBO_AI_ROSTER_SELECT_FOREIGN_SUMMARY_SLOTS 3
#define KBO_AI_ROSTER_SELECT_SCAN_LIMIT 256
#define KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT 256
#define KBO_AI_ROSTER_LOCAL_DETAIL_TRACE_LIMIT 24
#define KBO_AI_ROSTER_LOCAL_FOREIGN_SUMMARY_SLOTS 3
#define KBO_AI_ROSTER_SORT_TOP_SUMMARY_SLOTS 6
#define KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET 0xfe0u
#define KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET 0xfe4u
#define KBO_AI_ROSTER_SOURCE_GATE_TRACE_LIMIT 8
#define KBO_AI_ROSTER_SOURCE_BRANCH_DDA_STATUS4_OR_26 0x0001u
#define KBO_AI_ROSTER_SOURCE_BRANCH_DDA_STATUS5_OR_26 0x0002u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS24_2 0x0004u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_2 0x0008u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS11 0x0010u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS4 0x0020u
#define KBO_AI_ROSTER_SOURCE_BRANCH_DEPTH_SLOT 0x0040u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_9 0x0080u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS7 0x0100u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_12 0x0200u
#define KBO_AI_ROSTER_SOURCE_BRANCH_STATUS24_5 0x0400u
#define KBO_AI_ROSTER_FOREIGN_SORT_F06_BONUS 64u
#define KBO_AI_ROSTER_FOREIGN_SORT_C98_BONUS 96u
#define KBO_AI_ROSTER_FOREIGN_SCORE_FE_BONUS 128u
#define KBO_AI_ROSTER_FOREIGN_F25_MIN 100u
#define KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT 16
#define KBO_AI_ROSTER_APPLY_RESCUE_SHIELD_MS 5000u

#define KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX 512

typedef struct KboAiRosterForeignCandidateSummary {
    int32_t index;
    uintptr_t player_ptr;
    uint32_t player_id;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t league_id;
    uint32_t default_team_id;
    uint32_t status24;
    uint32_t status25;
    uint32_t status26;
    uint32_t c78;
    uint32_t c79;
    uint32_t f25;
    int32_t score_fe0;
    int32_t score_fe4;
    uint32_t value_ac;
    int16_t overall;
    int16_t talent;
    int16_t ratings;
} KboAiRosterForeignCandidateSummary;

typedef struct KboAiRosterSortCandidateSummary {
    int32_t index;
    int32_t push_index;
    uint32_t push_rva;
    uint32_t player_id;
    uint32_t foreign;
    uint32_t target;
    uint32_t status24;
    uint32_t status25;
    uint32_t status26;
    uint32_t f25;
    uint32_t f62;
    uint32_t f65;
    uint32_t sort_f06;
    uint32_t sort_c98;
    uint32_t sort_bce;
    uint32_t role_mask;
    uint32_t role_count;
    int32_t score_fe0;
    int32_t score_fe4;
    int16_t overall;
    int16_t ratings;
} KboAiRosterSortCandidateSummary;

typedef struct KboAiRosterSelectLiveTrace {
    int active;
    uintptr_t context_ptr;
    int32_t slot_index;
    int32_t depth_hint;
    uint32_t caller_rva;
    uintptr_t first_vector_ptr;
    uintptr_t last_vector_ptr;
    int32_t vector_switches;
    uint32_t first_push_rva;
    uint32_t last_push_rva;
    int32_t push_count;
    int32_t inserted_count;
    int32_t candidate_count;
    int32_t foreign_count;
    int32_t target_count;
    uintptr_t candidate_values[KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT];
    uint32_t candidate_push_rvas[KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT];
    KboAiRosterForeignCandidateSummary foreign_summaries[KBO_AI_ROSTER_LOCAL_FOREIGN_SUMMARY_SLOTS];
    uint32_t last_sort_caller_rva;
    uint32_t last_sort_comparator_rva;
    int32_t sort_count;
    int32_t score_sort_count;
    int32_t last_sort_count;
    int32_t last_sort_foreign_count;
    int32_t last_sort_target_count;
    int32_t last_sorted_rank_by_candidate[KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT];
    int32_t foreign_sort_bias_active;
    int32_t foreign_sort_bias_bonus;
    int32_t foreign_sort_bias_mixed_compare_count;
    int32_t foreign_sort_bias_decision_count;
} KboAiRosterSelectLiveTrace;

typedef struct KboAiRosterApplyRescueSlot {
    uintptr_t context_ptr;
    uintptr_t slot_block_ptr;
    uintptr_t player_ptr;
    uint32_t player_id;
    uint16_t slot_index;
    uint16_t target_slot;
    DWORD tick;
} KboAiRosterApplyRescueSlot;

typedef struct KboTeamAddAmateurLeagueCacheEntry {
    uintptr_t team_ptr;
    uint32_t league_id;
} KboTeamAddAmateurLeagueCacheEntry;

static KboTeamAddAmateurLeagueCacheEntry g_kbo_team_add_amateur_league_cache[KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX];
static KboAiRosterApplyRescueSlot g_kbo_ai_roster_apply_rescue_slots[KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT];
static volatile LONG g_kbo_team_add_amateur_league_cache_count = 0;
static volatile LONG g_kbo_team_add_amateur_league_cache_lock = 0;
static volatile LONG g_kbo_ai_roster_apply_rescue_slot_next = 0;

static int kbo_team_add_cached_bool_flag(
    const char* file_name,
    volatile LONG* cached_value,
    volatile LONG* cached_tick,
    DWORD ttl_ms)
{
    DWORD now = GetTickCount();
    LONG value = *cached_value;
    LONG tick = *cached_tick;
    if (value >= 0 && now - (DWORD)tick < ttl_ms) {
        return value != 0;
    }

    int fresh = read_kbo_localappdata_flag_file(file_name) ? 1 : 0;
    InterlockedExchange(cached_value, fresh);
    InterlockedExchange(cached_tick, (LONG)now);
    return fresh;
}

static uint32_t kbo_team_add_cached_amateur_league_id(uint8_t* team)
{
    if (team == NULL) {
        return 0u;
    }

    uintptr_t team_ptr = (uintptr_t)team;
    LONG count = g_kbo_team_add_amateur_league_cache_count;
    if (count > KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX) {
        count = KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_team_add_amateur_league_cache[i].team_ptr == team_ptr) {
            return g_kbo_team_add_amateur_league_cache[i].league_id;
        }
    }

    uint32_t league_id = kbo_resolve_amateur_assignment_league_id_for_team_ptr(team);

    while (InterlockedCompareExchange(&g_kbo_team_add_amateur_league_cache_lock, 1, 0) != 0) {
        Sleep(0);
    }
    count = g_kbo_team_add_amateur_league_cache_count;
    if (count > KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX) {
        count = KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX;
    }
    for (LONG i = 0; i < count; i++) {
        if (g_kbo_team_add_amateur_league_cache[i].team_ptr == team_ptr) {
            uint32_t cached = g_kbo_team_add_amateur_league_cache[i].league_id;
            InterlockedExchange(&g_kbo_team_add_amateur_league_cache_lock, 0);
            return cached;
        }
    }
    LONG slot = count;
    if (slot >= KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX) {
        slot = (LONG)(team_ptr % KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX);
    } else {
        InterlockedExchange(&g_kbo_team_add_amateur_league_cache_count, count + 1);
    }
    g_kbo_team_add_amateur_league_cache[slot].team_ptr = team_ptr;
    g_kbo_team_add_amateur_league_cache[slot].league_id = league_id;
    InterlockedExchange(&g_kbo_team_add_amateur_league_cache_lock, 0);
    return league_id;
}

static int kbo_team_add_amateur_verbose_log_enabled_cached(void)
{
    return kbo_team_add_cached_bool_flag(
        "enable_amateur_assignment_verbose_log.txt",
        &g_kbo_team_add_amateur_verbose_cached,
        &g_kbo_team_add_amateur_verbose_tick,
        5000u);
}

static int kbo_team_add_retry_rejected_targets_enabled_cached(void)
{
    return kbo_team_add_cached_bool_flag(
        "enable_amateur_assignment_retry_rejected_targets.txt",
        &g_kbo_team_add_retry_rejected_cached,
        &g_kbo_team_add_retry_rejected_tick,
        5000u);
}

static int kbo_foreign_roster_eval_bias_enabled(void)
{
    LONG disabled = g_kbo_foreign_eval_bias_disabled_cached;
    if (disabled < 0) {
        disabled = read_kbo_localappdata_flag_file("disable_foreign_roster_eval_bias.txt") ? 1 : 0;
        InterlockedCompareExchange(&g_kbo_foreign_eval_bias_disabled_cached, disabled, -1);
        disabled = g_kbo_foreign_eval_bias_disabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && disabled != 1;
}

static int kbo_ai_roster_foreign_sort_bias_enabled(void)
{
    LONG disabled = g_kbo_ai_roster_foreign_sort_bias_disabled_cached;
    if (disabled < 0) {
        disabled = read_kbo_localappdata_flag_file("disable_ai_roster_foreign_sort_bias.txt") ? 1 : 0;
        InterlockedCompareExchange(&g_kbo_ai_roster_foreign_sort_bias_disabled_cached, disabled, -1);
        disabled = g_kbo_ai_roster_foreign_sort_bias_disabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && disabled != 1;
}

static int kbo_ai_roster_foreign_f25_bias_enabled(void)
{
    LONG disabled = g_kbo_ai_roster_foreign_f25_bias_disabled_cached;
    if (disabled < 0) {
        disabled = read_kbo_localappdata_flag_file("disable_ai_roster_foreign_f25_bias.txt") ? 1 : 0;
        InterlockedCompareExchange(&g_kbo_ai_roster_foreign_f25_bias_disabled_cached, disabled, -1);
        disabled = g_kbo_ai_roster_foreign_f25_bias_disabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && disabled != 1;
}

static int kbo_ai_roster_foreign_role_bias_enabled(void)
{
    LONG disabled = g_kbo_ai_roster_foreign_role_bias_disabled_cached;
    if (disabled < 0) {
        disabled = read_kbo_localappdata_flag_file("disable_ai_roster_foreign_role_bias.txt") ? 1 : 0;
        InterlockedCompareExchange(&g_kbo_ai_roster_foreign_role_bias_disabled_cached, disabled, -1);
        disabled = g_kbo_ai_roster_foreign_role_bias_disabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && disabled != 1;
}

static int kbo_ai_roster_foreign_post_sort_bias_enabled(void)
{
    LONG disabled = g_kbo_ai_roster_foreign_post_sort_bias_disabled_cached;
    if (disabled < 0) {
        disabled = read_kbo_localappdata_flag_file("disable_ai_roster_foreign_post_sort_bias.txt") ? 1 : 0;
        InterlockedCompareExchange(&g_kbo_ai_roster_foreign_post_sort_bias_disabled_cached, disabled, -1);
        disabled = g_kbo_ai_roster_foreign_post_sort_bias_disabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && disabled != 1;
}

static int kbo_ai_roster_foreign_apply_rescue_enabled(void)
{
    LONG disabled = g_kbo_ai_roster_foreign_apply_rescue_disabled_cached;
    if (disabled < 0) {
        disabled = read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue.txt") ? 1 : 0;
        InterlockedCompareExchange(&g_kbo_ai_roster_foreign_apply_rescue_disabled_cached, disabled, -1);
        disabled = g_kbo_ai_roster_foreign_apply_rescue_disabled_cached;
    }
    return kbo_custom_foreign_policy_enabled() && disabled != 1;
}

static int kbo_ai_roster_foreign_apply_rescue_team_add_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && read_kbo_localappdata_flag_file("enable_ai_roster_foreign_apply_rescue_team_add.txt")
        && !read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue_team_add.txt");
}

static int kbo_ai_roster_foreign_source_select_rescue_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && read_kbo_localappdata_flag_file("enable_ai_roster_foreign_source_select_rescue.txt")
        && !read_kbo_localappdata_flag_file("disable_ai_roster_foreign_source_select_rescue.txt");
}

static uint32_t kbo_ai_roster_foreign_sort_f06_bonus(void)
{
    return KBO_AI_ROSTER_FOREIGN_SORT_F06_BONUS;
}

static uint32_t kbo_ai_roster_foreign_sort_c98_bonus(void)
{
    return KBO_AI_ROSTER_FOREIGN_SORT_C98_BONUS;
}

static uint32_t kbo_ai_roster_foreign_score_fe_bonus(void)
{
    return KBO_AI_ROSTER_FOREIGN_SCORE_FE_BONUS;
}

static double kbo_apply_foreign_roster_eval_bias(
    double result,
    uint32_t caller_rva,
    uint8_t* player,
    uint32_t current_team_id,
    uint32_t active_team_id,
    uint32_t league_id,
    int* out_adjusted)
{
    if (out_adjusted != NULL) {
        *out_adjusted = 0;
    }
    if (caller_rva != KBO_FOREIGN_ROSTER_EVAL_CALLER_RVA
            || player == NULL
            || current_team_id == 0u
            || active_team_id == 0u
            || !kbo_foreign_roster_eval_bias_enabled()) {
        return result;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int league_matches = league_id == 0u
        || kbo_league_id == 0u
        || league_id == kbo_league_id
        || league_id == kbo_league_id + 1u;
    if (!league_matches) {
        return result;
    }

    int rounded = (int)(result + 0.00001);
    if (rounded < 1 || rounded >= 6) {
        return result;
    }

    if (out_adjusted != NULL) {
        *out_adjusted = 1;
    }
    return 6.0;
}

static int kbo_ai_team_player_fit_bias_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && !read_kbo_localappdata_flag_file("disable_ai_team_player_fit_bias.txt");
}

static int kbo_ai_team_player_fit_caller_is_minor_foreign_candidate_prune_target(uint32_t caller_rva)
{
    return caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_A3991E_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_A39D24_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_DD96A2_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_DD98B5_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA48B_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA53B_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA619_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_DDB243_RVA
        || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_DDB309_RVA;
}

static int kbo_ai_team_player_fit_caller_is_minor_foreign_final_target(uint32_t caller_rva)
{
    (void)caller_rva;
    return 0;
}

static int kbo_ai_team_player_fit_caller_is_minor_foreign_upgrade_target(uint32_t caller_rva)
{
    (void)caller_rva;
    return 0;
}

static int kbo_ai_foreign_status_write_probe_allows_player(uint32_t player_id)
{
    if (!read_kbo_localappdata_flag_file("enable_foreign_status_write_probe.txt")) {
        return 0;
    }
    return player_id == 5320u || player_id == 5381u;
}

static int kbo_ai_roster_research_target_player_id(uint32_t player_id)
{
    return player_id == 5320u || player_id == 5381u || player_id == 5404u
        || player_id == 5368u || player_id == 5293u || player_id == 5417u;
}

static int kbo_ai_foreign_value_ac_probe_enabled(void)
{
    return !read_kbo_localappdata_flag_file("disable_foreign_value_ac_probe.txt");
}

static int kbo_ai_foreign_final_roster_candidate_probe_enabled(void)
{
    return !read_kbo_localappdata_flag_file("disable_foreign_final_roster_candidate_probe.txt");
}

static int kbo_ai_foreign_status_lookup_caller_is_final_candidate(uint32_t caller_rva)
{
    return caller_rva == KBO_PLAYER_TEAM_STATUS_CALLER_A39DC3_RVA
        || caller_rva == KBO_PLAYER_TEAM_STATUS_CALLER_A39DDC_RVA
        || caller_rva == KBO_PLAYER_TEAM_STATUS_CALLER_A3B24C_RVA;
}

static int kbo_ai_foreign_status_lookup_caller_is_status25_11_rescue_target(uint32_t caller_rva)
{
    return caller_rva == KBO_PLAYER_TEAM_STATUS_CALLER_A3B2CB_RVA
        || caller_rva == KBO_PLAYER_TEAM_STATUS_CALLER_A3B3F6_RVA;
}

static int kbo_ai_foreign_default_status_caller_is_status25_11_rescue_target(uint32_t caller_rva)
{
    return caller_rva == KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB196_RVA;
}

static const char* kbo_player_default_status_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDA3FB_RVA:
        return "roster_default_status_scan";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB058_RVA:
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB0F7_RVA:
        return "roster_default_status24_2_gate";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB196_RVA:
        return "roster_default_status25_2_gate";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB250_RVA:
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB25E_RVA:
        return "roster_default_status11_gate";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB316_RVA:
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB324_RVA:
        return "roster_default_status4_gate";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB5F5_RVA:
        return "roster_default_status25_9_gate";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB694_RVA:
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB6A2_RVA:
        return "roster_default_status7_gate";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB7CE_RVA:
        return "roster_default_status25_12_gate";
    case KBO_PLAYER_DEFAULT_STATUS_CALLER_DDB8C2_RVA:
        return "roster_default_status24_5_gate";
    default:
        return "other";
    }
}

static int kbo_pointer_vector_push_caller_is_ai_roster_candidate(uint32_t caller_rva)
{
    return caller_rva >= 0x00DD9000u && caller_rva <= 0x00DDC700u;
}

static const char* kbo_pointer_vector_push_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_POINTER_VECTOR_PUSH_CALLER_DD971C_RVA:
        return "push_prebranch_status0_5_candidate";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDA49C_RVA:
        return "push_initial_status4_or_26_candidate";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDA54C_RVA:
        return "push_status5_depth_candidate";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDA638_RVA:
        return "push_status4_retry_candidate";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDAA7F_RVA:
        return "push_status5_retry_candidate";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB06A_RVA:
        return "push_status24_2_initial";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB109_RVA:
        return "push_status24_2_retry";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB1A8_RVA:
        return "push_status25_2";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB270_RVA:
        return "push_status11";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB336_RVA:
        return "push_status4";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB425_RVA:
        return "push_status11_nonduplicate";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB510_RVA:
        return "push_depth_slot_candidate";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB62C_RVA:
        return "push_status25_9";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB6D9_RVA:
        return "push_status7";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB80A_RVA:
        return "push_status25_12";
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB8D9_RVA:
        return "push_status24_5";
    default:
        return "roster_candidate_push";
    }
}

static const char* kbo_pointer_vector_sort_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_POINTER_VECTOR_SORT_CALLER_DDC4BF_RVA:
        return "source_candidate_sort_909ec0";
    case KBO_POINTER_VECTOR_SORT_CALLER_DDC76D_RVA:
        return "league2_f25_type_sort_909fa0";
    case KBO_POINTER_VECTOR_SORT_CALLER_DDC8C5_RVA:
        return "depth_hint_priority_sort_917380";
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCA82_RVA:
        return "late_type2_sort_909fa0";
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCB1F_RVA:
        return "secondary_pool_sort";
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCCE1_RVA:
        return "select_primary_fe0_f25_sort";
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCF1B_RVA:
        return "select_reverse_last_candidate_sort";
    case KBO_POINTER_VECTOR_SORT_CALLER_DDD055_RVA:
        return "select_fallback_top_candidate_sort";
    default:
        return "other_sort";
    }
}

static const char* kbo_pointer_vector_sort_comparator_phase(uint32_t comparator_rva)
{
    switch (comparator_rva) {
    case KBO_AI_ROSTER_SELECT_COMPARATOR_909EC0_RVA:
        return "cmp_909ec0";
    case KBO_AI_ROSTER_SELECT_COMPARATOR_909FA0_RVA:
        return "cmp_909fa0";
    case KBO_AI_ROSTER_SELECT_COMPARATOR_909F30_RVA:
        return "cmp_909f30";
    case KBO_AI_ROSTER_SELECT_COMPARATOR_917380_RVA:
        return "cmp_917380";
    case KBO_AI_ROSTER_SELECT_SCORE_COMPARATOR_RVA:
        return "cmp_score_91e920";
    default:
        return "cmp_other";
    }
}

static int kbo_pointer_vector_sort_trace_is_roster_sort(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_POINTER_VECTOR_SORT_CALLER_DDC4BF_RVA:
    case KBO_POINTER_VECTOR_SORT_CALLER_DDC76D_RVA:
    case KBO_POINTER_VECTOR_SORT_CALLER_DDC8C5_RVA:
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCA82_RVA:
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCB1F_RVA:
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCCE1_RVA:
    case KBO_POINTER_VECTOR_SORT_CALLER_DDCF1B_RVA:
    case KBO_POINTER_VECTOR_SORT_CALLER_DDD055_RVA:
        return 1;
    default:
        return 0;
    }
}

static int kbo_ai_roster_target_player_id(uint32_t player_id)
{
    return player_id == 5320u || player_id == 5381u || player_id == 5404u
        || player_id == 5368u || player_id == 5293u || player_id == 5417u;
}

static int32_t kbo_ai_roster_push_phase_order(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_POINTER_VECTOR_PUSH_CALLER_DD971C_RVA:
        return 0;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDA49C_RVA:
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDA54C_RVA:
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDA638_RVA:
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDAA7F_RVA:
        return 1;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB06A_RVA:
        return 2;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB109_RVA:
        return 3;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB1A8_RVA:
        return 4;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB270_RVA:
        return 5;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB336_RVA:
        return 6;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB425_RVA:
        return 7;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB510_RVA:
        return 8;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB62C_RVA:
        return 9;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB6D9_RVA:
        return 10;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB80A_RVA:
        return 11;
    case KBO_POINTER_VECTOR_PUSH_CALLER_DDB8D9_RVA:
        return 12;
    default:
        return 0;
    }
}

static const char* kbo_ai_roster_role_check_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DD9A77_RVA:
        return "select_role3_gate_a";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DD9AA4_RVA:
        return "select_role4_gate_a";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DD9C7E_RVA:
        return "select_role3_gate_b";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA32C_RVA:
        return "select_role4_gate_b";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA440_RVA:
        return "source_initial_role_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA4F0_RVA:
        return "source_status5_role_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA5FE_RVA:
        return "source_status9_role_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA6EE_RVA:
        return "source_status7_role_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB04C_RVA:
        return "source_status24_2_role_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB5C4_RVA:
        return "source_status25_9_role_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB7A2_RVA:
        return "source_status25_12_role_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDC915_RVA:
        return "select_late_role_dynamic_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDC9CF_RVA:
        return "select_late_role5_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDCA1B_RVA:
        return "select_late_role4_gate";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDCD5F_RVA:
        return "select_return_role4_gate_a";
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDCF94_RVA:
        return "select_return_role4_gate_b";
    default:
        return "other";
    }
}

static const char* kbo_ai_roster_post_sort_gate_score_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_AI_ROSTER_POST_SORT_GATE_SCORE_CALLER_DDC8FA_RVA:
        return "select_return_recent_move_days_gate";
    default:
        return "other";
    }
}

static int kbo_ai_roster_role_check_blocks_source_push(uint32_t caller_rva, int32_t result)
{
    switch (caller_rva) {
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA440_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA4F0_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB04C_RVA:
        return result != 0;
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA5FE_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB5C4_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB7A2_RVA:
        return result >= 2;
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA6EE_RVA:
        return result > 2;
    default:
        return -1;
    }
}

static int kbo_ai_roster_source_role_gate_nonblocking_result(
    uint32_t caller_rva,
    int32_t result,
    int32_t* out_result)
{
    if (out_result != NULL) {
        *out_result = result;
    }

    switch (caller_rva) {
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA440_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA4F0_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB04C_RVA:
        if (result != 0) {
            if (out_result != NULL) {
                *out_result = 0;
            }
            return 1;
        }
        return 0;
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA5FE_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB5C4_RVA:
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDB7A2_RVA:
        if (result >= 2) {
            if (out_result != NULL) {
                *out_result = 1;
            }
            return 1;
        }
        return 0;
    case KBO_AI_ROSTER_ROLE_CHECK_CALLER_DDA6EE_RVA:
        if (result > 2) {
            if (out_result != NULL) {
                *out_result = 2;
            }
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

static int32_t kbo_pointer_vector_count(uintptr_t vector_ptr)
{
    if (vector_ptr == 0u || !memory_range_readable((void*)vector_ptr, 0x18u)) {
        return -1;
    }

    uint8_t* vector = (uint8_t*)vector_ptr;
    int32_t count = *(int32_t*)(vector + 0x0cu);
    if (count < 0 || count > 10000) {
        return -1;
    }
    uintptr_t values = *(uintptr_t*)vector;
    if (count > 0 && (values == 0u || !memory_range_readable((void*)values, (SIZE_T)count * sizeof(uintptr_t)))) {
        return -1;
    }
    return count;
}

static int kbo_pointer_vector_contains_value(uintptr_t vector_ptr, uintptr_t value_ptr, int32_t count)
{
    if (vector_ptr == 0u || value_ptr == 0u || count <= 0 || count > 10000
            || !memory_range_readable((void*)vector_ptr, 0x18u)) {
        return 0;
    }

    uintptr_t values = *(uintptr_t*)vector_ptr;
    if (values == 0u || !memory_range_readable((void*)values, (SIZE_T)count * sizeof(uintptr_t))) {
        return 0;
    }
    uintptr_t* items = (uintptr_t*)values;
    for (int32_t i = 0; i < count; i++) {
        if (items[i] == value_ptr) {
            return 1;
        }
    }
    return 0;
}

static uintptr_t kbo_pointer_vector_value_at(uintptr_t vector_ptr, int32_t index, int32_t count)
{
    if (vector_ptr == 0u || index < 0 || count <= 0 || index >= count || count > 10000
            || !memory_range_readable((void*)vector_ptr, 0x18u)) {
        return 0u;
    }

    uintptr_t values = *(uintptr_t*)vector_ptr;
    if (values == 0u || !memory_range_readable((void*)values, (SIZE_T)count * sizeof(uintptr_t))) {
        return 0u;
    }
    return ((uintptr_t*)values)[index];
}

static int32_t kbo_ai_roster_depth_id_slot_index(uintptr_t slot_block_ptr, uint32_t player_id)
{
    if (slot_block_ptr == 0u || player_id == 0u) {
        return -1;
    }

    uintptr_t table = slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_DEPTH_ID_TABLE_OFFSET;
    if (!memory_range_readable((void*)table, KBO_AI_ROSTER_SLOT_BLOCK_DEPTH_ID_COUNT * sizeof(uint32_t))) {
        return -1;
    }

    uint32_t* ids = (uint32_t*)table;
    for (int32_t i = 0; i < KBO_AI_ROSTER_SLOT_BLOCK_DEPTH_ID_COUNT; i++) {
        if (ids[i] == player_id) {
            return i;
        }
    }
    return -1;
}

static int32_t kbo_read_ai_roster_select_score(uintptr_t player_ptr, uint32_t offset)
{
    if (player_ptr == 0u
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player_ptr + offset);
}

static uint16_t kbo_read_player_u16_or_zero(uint8_t* player, uint32_t offset)
{
    if (player == NULL || !memory_range_readable(player + offset, sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(player + offset);
}

static uint16_t kbo_ai_roster_role_slot_value(uint8_t* player, int32_t slot_index)
{
    if (slot_index < 0 || slot_index > 5) {
        return 0u;
    }
    return kbo_read_player_u16_or_zero(player, 0x800u + (uint32_t)slot_index * sizeof(uint16_t));
}

static int kbo_ai_roster_role_check_scan_bounds(int32_t arg2, int32_t* out_first, int32_t* out_last)
{
    if (out_first != NULL) {
        *out_first = -1;
    }
    if (out_last != NULL) {
        *out_last = -1;
    }
    if (arg2 < 0) {
        return 0;
    }

    int32_t first = 5;
    int32_t last = 5 - arg2;
    if (last > 5) {
        return 0;
    }
    if (last < 0) {
        last = 0;
    }
    if (out_first != NULL) {
        *out_first = first;
    }
    if (out_last != NULL) {
        *out_last = last;
    }
    return 1;
}

static uint32_t kbo_ai_roster_role_nonzero_mask(uint8_t* player, int32_t first, int32_t last, int32_t* out_count)
{
    if (out_count != NULL) {
        *out_count = 0;
    }
    if (player == NULL || first < 0 || first > 5 || last < 0 || last > 5 || first < last) {
        return 0u;
    }

    uint32_t mask = 0u;
    int32_t count = 0;
    for (int32_t slot = first; slot >= last; slot--) {
        uint16_t value = kbo_ai_roster_role_slot_value(player, slot);
        if (value != 0u) {
            mask |= 1u << slot;
            count++;
        }
    }
    if (out_count != NULL) {
        *out_count = count;
    }
    return mask;
}

static int32_t kbo_ai_roster_role_first_nonzero_slot(uint8_t* player, int32_t first, int32_t last)
{
    if (player == NULL || first < 0 || first > 5 || last < 0 || last > 5 || first < last) {
        return -1;
    }
    for (int32_t slot = first; slot >= last; slot--) {
        if (kbo_ai_roster_role_slot_value(player, slot) != 0u) {
            return slot;
        }
    }
    return -1;
}

static void kbo_fill_ai_roster_foreign_candidate_summary(
    KboAiRosterForeignCandidateSummary* summary,
    int32_t index,
    uint8_t* player)
{
    if (summary == NULL || player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    memset(summary, 0, sizeof(*summary));
    summary->index = index;
    summary->player_ptr = (uintptr_t)player;
    summary->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    summary->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    summary->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    summary->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    summary->league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    summary->c78 = (uint32_t)player[0xc78u];
    summary->c79 = (uint32_t)player[0xc79u];
    summary->f25 = (uint32_t)player[0xf25u];
    summary->score_fe0 = kbo_read_ai_roster_select_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    summary->score_fe4 = kbo_read_ai_roster_select_score(summary->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    summary->value_ac = (uint32_t)player[0xacu];
    summary->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    summary->talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    summary->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        summary->default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }

    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr == 0u && summary->default_team_id != 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
        status_ptr = g_kbo_player_team_status_by_id_lookup_fn((uintptr_t)player, (int32_t)summary->default_team_id);
    }
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        summary->status24 = status[0x24u];
        summary->status25 = status[0x25u];
        summary->status26 = status[0x26u];
    }
}

static DWORD kbo_ai_roster_select_live_tls_index(void)
{
    LONG index = g_kbo_ai_roster_select_live_tls_index;
    if (index >= 0) {
        return (DWORD)index;
    }

    DWORD new_index = TlsAlloc();
    if (new_index == TLS_OUT_OF_INDEXES) {
        return TLS_OUT_OF_INDEXES;
    }

    LONG previous = InterlockedCompareExchange(
        &g_kbo_ai_roster_select_live_tls_index,
        (LONG)new_index,
        -1);
    if (previous >= 0) {
        TlsFree(new_index);
        return (DWORD)previous;
    }
    return new_index;
}

static KboAiRosterSelectLiveTrace* kbo_ai_roster_select_live_trace_get(void)
{
    DWORD index = kbo_ai_roster_select_live_tls_index();
    if (index == TLS_OUT_OF_INDEXES) {
        return NULL;
    }
    return (KboAiRosterSelectLiveTrace*)TlsGetValue(index);
}

static void kbo_ai_roster_select_live_trace_set(KboAiRosterSelectLiveTrace* trace)
{
    DWORD index = kbo_ai_roster_select_live_tls_index();
    if (index != TLS_OUT_OF_INDEXES) {
        TlsSetValue(index, trace);
    }
}

static void kbo_ai_roster_select_live_trace_init(
    KboAiRosterSelectLiveTrace* trace,
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint,
    uint32_t caller_rva)
{
    if (trace == NULL) {
        return;
    }

    memset(trace, 0, sizeof(*trace));
    trace->active = 1;
    trace->context_ptr = context_ptr;
    trace->slot_index = slot_index;
    trace->depth_hint = depth_hint;
    trace->caller_rva = caller_rva;
    for (int i = 0; i < KBO_AI_ROSTER_LOCAL_FOREIGN_SUMMARY_SLOTS; i++) {
        trace->foreign_summaries[i].index = -1;
    }
    for (int i = 0; i < KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT; i++) {
        trace->last_sorted_rank_by_candidate[i] = -1;
    }
}

static void kbo_ai_roster_select_live_trace_record_push(
    uintptr_t vector_ptr,
    uintptr_t value_ptr,
    int inserted,
    uint32_t caller_rva)
{
    KboAiRosterSelectLiveTrace* trace = kbo_ai_roster_select_live_trace_get();
    if (trace == NULL || !trace->active || !inserted || value_ptr == 0u) {
        return;
    }
    if (!kbo_player_pointer_plausible(value_ptr)
            || !memory_range_readable((void*)value_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)value_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int target_player = player_id == 5320u || player_id == 5381u || player_id == 5404u
        || player_id == 5368u || player_id == 5293u || player_id == 5417u;

    trace->push_count++;
    if (trace->inserted_count == 0) {
        trace->first_push_rva = caller_rva;
    }
    trace->last_push_rva = caller_rva;
    trace->inserted_count++;
    if (trace->first_vector_ptr == 0u) {
        trace->first_vector_ptr = vector_ptr;
    }
    if (trace->last_vector_ptr != 0u && trace->last_vector_ptr != vector_ptr) {
        trace->vector_switches++;
    }
    trace->last_vector_ptr = vector_ptr;

    int32_t local_index = trace->candidate_count;
    if (trace->candidate_count < KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT) {
        trace->candidate_values[trace->candidate_count] = value_ptr;
        trace->candidate_push_rvas[trace->candidate_count] = caller_rva;
    }
    trace->candidate_count++;

    if (target_player) {
        trace->target_count++;
    }
    if (kbo_player_is_foreign_for_kbo_rights(player)) {
        if (trace->foreign_count < KBO_AI_ROSTER_LOCAL_FOREIGN_SUMMARY_SLOTS) {
            kbo_fill_ai_roster_foreign_candidate_summary(
                &trace->foreign_summaries[trace->foreign_count],
                local_index,
                player);
        }
        trace->foreign_count++;
    }
}

static int32_t kbo_ai_roster_select_live_trace_candidate_index(
    const KboAiRosterSelectLiveTrace* trace,
    uintptr_t result_ptr,
    uint32_t* out_push_rva)
{
    if (out_push_rva != NULL) {
        *out_push_rva = 0u;
    }
    if (trace == NULL || result_ptr == 0u) {
        return -1;
    }

    int32_t count = trace->candidate_count;
    if (count > KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT) {
        count = KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT;
    }
    for (int32_t i = 0; i < count; i++) {
        if (trace->candidate_values[i] == result_ptr) {
            if (out_push_rva != NULL) {
                *out_push_rva = trace->candidate_push_rvas[i];
            }
            return i;
        }
    }
    return -1;
}

static int32_t kbo_ai_roster_select_live_trace_selected_index(
    const KboAiRosterSelectLiveTrace* trace,
    uintptr_t result_ptr)
{
    return kbo_ai_roster_select_live_trace_candidate_index(trace, result_ptr, NULL);
}

static int32_t kbo_ai_roster_select_live_trace_last_sorted_rank(
    const KboAiRosterSelectLiveTrace* trace,
    int32_t candidate_index)
{
    if (trace == NULL
            || candidate_index < 0
            || candidate_index >= KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT) {
        return -1;
    }
    return trace->last_sorted_rank_by_candidate[candidate_index];
}

static void kbo_ai_roster_sort_candidate_summary_init(KboAiRosterSortCandidateSummary* summary)
{
    if (summary == NULL) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->index = -1;
    summary->push_index = -1;
}

static void kbo_fill_ai_roster_sort_candidate_summary(
    KboAiRosterSortCandidateSummary* summary,
    int32_t index,
    uintptr_t player_ptr,
    const KboAiRosterSelectLiveTrace* live_trace)
{
    if (summary == NULL) {
        return;
    }
    kbo_ai_roster_sort_candidate_summary_init(summary);
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    summary->index = index;
    summary->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    summary->foreign = kbo_player_is_foreign_for_kbo_rights(player) ? 1u : 0u;
    summary->target = kbo_ai_roster_target_player_id(summary->player_id) ? 1u : 0u;
    summary->f25 = (uint32_t)player[0xf25u];
    summary->f62 = (uint32_t)player[0xf62u];
    summary->f65 = (uint32_t)player[0xf65u];
    summary->sort_f06 = (uint32_t)kbo_read_player_u16_or_zero(player, 0xf06u);
    summary->sort_c98 = (uint32_t)kbo_read_player_u16_or_zero(player, 0xc98u);
    summary->sort_bce = (uint32_t)player[0xbceu];
    int32_t role_count = 0;
    summary->role_mask = kbo_ai_roster_role_nonzero_mask(player, 5, 0, &role_count);
    summary->role_count = (uint32_t)role_count;
    summary->score_fe0 = kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    summary->score_fe4 = kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    summary->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    summary->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    summary->push_index = kbo_ai_roster_select_live_trace_candidate_index(
        live_trace,
        player_ptr,
        &summary->push_rva);

    uint32_t default_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr == 0u && default_team_id != 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
        status_ptr = g_kbo_player_team_status_by_id_lookup_fn(player_ptr, (int32_t)default_team_id);
    }
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        summary->status24 = status[0x24u];
        summary->status25 = status[0x25u];
        summary->status26 = status[0x26u];
    }
}

static void kbo_log_ai_roster_local_candidate_detail(
    LONG slot,
    uint32_t caller_rva,
    int32_t slot_index,
    const KboAiRosterSelectLiveTrace* live_trace,
    uintptr_t result_ptr)
{
    if (live_trace == NULL) {
        return;
    }

    int32_t count = live_trace->candidate_count;
    if (count <= 0) {
        return;
    }
    if (count > KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT) {
        count = KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT;
    }

    int32_t selected_index = kbo_ai_roster_select_live_trace_selected_index(live_trace, result_ptr);
    int32_t selected_last_sort_rank = kbo_ai_roster_select_live_trace_last_sorted_rank(
        live_trace,
        selected_index);
    int32_t logged_count = count > KBO_AI_ROSTER_LOCAL_DETAIL_TRACE_LIMIT
        ? KBO_AI_ROSTER_LOCAL_DETAIL_TRACE_LIMIT
        : count;

    append_logf(
        "ootp ai roster local detail header #%ld caller_rva=0x%x slot_index=%d local_candidates=%d logged=%d selected_local_index=%d selected_last_sort_rank=%d local_first_vec=%p local_last_vec=%p local_switches=%d local_pushes=%d local_inserted=%d local_foreign=%d local_targets=%d sort_count=%d score_sort_count=%d last_sort_caller=0x%x last_sort_phase=%s last_sort_cmp=0x%x last_sort_cmp_phase=%s last_sort_count=%d last_sort_foreign=%d last_sort_targets=%d",
        slot,
        caller_rva,
        slot_index,
        count,
        logged_count,
        selected_index,
        selected_last_sort_rank,
        (void*)live_trace->first_vector_ptr,
        (void*)live_trace->last_vector_ptr,
        live_trace->vector_switches,
        live_trace->push_count,
        live_trace->inserted_count,
        live_trace->foreign_count,
        live_trace->target_count,
        live_trace->sort_count,
        live_trace->score_sort_count,
        live_trace->last_sort_caller_rva,
        kbo_pointer_vector_sort_trace_phase(live_trace->last_sort_caller_rva),
        live_trace->last_sort_comparator_rva,
        kbo_pointer_vector_sort_comparator_phase(live_trace->last_sort_comparator_rva),
        live_trace->last_sort_count,
        live_trace->last_sort_foreign_count,
        live_trace->last_sort_target_count);

    for (int32_t i = 0; i < logged_count; i++) {
        uintptr_t candidate_ptr = live_trace->candidate_values[i];
        KboAiRosterSortCandidateSummary summary;
        kbo_fill_ai_roster_sort_candidate_summary(&summary, i, candidate_ptr, live_trace);
        int32_t last_sort_rank = kbo_ai_roster_select_live_trace_last_sorted_rank(live_trace, i);
        append_logf(
            "ootp ai roster local detail #%ld caller_rva=0x%x slot_index=%d idx=%d valid=%d selected=%d last_sort_rank=%d player_ptr=%p player=%u foreign=%u target=%u push_idx=%d push_rva=0x%x push_phase=%s push_order=%d fe0=%d fe4=%d f25=%u f62=%u f65=%u sort_f06=%u sort_bce=%u role=0x%x role_count=%u status24=%u status25=%u status26=%u overall=%d ratings=%d",
            slot,
            caller_rva,
            slot_index,
            i,
            summary.index >= 0 ? 1 : 0,
            i == selected_index ? 1 : 0,
            last_sort_rank,
            (void*)candidate_ptr,
            summary.player_id,
            summary.foreign,
            summary.target,
            summary.push_index,
            summary.push_rva,
            kbo_pointer_vector_push_trace_phase(summary.push_rva),
            kbo_ai_roster_push_phase_order(summary.push_rva),
            summary.score_fe0,
            summary.score_fe4,
            summary.f25,
            summary.f62,
            summary.f65,
            summary.sort_f06,
            summary.sort_bce,
            summary.role_mask,
            summary.role_count,
            summary.status24,
            summary.status25,
            summary.status26,
            summary.overall,
            summary.ratings);
    }

    if (selected_index >= logged_count && selected_index < count) {
        uintptr_t candidate_ptr = live_trace->candidate_values[selected_index];
        KboAiRosterSortCandidateSummary summary;
        kbo_fill_ai_roster_sort_candidate_summary(
            &summary,
            selected_index,
            candidate_ptr,
            live_trace);
        int32_t last_sort_rank = kbo_ai_roster_select_live_trace_last_sorted_rank(
            live_trace,
            selected_index);
        append_logf(
            "ootp ai roster local detail selected_extra #%ld caller_rva=0x%x slot_index=%d idx=%d valid=%d selected=1 last_sort_rank=%d player_ptr=%p player=%u foreign=%u target=%u push_idx=%d push_rva=0x%x push_phase=%s push_order=%d fe0=%d fe4=%d f25=%u f62=%u f65=%u sort_f06=%u sort_bce=%u role=0x%x role_count=%u status24=%u status25=%u status26=%u overall=%d ratings=%d",
            slot,
            caller_rva,
            slot_index,
            selected_index,
            summary.index >= 0 ? 1 : 0,
            last_sort_rank,
            (void*)candidate_ptr,
            summary.player_id,
            summary.foreign,
            summary.target,
            summary.push_index,
            summary.push_rva,
            kbo_pointer_vector_push_trace_phase(summary.push_rva),
            kbo_ai_roster_push_phase_order(summary.push_rva),
            summary.score_fe0,
            summary.score_fe4,
            summary.f25,
            summary.f62,
            summary.f65,
            summary.sort_f06,
            summary.sort_bce,
            summary.role_mask,
            summary.role_count,
            summary.status24,
            summary.status25,
            summary.status26,
            summary.overall,
            summary.ratings);
    }
}

static uint32_t kbo_ai_roster_source_visible_branch_mask(
    uint32_t status24,
    uint32_t status25,
    uint32_t status26,
    uint32_t f25)
{
    uint32_t mask = 0u;
    if (status24 == 0u || status24 == 4u || status25 == 4u || status26 == 3u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_DDA_STATUS4_OR_26;
    }
    if (status24 == 5u || status25 == 5u || status26 == 3u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_DDA_STATUS5_OR_26;
    }
    if (status24 == 2u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS24_2;
    }
    if (status25 == 2u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_2;
    }
    if (status24 == 11u || status25 == 11u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS11;
    }
    if (status24 == 4u || status25 == 4u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS4;
    }
    if (f25 == 100u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_DEPTH_SLOT;
    }
    if (status25 == 9u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_9;
    }
    if (status24 == 7u || status25 == 7u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS7;
    }
    if (status25 == 12u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_12;
    }
    if (status24 == 5u && f25 >= 75u) {
        mask |= KBO_AI_ROSTER_SOURCE_BRANCH_STATUS24_5;
    }
    return mask;
}

static int32_t kbo_ai_roster_source_branch_min_order(uint32_t mask)
{
    if ((mask & (KBO_AI_ROSTER_SOURCE_BRANCH_DDA_STATUS4_OR_26
            | KBO_AI_ROSTER_SOURCE_BRANCH_DDA_STATUS5_OR_26)) != 0u) {
        return 1;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS24_2) != 0u) {
        return 2;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_2) != 0u) {
        return 4;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS11) != 0u) {
        return 5;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS4) != 0u) {
        return 6;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_DEPTH_SLOT) != 0u) {
        return 8;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_9) != 0u) {
        return 9;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS7) != 0u) {
        return 10;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS25_12) != 0u) {
        return 11;
    }
    if ((mask & KBO_AI_ROSTER_SOURCE_BRANCH_STATUS24_5) != 0u) {
        return 12;
    }
    return 0;
}

static int64_t kbo_ai_roster_source_select_rescue_score(
    const KboAiRosterForeignCandidateSummary* summary,
    uint8_t* player)
{
    if (summary == NULL || player == NULL) {
        return INT64_MIN;
    }

    int64_t score = 0;
    score += (int64_t)summary->overall * 6;
    score += (int64_t)summary->ratings * 4;
    score += (int64_t)summary->talent * 2;
    score += (int64_t)summary->score_fe4 * 150;
    score += (int64_t)summary->score_fe0 * 80;
    score += (int64_t)summary->f25 * 12;
    score += (int64_t)kbo_read_player_i16(player, 0xf06u) * 8;
    score -= (int64_t)summary->status26 * 40;
    return score;
}

static int kbo_ai_player_quality_minor_foreign_callup_allows(
    int32_t team_arg,
    uint8_t* player,
    uint32_t* out_team_id,
    int* out_allowed);

static uintptr_t kbo_ai_roster_choose_source_select_rescue_candidate(
    uintptr_t source_vector_ptr,
    int32_t source_count,
    uintptr_t native_result_ptr,
    int32_t* out_source_index,
    uint32_t* out_active_team_id,
    int64_t* out_score,
    KboAiRosterForeignCandidateSummary* out_summary)
{
    if (out_source_index != NULL) {
        *out_source_index = -1;
    }
    if (out_active_team_id != NULL) {
        *out_active_team_id = 0u;
    }
    if (out_score != NULL) {
        *out_score = 0;
    }
    if (out_summary != NULL) {
        memset(out_summary, 0, sizeof(*out_summary));
        out_summary->index = -1;
    }

    int source_select_enabled = kbo_ai_roster_foreign_source_select_rescue_enabled();
    if (!source_select_enabled
            || source_vector_ptr == 0u
            || source_count <= 0) {
        static volatile LONG precheck_skip_log_count = 0;
        LONG precheck_slot = InterlockedIncrement(&precheck_skip_log_count);
        if (precheck_slot <= 200) {
            append_logf(
                "ootp ai roster foreign source-select rescue skip #%ld reason=precheck enabled=%d custom_policy=%d team_add_enabled=%d source_flag=%d source_disable=%d source_vector=%p source_count=%d native=%p",
                precheck_slot,
                source_select_enabled,
                kbo_custom_foreign_policy_enabled(),
                kbo_ai_roster_foreign_apply_rescue_team_add_enabled(),
                read_kbo_localappdata_flag_file("enable_ai_roster_foreign_source_select_rescue.txt"),
                read_kbo_localappdata_flag_file("disable_ai_roster_foreign_source_select_rescue.txt"),
                (void*)source_vector_ptr,
                source_count,
                (void*)native_result_ptr);
        }
        return 0u;
    }

    if (kbo_player_pointer_plausible(native_result_ptr)
            && memory_range_readable((void*)native_result_ptr, OOTP27_PLAYER_SCAN_BYTES)
            && kbo_player_is_foreign_for_kbo_rights((uint8_t*)native_result_ptr)) {
        static volatile LONG native_foreign_skip_log_count = 0;
        LONG native_foreign_slot = InterlockedIncrement(&native_foreign_skip_log_count);
        if (native_foreign_slot <= 200) {
            uint8_t* native_player = (uint8_t*)native_result_ptr;
            append_logf(
                "ootp ai roster foreign source-select rescue skip #%ld reason=native_already_foreign source_count=%d native=%u current=%u active=%u league=%u status26=%u f25=%u f62=%u f65=%u",
                native_foreign_slot,
                source_count,
                *(uint32_t*)(native_player + OOTP27_PLAYER_ID_OFFSET),
                *(uint32_t*)(native_player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
                *(uint32_t*)(native_player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
                *(uint32_t*)(native_player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                (uint32_t)native_player[0x26u],
                (uint32_t)native_player[0xf25u],
                (uint32_t)native_player[0xf62u],
                (uint32_t)native_player[0xf65u]);
        }
        return 0u;
    }

    int32_t scanned_count = source_count > KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        ? KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        : source_count;
    if (scanned_count <= 0) {
        return 0u;
    }

    uintptr_t best_ptr = 0u;
    int32_t best_index = -1;
    uint32_t best_active_team_id = 0u;
    int64_t best_score = INT64_MIN;
    KboAiRosterForeignCandidateSummary best_summary = {0};
    best_summary.index = -1;
    KboAiRosterForeignCandidateSummary best_seen_summary = {0};
    best_seen_summary.index = -1;
    int64_t best_seen_score = INT64_MIN;
    uint32_t best_seen_active_team_id = 0u;
    int best_seen_callup_allowed = 0;
    const char* best_seen_reason = "none";
    int foreign_seen_count = 0;
    int gate_ready_count = 0;
    int callup_ready_count = 0;
    int already_active_count = 0;
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();

    for (int32_t i = 0; i < scanned_count; i++) {
        uintptr_t candidate_ptr = kbo_pointer_vector_value_at(source_vector_ptr, i, source_count);
        if (!kbo_player_pointer_plausible(candidate_ptr)
                || !memory_range_readable((void*)candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        uint8_t* candidate = (uint8_t*)candidate_ptr;
        if (!kbo_player_is_foreign_for_kbo_rights(candidate)) {
            continue;
        }

        KboAiRosterForeignCandidateSummary summary;
        kbo_fill_ai_roster_foreign_candidate_summary(&summary, i, candidate);
        foreign_seen_count++;
        int64_t seen_score = kbo_ai_roster_source_select_rescue_score(&summary, candidate);
        uint32_t seen_active_team_id = 0u;
        int seen_callup_allowed = 0;
        const char* seen_reason = "candidate_gate";
        if (summary.player_id == 0u
                || summary.f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN
                || candidate[0xf65u] != 0u
                || (summary.status26 != 0u && summary.status26 != 1u)) {
            if (seen_score > best_seen_score) {
                best_seen_score = seen_score;
                best_seen_summary = summary;
                best_seen_active_team_id = seen_active_team_id;
                best_seen_callup_allowed = seen_callup_allowed;
                best_seen_reason = seen_reason;
            }
            continue;
        }
        gate_ready_count++;

        uint32_t active_team_id = 0u;
        int callup_allowed = 0;
        if (!kbo_ai_player_quality_minor_foreign_callup_allows(
                (int32_t)summary.league_id,
                candidate,
                &active_team_id,
                &callup_allowed)
                || !callup_allowed
                || active_team_id == 0u) {
            seen_reason = "callup_not_allowed";
            seen_active_team_id = active_team_id;
            seen_callup_allowed = callup_allowed;
            if (seen_score > best_seen_score) {
                best_seen_score = seen_score;
                best_seen_summary = summary;
                best_seen_active_team_id = seen_active_team_id;
                best_seen_callup_allowed = seen_callup_allowed;
                best_seen_reason = seen_reason;
            }
            continue;
        }
        callup_ready_count++;

        if (kbo_league_id != 0u
                && summary.current_team_id == active_team_id
                && summary.active_team_id == active_team_id
                && summary.league_id == kbo_league_id) {
            seen_reason = "already_active";
            seen_active_team_id = active_team_id;
            seen_callup_allowed = callup_allowed;
            already_active_count++;
            if (seen_score > best_seen_score) {
                best_seen_score = seen_score;
                best_seen_summary = summary;
                best_seen_active_team_id = seen_active_team_id;
                best_seen_callup_allowed = seen_callup_allowed;
                best_seen_reason = seen_reason;
            }
            continue;
        }

        seen_reason = "candidate_selected";
        seen_active_team_id = active_team_id;
        seen_callup_allowed = callup_allowed;
        if (seen_score > best_seen_score) {
            best_seen_score = seen_score;
            best_seen_summary = summary;
            best_seen_active_team_id = seen_active_team_id;
            best_seen_callup_allowed = seen_callup_allowed;
            best_seen_reason = seen_reason;
        }
        int64_t score = seen_score;
        if (score > best_score) {
            best_score = score;
            best_ptr = candidate_ptr;
            best_index = i;
            best_active_team_id = active_team_id;
            best_summary = summary;
        }
    }

    if (best_ptr == 0u) {
        static volatile LONG no_candidate_skip_log_count = 0;
        LONG no_candidate_slot = InterlockedIncrement(&no_candidate_skip_log_count);
        if (no_candidate_slot <= 300 || foreign_seen_count > 0) {
            append_logf(
                "ootp ai roster foreign source-select rescue skip #%ld reason=no_candidate source_count=%d scanned=%d foreign_seen=%d gate_ready=%d callup_ready=%d already_active=%d best_seen_reason=%s best_seen=%u best_seen_score=%lld best_seen_active_team=%u best_seen_callup=%d current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f25=%u f65=%u fe0=%d fe4=%d overall=%d talent=%d ratings=%d kbo_league=%u",
                no_candidate_slot,
                source_count,
                scanned_count,
                foreign_seen_count,
                gate_ready_count,
                callup_ready_count,
                already_active_count,
                best_seen_reason,
                best_seen_summary.player_id,
                (long long)best_seen_score,
                best_seen_active_team_id,
                best_seen_callup_allowed,
                best_seen_summary.current_team_id,
                best_seen_summary.active_team_id,
                best_seen_summary.league_id,
                best_seen_summary.default_team_id,
                best_seen_summary.status24,
                best_seen_summary.status25,
                best_seen_summary.status26,
                best_seen_summary.f25,
                best_seen_summary.player_ptr != 0u
                    ? (uint32_t)((uint8_t*)best_seen_summary.player_ptr)[0xf65u]
                    : 0u,
                best_seen_summary.score_fe0,
                best_seen_summary.score_fe4,
                best_seen_summary.overall,
                best_seen_summary.talent,
                best_seen_summary.ratings,
                kbo_league_id);
        }
        return 0u;
    }

    if (out_source_index != NULL) {
        *out_source_index = best_index;
    }
    if (out_active_team_id != NULL) {
        *out_active_team_id = best_active_team_id;
    }
    if (out_score != NULL) {
        *out_score = best_score;
    }
    if (out_summary != NULL) {
        *out_summary = best_summary;
    }
    return best_ptr;
}

static void kbo_log_ai_roster_source_local_gate_trace(
    LONG select_slot,
    uint32_t caller_rva,
    int32_t slot_index,
    int32_t depth_hint,
    uintptr_t source_vector_ptr,
    int32_t source_count,
    const KboAiRosterSelectLiveTrace* live_trace)
{
    if (source_count <= 0 || live_trace == NULL) {
        return;
    }

    int32_t scanned_count = source_count > KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        ? KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        : source_count;
    int32_t logged = 0;
    int32_t first_push_order = kbo_ai_roster_push_phase_order(live_trace->first_push_rva);
    uintptr_t slot_block_ptr = source_vector_ptr > KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET
        ? source_vector_ptr - KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET
        : 0u;
    uint32_t slot_block_flag = 0u;
    uint32_t marked_player_id = 0u;
    uint32_t marked_player_id2 = 0u;
    if (slot_block_ptr != 0u) {
        if (memory_range_readable((void*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_FLAG_OFFSET), sizeof(uint8_t))) {
            slot_block_flag = *(uint8_t*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_FLAG_OFFSET);
        }
        if (memory_range_readable((void*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
            marked_player_id = *(uint32_t*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID_OFFSET);
        }
        if (memory_range_readable((void*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID2_OFFSET), sizeof(uint32_t))) {
            marked_player_id2 = *(uint32_t*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID2_OFFSET);
        }
    }
    for (int32_t i = 0; i < scanned_count && logged < KBO_AI_ROSTER_SOURCE_GATE_TRACE_LIMIT; i++) {
        uintptr_t candidate_ptr = kbo_pointer_vector_value_at(source_vector_ptr, i, source_count);
        if (!kbo_player_pointer_plausible(candidate_ptr)
                || !memory_range_readable((void*)candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            continue;
        }

        uint8_t* player = (uint8_t*)candidate_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        int target_player = player_id == 5320u || player_id == 5381u || player_id == 5404u
            || player_id == 5368u || player_id == 5293u || player_id == 5417u;
        int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
        if (!target_player && !foreign_player) {
            continue;
        }

        uint32_t local_push_rva = 0u;
        int32_t local_index = kbo_ai_roster_select_live_trace_candidate_index(
            live_trace,
            candidate_ptr,
            &local_push_rva);
        if (!target_player && local_index >= 0 && logged >= 3) {
            continue;
        }

        KboAiRosterForeignCandidateSummary summary;
        kbo_fill_ai_roster_foreign_candidate_summary(&summary, i, player);
        uint32_t branch_mask = kbo_ai_roster_source_visible_branch_mask(
            summary.status24,
            summary.status25,
            summary.status26,
            summary.f25);
        int32_t min_order = kbo_ai_roster_source_branch_min_order(branch_mask);
        int32_t local_push_order = kbo_ai_roster_push_phase_order(local_push_rva);
        int32_t fixed_slot_index = kbo_ai_roster_depth_id_slot_index(slot_block_ptr, summary.player_id);
        int marked_candidate = summary.player_id != 0u
            && (summary.player_id == marked_player_id || summary.player_id == marked_player_id2);
        int prior_local_possible = local_index < 0
            && first_push_order > 0
            && min_order > first_push_order;

        append_logf(
            "ootp ai roster source-local gate trace #%ld probe=%d caller_rva=0x%x slot_index=%d depth_hint=%d slot_block=%p slot_flag=%u marked1=%u marked2=%u fixed_slot_idx=%d marked_candidate=%d first_push_rva=0x%x first_push_phase=%s first_push_order=%d source_idx=%d local_idx=%d local_push_rva=0x%x local_push_phase=%s local_push_order=%d visible_mask=0x%x min_order=%d prior_local_possible=%d player=%u target=%d foreign=%d nation=%u current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u c78=%u c79=%u f25=%u f62=%u f65=%u f6a=%u f06=%d fec=%u pos_group=%u pos_role=%u role800=%u role802=%u role804=%u role806=%u role808=%u role80a=%u value_ac=%u fe0=%d fe4=%d overall=%d talent=%d ratings=%d",
            select_slot,
            logged + 1,
            caller_rva,
            slot_index,
            depth_hint,
            (void*)slot_block_ptr,
            slot_block_flag,
            marked_player_id,
            marked_player_id2,
            fixed_slot_index,
            marked_candidate,
            live_trace->first_push_rva,
            kbo_pointer_vector_push_trace_phase(live_trace->first_push_rva),
            first_push_order,
            i,
            local_index,
            local_push_rva,
            kbo_pointer_vector_push_trace_phase(local_push_rva),
            local_push_order,
            branch_mask,
            min_order,
            prior_local_possible,
            summary.player_id,
            target_player,
            foreign_player,
            summary.nation_id,
            summary.current_team_id,
            summary.active_team_id,
            summary.league_id,
            summary.default_team_id,
            summary.status24,
            summary.status25,
            summary.status26,
            summary.c78,
            summary.c79,
            summary.f25,
            (uint32_t)player[0xf62u],
            (uint32_t)player[0xf65u],
            (uint32_t)player[0xf6au],
            kbo_read_player_i16(player, 0xf06u),
            (uint32_t)player[0xfecu],
            (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
            (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
            (uint32_t)kbo_ai_roster_role_slot_value(player, 0),
            (uint32_t)kbo_ai_roster_role_slot_value(player, 1),
            (uint32_t)kbo_ai_roster_role_slot_value(player, 2),
            (uint32_t)kbo_ai_roster_role_slot_value(player, 3),
            (uint32_t)kbo_ai_roster_role_slot_value(player, 4),
            (uint32_t)kbo_ai_roster_role_slot_value(player, 5),
            summary.value_ac,
            summary.score_fe0,
            summary.score_fe4,
            summary.overall,
            summary.talent,
            summary.ratings);
        logged++;
    }
}

static int kbo_ai_player_quality_trace_caller_is_roster_relevant(uint32_t caller_rva)
{
    return caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A39879_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A3993E_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A39A6C_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A39C56_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A3AACE_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A3AAEE_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A3B09E_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A3B0E5_RVA;
}

static int kbo_ai_player_quality_bias_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && !read_kbo_localappdata_flag_file("disable_ai_player_quality_bias.txt");
}

static int kbo_ai_player_quality_status25_bias_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && !read_kbo_localappdata_flag_file("disable_ai_player_quality_status25_bias.txt");
}

static int kbo_ai_status25_11_rescue_enabled(void)
{
    return kbo_custom_foreign_policy_enabled()
        && !read_kbo_localappdata_flag_file("disable_ai_status25_11_rescue.txt");
}

static int kbo_ai_player_quality_caller_is_candidate_bias_target(uint32_t caller_rva)
{
    return caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A39879_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A3993E_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A39A6C_RVA
        || caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A39C56_RVA;
}

static int kbo_ai_player_quality_caller_is_status25_callup_bias_target(uint32_t caller_rva)
{
    return caller_rva == KBO_AI_PLAYER_QUALITY_CALLER_A3B0E5_RVA;
}

static uint32_t kbo_ai_player_quality_parent_team_id(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            return parent_team_id;
        }
    }
    return team_id;
}

static uint8_t* kbo_ai_player_quality_resolve_active_team(
    uint8_t* player,
    uint32_t* out_team_id)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return NULL;
    }

    uint32_t team_ids[3] = {
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
    };
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    for (uint32_t i = 0u; i < 3u; i++) {
        uint32_t org_team_id = kbo_ai_player_quality_parent_team_id(team_ids[i]);
        if (org_team_id == 0u) {
            continue;
        }

        uint8_t* team = find_kbo_team_by_numeric_id_any_league(org_team_id, 1);
        if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }

        uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        if (kbo_league_id != 0u && team_league_id != kbo_league_id) {
            continue;
        }

        if (out_team_id != NULL) {
            *out_team_id = org_team_id;
        }
        return team;
    }
    return NULL;
}

static int kbo_ai_player_quality_minor_foreign_callup_allows(
    int32_t team_arg,
    uint8_t* player,
    uint32_t* out_team_id,
    int* out_allowed)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_allowed != NULL) {
        *out_allowed = 0;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int minor_league_match = kbo_league_id != 0u
        && (current_league_id == kbo_league_id + 1u || (uint32_t)team_arg == kbo_league_id + 1u);
    if (!minor_league_match) {
        return 0;
    }

    uint32_t active_team_id = 0u;
    uint8_t* active_team = kbo_ai_player_quality_resolve_active_team(player, &active_team_id);
    if (active_team == NULL) {
        return 0;
    }

    uint8_t allowed = kbo_custom_foreign_policy_callup_allows(
        (uintptr_t)active_team,
        (uintptr_t)player,
        0,
        (int32_t)KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
        3);
    if (out_team_id != NULL) {
        *out_team_id = active_team_id;
    }
    if (out_allowed != NULL) {
        *out_allowed = allowed ? 1 : 0;
    }
    return allowed ? 1 : 0;
}

static int32_t kbo_apply_ai_player_quality_candidate_bias(
    int32_t result,
    uint32_t caller_rva,
    int32_t team_arg,
    uint8_t* player,
    uint32_t* out_team_id,
    int* out_adjusted,
    int* out_allowed)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_adjusted != NULL) {
        *out_adjusted = 0;
    }
    if (out_allowed != NULL) {
        *out_allowed = 0;
    }

    if (result < 0
            || result >= 3
            || player == NULL
            || !kbo_ai_player_quality_bias_enabled()
            || !kbo_ai_player_quality_caller_is_candidate_bias_target(caller_rva)) {
        return result;
    }

    uint32_t active_team_id = 0u;
    int allowed = 0;
    if (!kbo_ai_player_quality_minor_foreign_callup_allows(team_arg, player, &active_team_id, &allowed)) {
        if (out_team_id != NULL) {
            *out_team_id = active_team_id;
        }
        if (out_allowed != NULL) {
            *out_allowed = allowed;
        }
        return result;
    }

    if (out_team_id != NULL) {
        *out_team_id = active_team_id;
    }
    if (out_adjusted != NULL) {
        *out_adjusted = 1;
    }
    if (out_allowed != NULL) {
        *out_allowed = allowed;
    }
    return 3;
}

static int32_t kbo_apply_ai_player_quality_status25_bias(
    int32_t result,
    uint32_t caller_rva,
    int32_t team_arg,
    uint8_t* player,
    uint32_t* out_team_id,
    int* out_adjusted,
    int* out_allowed)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_adjusted != NULL) {
        *out_adjusted = 0;
    }
    if (out_allowed != NULL) {
        *out_allowed = 0;
    }

    if (result < 0
            || result >= 4
            || player == NULL
            || !kbo_ai_player_quality_status25_bias_enabled()
            || !kbo_ai_player_quality_caller_is_status25_callup_bias_target(caller_rva)
            || player[0xc78u] != 1u) {
        return result;
    }

    uint32_t active_team_id = 0u;
    int allowed = 0;
    if (!kbo_ai_player_quality_minor_foreign_callup_allows(team_arg, player, &active_team_id, &allowed)) {
        if (out_team_id != NULL) {
            *out_team_id = active_team_id;
        }
        if (out_allowed != NULL) {
            *out_allowed = allowed;
        }
        return result;
    }

    if (out_team_id != NULL) {
        *out_team_id = active_team_id;
    }
    if (out_adjusted != NULL) {
        *out_adjusted = 1;
    }
    if (out_allowed != NULL) {
        *out_allowed = allowed;
    }
    return 4;
}

static const char* kbo_ai_player_quality_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_AI_PLAYER_QUALITY_CALLER_A39879_RVA:
        return "candidate_insert_primary";
    case KBO_AI_PLAYER_QUALITY_CALLER_A3993E_RVA:
        return "candidate_insert_secondary";
    case KBO_AI_PLAYER_QUALITY_CALLER_A39A6C_RVA:
        return "candidate_insert_depth";
    case KBO_AI_PLAYER_QUALITY_CALLER_A39C56_RVA:
        return "candidate_replace";
    case KBO_AI_PLAYER_QUALITY_CALLER_A3AACE_RVA:
        return "status4_gate_probe_a";
    case KBO_AI_PLAYER_QUALITY_CALLER_A3AAEE_RVA:
        return "status5_gate_probe_b";
    case KBO_AI_PLAYER_QUALITY_CALLER_A3B09E_RVA:
        return "status25_4_gate";
    case KBO_AI_PLAYER_QUALITY_CALLER_A3B0E5_RVA:
        return "status25_2_gate";
    default:
        return "other";
    }
}

static const char* kbo_ai_roster_eligibility_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA0E5_RVA:
        return "six_id_active_candidate";
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA1BA_RVA:
        return "six_id_status24_ge_5";
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA2D6_RVA:
        return "vector_status8_7_6_gate";
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA471_RVA:
        return "vector_status24_0_or_4_gate";
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA521_RVA:
        return "vector_status24_5_gate";
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB0EB_RVA:
        return "final_status24_2_gate";
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB18A_RVA:
        return "final_status25_2_gate";
    case KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB229_RVA:
        return "final_status11_gate";
    default:
        return "other";
    }
}

static int kbo_ai_roster_eligibility_trace_caller_is_relevant(uint32_t caller_rva)
{
    return caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA0E5_RVA
        || caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA1BA_RVA
        || caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA2D6_RVA
        || caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA471_RVA
        || caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDA521_RVA
        || caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB0EB_RVA
        || caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB18A_RVA
        || caller_rva == KBO_AI_ROSTER_ELIGIBILITY_CALLER_DDB229_RVA;
}

static const char* kbo_ai_roster_availability_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case 0x00DDA40Cu:
        return "source_initial_availability_gate";
    case 0x00DDA4B4u:
        return "source_status5_availability_gate";
    case 0x00DDB491u:
        return "depth_slot_availability_gate";
    case 0x00DDB57Bu:
        return "status25_9_availability_gate";
    default:
        if (caller_rva >= 0x00DD8600u && caller_rva <= 0x00DDD100u) {
            return "roster_availability_gate";
        }
        return "other";
    }
}

static int kbo_ai_roster_availability_trace_caller_is_relevant(uint32_t caller_rva)
{
    return caller_rva >= 0x00DD8600u && caller_rva <= 0x00DDD100u;
}

static void kbo_format_probe_bytes(uintptr_t ptr, size_t byte_count, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (ptr == 0u || byte_count == 0u || !memory_range_readable((void*)ptr, byte_count)) {
        snprintf(out, out_size, "unreadable");
        return;
    }

    const uint8_t* bytes = (const uint8_t*)ptr;
    size_t used = 0u;
    for (size_t i = 0u; i < byte_count && used + 3u < out_size; i++) {
        int written = snprintf(out + used, out_size - used, "%02X", (unsigned int)bytes[i]);
        if (written <= 0) {
            break;
        }
        used += (size_t)written;
        if (i + 1u < byte_count && used + 1u < out_size) {
            out[used++] = ' ';
            out[used] = '\0';
        }
    }
}

static int32_t kbo_find_u32_in_probe_bytes(uintptr_t ptr, size_t byte_count, uint32_t needle)
{
    if (ptr == 0u || byte_count < sizeof(uint32_t) || !memory_range_readable((void*)ptr, byte_count)) {
        return -1;
    }

    const uint8_t* bytes = (const uint8_t*)ptr;
    for (size_t i = 0u; i + sizeof(uint32_t) <= byte_count; i += sizeof(uint32_t)) {
        uint32_t value = *(const uint32_t*)(bytes + i);
        if (value == needle) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int32_t kbo_apply_ai_team_player_fit_bias(
    int32_t result,
    uint32_t caller_rva,
    uint8_t* player,
    uint32_t team_league_id,
    uint32_t current_league_id,
    uint32_t* out_team_id,
    int* out_adjusted)
{
    if (out_team_id != NULL) {
        *out_team_id = 0u;
    }
    if (out_adjusted != NULL) {
        *out_adjusted = 0;
    }
    if (!kbo_ai_team_player_fit_bias_enabled()) {
        return result;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int minor_league_match = (kbo_league_id != 0u && (team_league_id == kbo_league_id + 1u || current_league_id == kbo_league_id + 1u))
        || (kbo_league_id == 0u && team_league_id != 0u && current_league_id == team_league_id);
    if (!minor_league_match) {
        return result;
    }

    if (result == 1 && kbo_ai_team_player_fit_caller_is_minor_foreign_candidate_prune_target(caller_rva)) {
        uint32_t active_team_id = 0u;
        uint8_t* active_team = kbo_ai_player_quality_resolve_active_team(player, &active_team_id);
        if (active_team == NULL) {
            return result;
        }

        uint8_t allowed = kbo_custom_foreign_policy_callup_allows(
            (uintptr_t)active_team,
            (uintptr_t)player,
            0,
            (int32_t)KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT,
            3);
        if (out_team_id != NULL) {
            *out_team_id = active_team_id;
        }
        if (!allowed) {
            return result;
        }

        if (out_adjusted != NULL) {
            *out_adjusted = 1;
        }
        return 0;
    }

    if (result == 3 && kbo_ai_team_player_fit_caller_is_minor_foreign_upgrade_target(caller_rva)) {
        if (out_adjusted != NULL) {
            *out_adjusted = 1;
        }
        return 1;
    }

    if (result != 0 || !kbo_ai_team_player_fit_caller_is_minor_foreign_final_target(caller_rva)) {
        return result;
    }

    if (out_adjusted != NULL) {
        *out_adjusted = 1;
    }
    return 1;
}

static const char* kbo_ai_team_player_fit_trace_phase(uint32_t caller_rva)
{
    switch (caller_rva) {
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_A3985D_RVA:
        return "candidate_fit_primary_gate";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_A3991E_RVA:
        return "candidate_fit_secondary_gate";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_A39D24_RVA:
        return "candidate_prune_or_penalty";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_A3B2F2_RVA:
        return "final_status_rescue_gate";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DD8C09_RVA:
        return "roster_fit_dd8c09";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DD92CA_RVA:
        return "roster_fit_dd92ca";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DD96A2_RVA:
        return "roster_fit_dd96a2";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DD98B5_RVA:
        return "roster_fit_dd98b5";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA2F0_RVA:
        return "roster_fit_dda2f0";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA48B_RVA:
        return "roster_fit_dda48b";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA53B_RVA:
        return "roster_fit_dda53b";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DDA619_RVA:
        return "roster_fit_dda619";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DDB243_RVA:
        return "roster_fit_ddb243";
    case KBO_AI_TEAM_PLAYER_FIT_CALLER_DDB309_RVA:
        return "roster_fit_ddb309";
    default:
        return "other";
    }
}

void kbo_set_team_add_player_guard_trampoline(void* trampoline)
{
    g_kbo_team_add_player_guard_trampoline = (OotpKboTeamAddPlayerExFn)(uintptr_t)trampoline;
}

void kbo_clear_team_add_player_guard_trampoline(void)
{
    g_kbo_team_add_player_guard_trampoline = NULL;
}

void kbo_set_roster_move_active_trace_trampoline(void* trampoline)
{
    g_kbo_roster_move_active_trace_trampoline = (OotpKboTeamAddPlayerExFn)(uintptr_t)trampoline;
}

void kbo_set_roster_move_secondary_trace_trampoline(void* trampoline)
{
    g_kbo_roster_move_secondary_trace_trampoline = (OotpKboTeamAddPlayerExFn)(uintptr_t)trampoline;
}

void kbo_set_roster_move_assignment_trace_trampoline(void* trampoline)
{
    g_kbo_roster_move_assignment_trace_trampoline = (OotpKboTeamAddPlayerExFn)(uintptr_t)trampoline;
}

void kbo_set_player_eval_double_trace_trampoline(void* trampoline)
{
    g_kbo_player_eval_double_trace_trampoline = (OotpKboPlayerEvalDoubleFn)(uintptr_t)trampoline;
}

void kbo_set_player_eval_cache_trace_trampoline(void* trampoline)
{
    g_kbo_player_eval_cache_trace_trampoline = (OotpKboPlayerEvalCacheFn)(uintptr_t)trampoline;
}

void kbo_set_ai_player_quality_trace_trampoline(void* trampoline)
{
    g_kbo_ai_player_quality_trace_trampoline = (OotpKboAiPlayerQualityFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_role_check_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_role_check_trace_trampoline = (OotpKboAiRosterRoleCheckFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_post_sort_gate_score_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_post_sort_gate_score_trace_trampoline =
        (OotpKboAiRosterPostSortGateScoreFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_eligibility_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_eligibility_trace_trampoline = (OotpKboAiRosterEligibilityFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_availability_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_availability_trace_trampoline = (OotpKboAiRosterAvailabilityFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_f65_update_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_f65_update_trace_trampoline = (OotpKboAiRosterF65UpdateFn)(uintptr_t)trampoline;
}

void kbo_set_ai_team_player_fit_trace_trampoline(void* trampoline)
{
    g_kbo_ai_team_player_fit_trace_trampoline = (OotpKboAiTeamPlayerFitFn)(uintptr_t)trampoline;
}

void kbo_set_player_team_status_lookup_trace_trampoline(void* trampoline)
{
    g_kbo_player_team_status_lookup_trace_trampoline = (OotpKboPlayerTeamStatusLookupFn)(uintptr_t)trampoline;
}

void kbo_set_player_team_status_by_id_lookup_fn(void* fn)
{
    g_kbo_player_team_status_by_id_lookup_fn = (OotpKboPlayerTeamStatusLookupFn)(uintptr_t)fn;
}

void kbo_set_pointer_vector_push_trace_trampoline(void* trampoline)
{
    g_kbo_pointer_vector_push_trace_trampoline = (OotpKboPointerVectorPushFn)(uintptr_t)trampoline;
}

void kbo_set_pointer_vector_sort_trace_trampoline(void* trampoline)
{
    g_kbo_pointer_vector_sort_trace_trampoline = (OotpKboPointerVectorSortFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_priority_compare_trampoline(void* trampoline)
{
    g_kbo_ai_roster_priority_compare_trampoline =
        (OotpKboAiRosterPriorityCompareFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_type_compare_trampoline(void* trampoline)
{
    g_kbo_ai_roster_type_compare_trampoline =
        (OotpKboAiRosterTypeCompareFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_score_compare_trampoline(void* trampoline)
{
    g_kbo_ai_roster_score_compare_trampoline =
        (OotpKboAiRosterScoreCompareFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_select_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_select_trace_trampoline = (OotpKboAiRosterSelectFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_primary_apply_flow_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_primary_apply_flow_trace_trampoline =
        (OotpKboAiRosterContextFlowFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_secondary_main_flow_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_secondary_main_flow_trace_trampoline =
        (OotpKboAiRosterContextFlowFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_secondary_alt_flow_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_secondary_alt_flow_trace_trampoline =
        (OotpKboAiRosterContextFlowFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_mark_selected_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_mark_selected_trace_trampoline =
        (OotpKboAiRosterMarkSelectedFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_selection_reconcile_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_selection_reconcile_trace_trampoline =
        (OotpKboAiRosterSelectionReconcileFn)(uintptr_t)trampoline;
}

void kbo_set_ai_roster_apply_selection_trace_trampoline(void* trampoline)
{
    g_kbo_ai_roster_apply_selection_trace_trampoline =
        (OotpKboAiRosterApplySelectionFn)(uintptr_t)trampoline;
}

void kbo_set_player_clear_team_trace_trampoline(void* trampoline)
{
    g_kbo_player_clear_team_trace_trampoline =
        (OotpKboPlayerClearTeamTraceFn)(uintptr_t)trampoline;
}

void kbo_set_player_set_team_trace_trampoline(void* trampoline)
{
    g_kbo_player_set_team_trace_trampoline =
        (OotpKboPlayerSetTeamTraceFn)(uintptr_t)trampoline;
}

uint8_t kbo_team_add_player_guard_call_original(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8)
{
    OotpKboTeamAddPlayerExFn original = g_kbo_team_add_player_guard_trampoline;
    if (original == NULL) {
        return 0;
    }
    return original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
}

static void kbo_team_add_player_record_fa_compensation_success(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    KBO_PROFILE_BEGIN(profile_fa_comp_probe_inner);
    if (!kbo_fix_enabled()
            || read_kbo_localappdata_flag_file("disable_kbo_fa_compensation.txt")
            || team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.precheck_reject");
        return;
    }

    if (before_current_team_id != 0u && before_active_team_id != 0u) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.not_teamless_before");
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_id == 0u || league_id == 0u || kbo_team_id_is_military_service_team(team_id)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.bad_team");
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t filing_original_team_id = 0u;
    uint32_t filing_league_id = 0u;
    uint32_t filing_season = 0u;
    if (!kbo_fa_filing_find_latest_player(player_id, &filing_original_team_id, &filing_league_id, &filing_season)) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.no_filing");
        return;
    }

    if (filing_league_id != 0u) {
        league_id = filing_league_id;
    }

    uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t signing_team_id = after_active_team_id != 0u ? after_active_team_id : team_id;
    if (signing_team_id == filing_original_team_id) {
        KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.same_team");
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot <= 160) {
        append_logf(
            "KBO team-add FA compensation probe player=%u team=%u league=%u before_current=%u before_active=%u before_original=%u filing_original=%u filing_league=%u filing_season=%u after_current=%u after_active=%u",
            player_id,
            signing_team_id,
            league_id,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id,
            filing_original_team_id,
            filing_league_id,
            filing_season,
            after_current_team_id,
            after_active_team_id);
    }

    kbo_record_fa_compensation_signing(player_ptr, signing_team_id, league_id, "team_add_player_success");
    KBO_PROFILE_END(profile_fa_comp_probe_inner, "team_add_guard.fa_comp_inner.record_attempt");
}

static int kbo_team_add_foreign_policy_should_block(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uint32_t team_id,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id)
{
    if (!kbo_fix_enabled()
            || !kbo_custom_foreign_policy_enabled()
            || team_id == 0u
            || team_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)) {
        return 0;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)
            || !kbo_player_is_foreign_for_kbo_rights(player)) {
        return 0;
    }

    if (kbo_player_current_assignment_matches_team_or_affiliate(player, team_id)) {
        return 0;
    }

    uint32_t effective_before = 0u;
    uint32_t effective_after = 0u;
    uint32_t effective_limit = KBO_CUSTOM_FOREIGN_BASE_EFFECTIVE_LIMIT;
    uint8_t slot_type = 0u;
    uint32_t injured_player_id = 0u;
    int allowed = kbo_custom_foreign_policy_team_allows_candidate(
        team_id,
        player,
        &effective_before,
        &effective_after,
        &effective_limit,
        &slot_type,
        &injured_player_id);
    if (allowed) {
        return 0;
    }

    static volatile LONG block_log_count = 0;
    LONG slot = InterlockedIncrement(&block_log_count);
    if (slot <= 300) {
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        append_logf(
            "custom foreign policy team-add blocked player=%u team=%u before_current=%u before_active=%u current=%u active=%u effective_before=%u effective_after=%u limit=%u injury_slot=%s injured=%u",
            player_id,
            team_id,
            before_current_team_id,
            before_active_team_id,
            current_team_id,
            active_team_id,
            effective_before,
            effective_after,
            effective_limit,
            slot_type != 0u ? kbo_foreign_injury_slot_label(slot_type) : "none",
            injured_player_id);
    }
    return 1;
}

static void kbo_log_foreign_team_add_trace(
    uint32_t caller_rva,
    const char* result_label,
    uint8_t result,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG trace_log_count = 0;
    LONG slot = InterlockedIncrement(&trace_log_count);
    if (slot > 800) {
        if (slot == 801) {
            append_log_line("foreign team_add caller trace suppressed after 800 entries");
        }
        return;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t after_original_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        after_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }

    append_logf(
        "foreign team_add caller trace #%ld caller_rva=0x%x result=%s/%u team=%u league=%u player=%u nation=%u before_current=%u before_active=%u before_original=%u after_current=%u after_active=%u after_original=%u secondary=%u dfa=%u contract_level=%u pos_group=%u pos_role=%u overall=%u talent=%u ratings=%u args=%llu,%llu,%llu,%llu,%llu,%llu",
        slot,
        caller_rva,
        result_label != NULL ? result_label : "",
        (uint32_t)result,
        team_id,
        league_id,
        player_id,
        nation_id,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id,
        after_current_team_id,
        after_active_team_id,
        after_original_team_id,
        (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET)),
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET)),
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET)),
        (unsigned long long)arg3,
        (unsigned long long)arg4,
        (unsigned long long)arg5,
        (unsigned long long)arg6,
        (unsigned long long)arg7,
        (unsigned long long)arg8);
}

static void kbo_log_foreign_roster_move_trace(
    const char* label,
    uint32_t caller_rva,
    uint8_t result,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id)
{
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* team = (uint8_t*)team_ptr;
    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG trace_log_count = 0;
    LONG slot = InterlockedIncrement(&trace_log_count);
    if (slot > 1200) {
        if (slot == 1201) {
            append_log_line("foreign roster-move trace suppressed after 1200 entries");
        }
        return;
    }

    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t after_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t after_original_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        after_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    }

    append_logf(
        "foreign roster-move trace #%ld label=%s caller_rva=0x%x result=%u team=%u league=%u player=%u nation=%u before_current=%u before_active=%u before_original=%u after_current=%u after_active=%u after_original=%u secondary=%u dfa=%u contract_level=%u pos_group=%u pos_role=%u overall=%u talent=%u ratings=%u args=%llu,%llu,%llu,%llu,%llu,%llu",
        slot,
        label != NULL ? label : "",
        caller_rva,
        (uint32_t)result,
        team_id,
        league_id,
        player_id,
        nation_id,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id,
        after_current_team_id,
        after_active_team_id,
        after_original_team_id,
        (uint32_t)player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_DFA_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_OVERALL_VALUE_OFFSET)),
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_TALENT_VALUE_OFFSET)),
        (uint32_t)(*(uint16_t*)(player + OOTP27_PLAYER_RATINGS_VALUE_OFFSET)),
        (unsigned long long)arg3,
        (unsigned long long)arg4,
        (unsigned long long)arg5,
        (unsigned long long)arg6,
        (unsigned long long)arg7,
        (unsigned long long)arg8);
}

static uint8_t ootp_kbo_roster_move_trace_common(
    const char* label,
    OotpKboTeamAddPlayerExFn original,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    uint32_t before_current_team_id = 0u;
    uint32_t before_active_team_id = 0u;
    uint32_t before_original_team_id = 0u;
    if (kbo_player_pointer_plausible(player_ptr)) {
        uint8_t* player = (uint8_t*)player_ptr;
        before_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        before_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            before_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        }
    }

    if (original == NULL) {
        kbo_log_foreign_roster_move_trace(label, caller_rva, 0u, team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8, before_current_team_id, before_active_team_id, before_original_team_id);
        return 0u;
    }

    uint8_t result = original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
    kbo_log_foreign_roster_move_trace(label, caller_rva, result, team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8, before_current_team_id, before_active_team_id, before_original_team_id);
    return result;
}

#define KBO_DEFINE_ROSTER_MOVE_TRACE_WRAPPER(name, label, trampoline) \
__declspec(noinline) uint8_t name( \
    uintptr_t team_ptr, uintptr_t player_ptr, uintptr_t arg3, uintptr_t arg4, \
    uintptr_t arg5, uintptr_t arg6, uintptr_t arg7, uintptr_t arg8) \
{ \
    return ootp_kbo_roster_move_trace_common( \
        label, trampoline, team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8); \
}

KBO_DEFINE_ROSTER_MOVE_TRACE_WRAPPER(
    ootp_kbo_roster_move_active_trace_wrapper,
    "active_move_a52950",
    g_kbo_roster_move_active_trace_trampoline)
KBO_DEFINE_ROSTER_MOVE_TRACE_WRAPPER(
    ootp_kbo_roster_move_secondary_trace_wrapper,
    "secondary_move_a565f0",
    g_kbo_roster_move_secondary_trace_trampoline)
KBO_DEFINE_ROSTER_MOVE_TRACE_WRAPPER(
    ootp_kbo_roster_move_assignment_trace_wrapper,
    "assignment_move_ab9280",
    g_kbo_roster_move_assignment_trace_trampoline)

static uint32_t kbo_read_player_u32_or_zero(uint8_t* player, uint32_t offset)
{
    return player != NULL && memory_range_readable(player + offset, sizeof(uint32_t))
        ? *(uint32_t*)(player + offset)
        : 0u;
}

static uint8_t kbo_read_player_u8_or_zero(uint8_t* player, uint32_t offset)
{
    return player != NULL && memory_range_readable(player + offset, sizeof(uint8_t))
        ? player[offset]
        : 0u;
}

static void kbo_log_player_team_assignment_trace(
    const char* label,
    uint32_t caller_rva,
    uintptr_t player_ptr,
    int32_t arg_team_id,
    int32_t arg_active_team_id,
    int32_t arg_league_id,
    uint8_t loan_flag,
    uint32_t before_current_team_id,
    uint32_t before_active_team_id,
    uint32_t before_original_team_id,
    uint32_t before_league_id,
    uint32_t before_loan_team_id,
    uint32_t before_loan_league_id,
    uint32_t before_default_team_id,
    uint8_t before_status_flags,
    uint8_t before_restricted,
    uint8_t before_secondary_restricted,
    uint8_t before_dfa,
    uint8_t before_loan_active)
{
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return;
    }

    static volatile LONG assignment_log_count = 0;
    LONG slot = InterlockedIncrement(&assignment_log_count);
    if (slot > 1800) {
        if (slot == 1801) {
            append_log_line("player team assignment trace suppressed after 1800 entries");
        }
        return;
    }

    uint32_t player_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t after_current_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t after_active_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t after_original_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t after_league_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t after_loan_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    uint32_t after_loan_league_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
    uint32_t after_default_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    uint8_t after_status_flags = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_STATUS_FLAGS_OFFSET);
    uint8_t after_restricted = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET);
    uint8_t after_secondary_restricted =
        kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET);
    uint8_t after_dfa = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_DFA_FLAG_OFFSET);
    uint8_t after_loan_active = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET);

    append_logf(
        "player team assignment trace #%ld label=%s caller_rva=0x%x player=%u nation=%u args_team=%d args_active=%d args_league=%d loan_flag=%u current=%u->%u active=%u->%u original=%u->%u league=%u->%u loan_team=%u->%u loan_league=%u->%u default_team=%u->%u status_flags=%u->%u restricted=%u->%u secondary=%u->%u dfa=%u->%u loan_active=%u->%u contract_level=%u pos_group=%u pos_role=%u overall=%d talent=%d ratings=%d",
        slot,
        label != NULL ? label : "",
        caller_rva,
        player_id,
        nation_id,
        arg_team_id,
        arg_active_team_id,
        arg_league_id,
        (uint32_t)loan_flag,
        before_current_team_id,
        after_current_team_id,
        before_active_team_id,
        after_active_team_id,
        before_original_team_id,
        after_original_team_id,
        before_league_id,
        after_league_id,
        before_loan_team_id,
        after_loan_team_id,
        before_loan_league_id,
        after_loan_league_id,
        before_default_team_id,
        after_default_team_id,
        (uint32_t)before_status_flags,
        (uint32_t)after_status_flags,
        (uint32_t)before_restricted,
        (uint32_t)after_restricted,
        (uint32_t)before_secondary_restricted,
        (uint32_t)after_secondary_restricted,
        (uint32_t)before_dfa,
        (uint32_t)after_dfa,
        (uint32_t)before_loan_active,
        (uint32_t)after_loan_active,
        (uint32_t)kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET),
        (uint32_t)kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_POSITION_GROUP_OFFSET),
        (uint32_t)kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_POSITION_ROLE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
}

static void kbo_snapshot_player_team_assignment_fields(
    uintptr_t player_ptr,
    uint32_t* current_team_id,
    uint32_t* active_team_id,
    uint32_t* original_team_id,
    uint32_t* league_id,
    uint32_t* loan_team_id,
    uint32_t* loan_league_id,
    uint32_t* default_team_id,
    uint8_t* status_flags,
    uint8_t* restricted,
    uint8_t* secondary_restricted,
    uint8_t* dfa,
    uint8_t* loan_active)
{
    uint8_t* player = kbo_player_pointer_plausible(player_ptr) ? (uint8_t*)player_ptr : NULL;
    *current_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    *active_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    *original_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    *league_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    *loan_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_LOAN_TEAM_ID_OFFSET);
    *loan_league_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_LOAN_LEAGUE_ID_OFFSET);
    *default_team_id = kbo_read_player_u32_or_zero(player, OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    *status_flags = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_STATUS_FLAGS_OFFSET);
    *restricted = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET);
    *secondary_restricted =
        kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET);
    *dfa = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_DFA_FLAG_OFFSET);
    *loan_active = kbo_read_player_u8_or_zero(player, OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET);
}

__declspec(noinline) void ootp_kbo_player_clear_team_trace_wrapper(
    uintptr_t player_ptr,
    uint8_t loan_flag)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    uint32_t before_current_team_id = 0u;
    uint32_t before_active_team_id = 0u;
    uint32_t before_original_team_id = 0u;
    uint32_t before_league_id = 0u;
    uint32_t before_loan_team_id = 0u;
    uint32_t before_loan_league_id = 0u;
    uint32_t before_default_team_id = 0u;
    uint8_t before_status_flags = 0u;
    uint8_t before_restricted = 0u;
    uint8_t before_secondary_restricted = 0u;
    uint8_t before_dfa = 0u;
    uint8_t before_loan_active = 0u;
    kbo_snapshot_player_team_assignment_fields(
        player_ptr,
        &before_current_team_id,
        &before_active_team_id,
        &before_original_team_id,
        &before_league_id,
        &before_loan_team_id,
        &before_loan_league_id,
        &before_default_team_id,
        &before_status_flags,
        &before_restricted,
        &before_secondary_restricted,
        &before_dfa,
        &before_loan_active);

    OotpKboPlayerClearTeamTraceFn original = g_kbo_player_clear_team_trace_trampoline;
    if (original != NULL) {
        original(player_ptr, loan_flag);
    }

    kbo_log_player_team_assignment_trace(
        "clear_team_7d7870",
        caller_rva,
        player_ptr,
        0,
        0,
        0,
        loan_flag,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id,
        before_league_id,
        before_loan_team_id,
        before_loan_league_id,
        before_default_team_id,
        before_status_flags,
        before_restricted,
        before_secondary_restricted,
        before_dfa,
        before_loan_active);
}

__declspec(noinline) void ootp_kbo_player_set_team_trace_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    int32_t active_team_id,
    int32_t league_id,
    uint8_t loan_flag)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    uint32_t before_current_team_id = 0u;
    uint32_t before_active_team_id = 0u;
    uint32_t before_original_team_id = 0u;
    uint32_t before_league_id = 0u;
    uint32_t before_loan_team_id = 0u;
    uint32_t before_loan_league_id = 0u;
    uint32_t before_default_team_id = 0u;
    uint8_t before_status_flags = 0u;
    uint8_t before_restricted = 0u;
    uint8_t before_secondary_restricted = 0u;
    uint8_t before_dfa = 0u;
    uint8_t before_loan_active = 0u;
    kbo_snapshot_player_team_assignment_fields(
        player_ptr,
        &before_current_team_id,
        &before_active_team_id,
        &before_original_team_id,
        &before_league_id,
        &before_loan_team_id,
        &before_loan_league_id,
        &before_default_team_id,
        &before_status_flags,
        &before_restricted,
        &before_secondary_restricted,
        &before_dfa,
        &before_loan_active);

    OotpKboPlayerSetTeamTraceFn original = g_kbo_player_set_team_trace_trampoline;
    if (original != NULL) {
        original(player_ptr, team_id, active_team_id, league_id, loan_flag);
    }

    kbo_log_player_team_assignment_trace(
        "set_team_7d78e0",
        caller_rva,
        player_ptr,
        team_id,
        active_team_id,
        league_id,
        loan_flag,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id,
        before_league_id,
        before_loan_team_id,
        before_loan_league_id,
        before_default_team_id,
        before_status_flags,
        before_restricted,
        before_secondary_restricted,
        before_dfa,
        before_loan_active);
}

__declspec(noinline) double ootp_kbo_player_eval_double_trace_wrapper(
    uintptr_t player_ptr,
    uint8_t arg2,
    uint8_t arg3,
    uint8_t arg4)
{
    OotpKboPlayerEvalDoubleFn original = g_kbo_player_eval_double_trace_trampoline;
    double result = original != NULL ? original(player_ptr, arg2, arg3, arg4) : 0.0;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int16_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    int bias_adjusted = 0;
    double adjusted_result = kbo_apply_foreign_roster_eval_bias(
        result,
        caller_rva,
        player,
        current_team_id,
        active_team_id,
        league_id,
        &bias_adjusted);

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 400) {
        if (slot == 401) {
            append_log_line("ootp player eval double trace suppressed after 400 foreign entries");
        }
        return adjusted_result;
    }

    append_logf(
        "ootp player eval double trace #%ld caller_rva=0x%x player=%u nation=%u current=%u active=%u league=%u result=%.3f adjusted=%.3f bias=%d args=%u,%u,%u value=overall:%d talent:%d ratings:%d career:%d",
        slot,
        caller_rva,
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        result,
        adjusted_result,
        bias_adjusted,
        (uint32_t)arg2,
        (uint32_t)arg3,
        (uint32_t)arg4,
        overall,
        talent,
        ratings,
        career);
    return adjusted_result;
}

__declspec(noinline) uintptr_t ootp_kbo_player_eval_cache_trace_wrapper(
    uintptr_t player_ptr,
    int32_t arg2,
    int32_t arg3,
    uint16_t arg4)
{
    OotpKboPlayerEvalCacheFn original = g_kbo_player_eval_cache_trace_trampoline;
    uintptr_t result = original != NULL ? original(player_ptr, arg2, arg3, arg4) : 0u;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 400) {
        if (slot == 401) {
            append_log_line("ootp player eval cache trace suppressed after 400 foreign entries");
        }
        return result;
    }

    double cached_score = 0.0;
    if (memory_range_readable(player + 0x1058u, sizeof(double))) {
        cached_score = *(double*)(player + 0x1058u);
    }

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int16_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    append_logf(
        "ootp player eval cache trace #%ld caller_rva=0x%x player=%u nation=%u current=%u active=%u league=%u cached1058=%.3f args=%d,%d,%u value=overall:%d talent:%d ratings:%d career:%d",
        slot,
        caller_rva,
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        cached_score,
        arg2,
        arg3,
        (uint32_t)arg4,
        overall,
        talent,
        ratings,
        career);
    return result;
}

__declspec(noinline) int32_t ootp_kbo_ai_player_quality_trace_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uint8_t arg3,
    uint8_t arg4)
{
    OotpKboAiPlayerQualityFn original = g_kbo_ai_player_quality_trace_trampoline;
    int32_t original_result = original != NULL ? original(player_ptr, team_id, arg3, arg4) : 0;
    int32_t result = original_result;
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return result;
    }

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    int roster_relevant = kbo_ai_player_quality_trace_caller_is_roster_relevant(caller_rva);

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int status_probe_player = kbo_ai_foreign_status_write_probe_allows_player(player_id);
    if (!roster_relevant && !status_probe_player) {
        return result;
    }

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t bias_team_id = 0u;
    int quality_bias_adjusted = 0;
    int quality_bias_allowed = 0;
    const char* quality_bias_kind = "none";
    result = kbo_apply_ai_player_quality_candidate_bias(
        original_result,
        caller_rva,
        team_id,
        player,
        &bias_team_id,
        &quality_bias_adjusted,
        &quality_bias_allowed);
    if (quality_bias_adjusted) {
        quality_bias_kind = "candidate";
    } else {
        result = kbo_apply_ai_player_quality_status25_bias(
            result,
            caller_rva,
            team_id,
            player,
            &bias_team_id,
            &quality_bias_adjusted,
            &quality_bias_allowed);
        if (quality_bias_adjusted) {
            quality_bias_kind = "status25";
        }
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 1600 && !status_probe_player) {
        if (slot == 1601) {
            append_log_line("ootp ai player-quality trace suppressed after 1600 foreign entries");
        }
        return result;
    }

    int16_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t fit_d44 = kbo_read_player_i16(player, 0xd44u);
    int16_t fit_d46 = kbo_read_player_i16(player, 0xd46u);
    int16_t fit_d4a = kbo_read_player_i16(player, 0xd4au);

    append_logf(
        "ootp ai player-quality trace #%ld caller_rva=0x%x phase=%s result=%d adjusted=%d bias=%d bias_kind=%s bias_allowed=%d bias_team=%u player=%u nation=%u current=%u active=%u league=%u team_arg=%d arg3=%u arg4=%u contract_level=%u pos_group=%u pos_role=%u status_c78=%u status_c79=%u rank_bc7=%u value_ac=%u fit_d44=%d fit_d46=%d fit_d4a=%d overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        kbo_ai_player_quality_trace_phase(caller_rva),
        original_result,
        result,
        quality_bias_adjusted,
        quality_bias_kind,
        quality_bias_allowed,
        bias_team_id,
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        team_id,
        (uint32_t)arg3,
        (uint32_t)arg4,
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        (uint32_t)player[0xbc7u],
        (uint32_t)player[0xacu],
        fit_d44,
        fit_d46,
        fit_d4a,
        overall,
        talent,
        ratings);
    return result;
}

__declspec(noinline) int32_t ootp_kbo_ai_roster_role_check_trace_wrapper(
    uintptr_t player_ptr,
    int32_t arg2)
{
    OotpKboAiRosterRoleCheckFn original = g_kbo_ai_roster_role_check_trace_trampoline;
    int32_t result = original != NULL ? original(player_ptr, arg2) : 0;
    int32_t native_result = result;
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    if (caller_rva < 0x00DD8600u || caller_rva > 0x00DDD100u) {
        return result;
    }
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int target_player = player_id == 5320u || player_id == 5381u || player_id == 5404u
        || player_id == 5368u || player_id == 5293u || player_id == 5417u;
    int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
    if (!target_player && !foreign_player) {
        return result;
    }

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int league_matches = kbo_league_id == 0u
        || league_id == kbo_league_id
        || league_id == kbo_league_id + 1u;
    if (!target_player && !league_matches) {
        return result;
    }

    uint32_t default_team_id = 0u;
    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr == 0u && default_team_id != 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
        status_ptr = g_kbo_player_team_status_by_id_lookup_fn(player_ptr, (int32_t)default_team_id);
    }

    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
    }

    int role_bias_adjusted = 0;
    uint32_t role_bias_team_id = 0u;
    int role_bias_allowed = 0;
    int32_t role_bias_result = result;
    if (foreign_player
            && kbo_ai_roster_foreign_role_bias_enabled()
            && player[0xf62u] == 0u
            && player[0xf65u] == 0u
            && player[0xf25u] >= KBO_AI_ROSTER_FOREIGN_F25_MIN
            && kbo_ai_roster_source_role_gate_nonblocking_result(
                caller_rva,
                native_result,
                &role_bias_result)
            && kbo_ai_player_quality_minor_foreign_callup_allows(
                (int32_t)league_id,
                player,
                &role_bias_team_id,
                &role_bias_allowed)) {
        result = role_bias_result;
        role_bias_adjusted = 1;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 2000 && !target_player && !role_bias_adjusted) {
        if (slot == 2001) {
            append_log_line("ootp ai roster role-check trace suppressed after 2000 foreign entries");
        }
        return result;
    }

    int32_t scan_first = -1;
    int32_t scan_last = -1;
    int has_scan = kbo_ai_roster_role_check_scan_bounds(arg2, &scan_first, &scan_last);
    int32_t computed_result = 0;
    uint32_t scan_mask = has_scan
        ? kbo_ai_roster_role_nonzero_mask(player, scan_first, scan_last, &computed_result)
        : 0u;
    int32_t all_nonzero_count = 0;
    uint32_t all_role_mask = kbo_ai_roster_role_nonzero_mask(player, 5, 0, &all_nonzero_count);
    int32_t first_nonzero_slot = has_scan
        ? kbo_ai_roster_role_first_nonzero_slot(player, scan_first, scan_last)
        : -1;
    uint32_t first_nonzero_offset = first_nonzero_slot >= 0
        ? 0x800u + (uint32_t)first_nonzero_slot * sizeof(uint16_t)
        : 0u;
    uint32_t first_nonzero_value = first_nonzero_slot >= 0
        ? (uint32_t)kbo_ai_roster_role_slot_value(player, first_nonzero_slot)
        : 0u;
    int source_push_blocked = kbo_ai_roster_role_check_blocks_source_push(caller_rva, result);
    int native_source_push_blocked = kbo_ai_roster_role_check_blocks_source_push(caller_rva, native_result);

    append_logf(
        "ootp ai roster role-check trace #%ld caller_rva=0x%x phase=%s result=%d native_result=%d role_bias=%d role_bias_team=%u role_bias_allowed=%d computed=%d arg2=%d scan=%d..%d scan_mask=0x%x all_role_mask=0x%x all_role_count=%d first_nonzero_slot=%d first_nonzero_offset=0x%x first_nonzero_value=%u source_push_blocked=%d native_source_push_blocked=%d player=%u target=%d foreign=%d nation=%u current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u c78=%u c79=%u f25=%u f62=%u f65=%u f6a=%u f06=%d bce=%u fec=%u pos_group=%u pos_role=%u role800=%u role802=%u role804=%u role806=%u role808=%u role80a=%u value_ac=%u overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        kbo_ai_roster_role_check_trace_phase(caller_rva),
        result,
        native_result,
        role_bias_adjusted,
        role_bias_team_id,
        role_bias_allowed,
        computed_result,
        arg2,
        scan_first,
        scan_last,
        scan_mask,
        all_role_mask,
        all_nonzero_count,
        first_nonzero_slot,
        first_nonzero_offset,
        first_nonzero_value,
        source_push_blocked,
        native_source_push_blocked,
        player_id,
        target_player,
        foreign_player,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        status24,
        status25,
        status26,
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        (uint32_t)player[0xf25u],
        (uint32_t)player[0xf62u],
        (uint32_t)player[0xf65u],
        (uint32_t)player[0xf6au],
        kbo_read_player_i16(player, 0xf06u),
        (uint32_t)player[0xbceu],
        (uint32_t)player[0xfecu],
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)kbo_read_player_u16_or_zero(player, 0x800u),
        (uint32_t)kbo_read_player_u16_or_zero(player, 0x802u),
        (uint32_t)kbo_read_player_u16_or_zero(player, 0x804u),
        (uint32_t)kbo_read_player_u16_or_zero(player, 0x806u),
        (uint32_t)kbo_read_player_u16_or_zero(player, 0x808u),
        (uint32_t)kbo_read_player_u16_or_zero(player, 0x80au),
        (uint32_t)player[0xacu],
        kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
    return result;
}

__declspec(noinline) int32_t ootp_kbo_ai_roster_post_sort_gate_score_trace_wrapper(
    uintptr_t player_ptr)
{
    OotpKboAiRosterPostSortGateScoreFn original =
        g_kbo_ai_roster_post_sort_gate_score_trace_trampoline;
    int32_t result = original != NULL ? original(player_ptr) : -1;
    int32_t native_result = result;

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    if (caller_rva < 0x00DD8600u || caller_rva > 0x00DDD100u) {
        return result;
    }
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int target_player = kbo_ai_roster_target_player_id(player_id);
    int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);

    KboAiRosterSelectLiveTrace* live_trace = kbo_ai_roster_select_live_trace_get();
    uint32_t live_push_rva = 0u;
    int32_t live_candidate_index = kbo_ai_roster_select_live_trace_candidate_index(
        live_trace,
        player_ptr,
        &live_push_rva);
    int32_t live_sort_rank = kbo_ai_roster_select_live_trace_last_sorted_rank(
        live_trace,
        live_candidate_index);
    int live_trace_active = live_trace != NULL && live_trace->active && live_candidate_index >= 0;
    int top_live_candidate = live_trace_active && live_sort_rank >= 0 && live_sort_rank < 6;

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t default_team_id = 0u;
    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr == 0u && default_team_id != 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
        status_ptr = g_kbo_player_team_status_by_id_lookup_fn(player_ptr, (int32_t)default_team_id);
    }

    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
    }

    uint32_t post_sort_bias_team_id = 0u;
    int post_sort_bias_allowed = 0;
    int post_sort_bias_adjusted = 0;
    int post_sort_bias_context = caller_rva == KBO_AI_ROSTER_POST_SORT_GATE_SCORE_CALLER_DDC8FA_RVA
        && live_trace_active
        && live_trace->last_sort_caller_rva == KBO_POINTER_VECTOR_SORT_CALLER_DDC8C5_RVA
        && live_trace->last_sort_comparator_rva == KBO_AI_ROSTER_SELECT_COMPARATOR_917380_RVA;
    if (post_sort_bias_context
            && foreign_player
            && native_result >= 0
            && native_result < KBO_AI_ROSTER_POST_SORT_GATE_SCORE_ACCEPT_DAYS
            && kbo_ai_roster_foreign_post_sort_bias_enabled()
            && player[0xf62u] == 0u
            && player[0xf65u] == 0u
            && player[0xf25u] >= KBO_AI_ROSTER_FOREIGN_F25_MIN
            && kbo_ai_player_quality_minor_foreign_callup_allows(
                (int32_t)league_id,
                player,
                &post_sort_bias_team_id,
                &post_sort_bias_allowed)) {
        result = KBO_AI_ROSTER_POST_SORT_GATE_SCORE_ACCEPT_DAYS;
        post_sort_bias_adjusted = 1;
    }

    int focus = post_sort_bias_adjusted || target_player || foreign_player || top_live_candidate;
    if (!focus) {
        return result;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 4000 && !target_player && !post_sort_bias_adjusted) {
        if (slot == 4001) {
            append_log_line("ootp ai roster post-sort gate trace suppressed after 4000 entries");
        }
        return result;
    }

    int32_t all_nonzero_count = 0;
    uint32_t all_role_mask = kbo_ai_roster_role_nonzero_mask(player, 5, 0, &all_nonzero_count);
    append_logf(
        "ootp ai roster post-sort gate trace #%ld caller_rva=0x%x phase=%s result=%d native_result=%d post_sort_bias=%d post_sort_bias_team=%u post_sort_bias_allowed=%d post_sort_context=%d live_context=%p live_slot=%d live_depth=%d live_candidates=%d live_foreign=%d live_targets=%d live_candidate_idx=%d live_sort_rank=%d live_push_rva=0x%x last_sort_rva=0x%x last_cmp_rva=0x%x player=%u target=%d foreign=%d nation=%u current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f25=%u f62=%u f65=%u f6a=%u f06=%d bce=%u fec=%u role_mask=0x%x role_count=%d value_ac=%u overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        kbo_ai_roster_post_sort_gate_score_trace_phase(caller_rva),
        result,
        native_result,
        post_sort_bias_adjusted,
        post_sort_bias_team_id,
        post_sort_bias_allowed,
        post_sort_bias_context,
        live_trace != NULL ? (void*)live_trace->context_ptr : NULL,
        live_trace != NULL ? live_trace->slot_index : -1,
        live_trace != NULL ? live_trace->depth_hint : -1,
        live_trace != NULL ? live_trace->candidate_count : -1,
        live_trace != NULL ? live_trace->foreign_count : -1,
        live_trace != NULL ? live_trace->target_count : -1,
        live_candidate_index,
        live_sort_rank,
        live_push_rva,
        live_trace != NULL ? live_trace->last_sort_caller_rva : 0u,
        live_trace != NULL ? live_trace->last_sort_comparator_rva : 0u,
        player_id,
        target_player,
        foreign_player,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        status24,
        status25,
        status26,
        (uint32_t)player[0xf25u],
        (uint32_t)player[0xf62u],
        (uint32_t)player[0xf65u],
        (uint32_t)player[0xf6au],
        kbo_read_player_i16(player, 0xf06u),
        (uint32_t)player[0xbceu],
        (uint32_t)player[0xfecu],
        all_role_mask,
        all_nonzero_count,
        (uint32_t)player[0xacu],
        kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
    return result;
}

__declspec(noinline) int32_t ootp_kbo_ai_team_player_fit_trace_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    int32_t arg4,
    uint8_t arg5)
{
    OotpKboAiTeamPlayerFitFn original = g_kbo_ai_team_player_fit_trace_trampoline;
    int32_t original_result = original != NULL ? original(team_ptr, player_ptr, arg3, arg4, arg5) : 0;
    int32_t result = original_result;
    if (team_ptr == 0
            || player_ptr == 0
            || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    uint8_t* team = (uint8_t*)team_ptr;
    uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    int bias_adjusted = 0;
    uint32_t bias_team_id = 0u;
    result = kbo_apply_ai_team_player_fit_bias(
        original_result,
        caller_rva,
        player,
        team_league_id,
        current_league_id,
        &bias_team_id,
        &bias_adjusted);
    uint8_t value_ac_before_probe = player[0xacu];
    int value_ac_probe_adjusted = 0;
    if (bias_adjusted
            && caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_A39D24_RVA
            && kbo_ai_foreign_status_write_probe_allows_player(player_id)
            && kbo_ai_foreign_value_ac_probe_enabled()
            && player[0xacu] < 220u) {
        player[0xacu] = 220u;
        value_ac_probe_adjusted = 1;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 800) {
        if (slot == 801) {
            append_log_line("ootp ai team-player fit trace suppressed after 800 foreign entries");
        }
        return result;
    }

    int16_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int16_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int16_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int16_t fit_d44 = kbo_read_player_i16(player, 0xd44u);
    int16_t fit_d46 = kbo_read_player_i16(player, 0xd46u);
    int16_t fit_d4a = kbo_read_player_i16(player, 0xd4au);
    char arg3_bytes[768] = {0};
    int32_t arg3_self_id_offset = -1;
    if (kbo_ai_foreign_status_write_probe_allows_player(player_id)
            && (caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_A3985D_RVA
                || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_A3991E_RVA
                || caller_rva == KBO_AI_TEAM_PLAYER_FIT_CALLER_A39D24_RVA)) {
        kbo_format_probe_bytes(arg3, 256u, arg3_bytes, sizeof(arg3_bytes));
        arg3_self_id_offset = kbo_find_u32_in_probe_bytes(arg3, 256u, player_id);
    }
    append_logf(
        "ootp ai team-player fit trace #%ld caller_rva=0x%x phase=%s result=%d adjusted=%d bias=%d bias_team=%u team=%u team_league=%u player=%u nation=%u current=%u active=%u league=%u team_ptr=%p player_ptr=%p arg3=0x%llx arg3_self_id_offset=%d arg3_bytes=%s arg4=%d arg5=%u contract_level=%u pos_group=%u pos_role=%u status_c78=%u status_c79=%u rank_bc7=%u value_ac=%u value_ac_before_probe=%u value_ac_probe=%d fit_d44=%d fit_d46=%d fit_d4a=%d overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        kbo_ai_team_player_fit_trace_phase(caller_rva),
        original_result,
        result,
        bias_adjusted,
        bias_team_id,
        team_id,
        team_league_id,
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        current_league_id,
        (void*)team_ptr,
        (void*)player_ptr,
        (unsigned long long)arg3,
        arg3_self_id_offset,
        arg3_bytes[0] != '\0' ? arg3_bytes : "-",
        arg4,
        (uint32_t)arg5,
        (uint32_t)player[OOTP27_PLAYER_CONTRACT_LEVEL_FLAG_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        (uint32_t)player[0xbc7u],
        (uint32_t)player[0xacu],
        (uint32_t)value_ac_before_probe,
        value_ac_probe_adjusted,
        fit_d44,
        fit_d46,
        fit_d4a,
        overall,
        talent,
        ratings);
    return result;
}

__declspec(noinline) void ootp_kbo_ai_roster_f65_update_trace_wrapper(
    uintptr_t context_ptr,
    uintptr_t player_ptr,
    int32_t arg3)
{
    uint8_t* player = (uint8_t*)player_ptr;
    int before_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    uint32_t before_f25 = before_plausible ? (uint32_t)player[0xf25u] : 0u;
    uint32_t before_f26 = before_plausible ? (uint32_t)player[0xf26u] : 0u;
    uint32_t before_f38 = before_plausible ? (uint32_t)player[0xf38u] : 0u;
    uint32_t before_f61 = before_plausible ? (uint32_t)player[0xf61u] : 0u;
    uint32_t before_f62 = before_plausible ? (uint32_t)player[0xf62u] : 0u;
    uint32_t before_f65 = before_plausible ? (uint32_t)player[0xf65u] : 0u;
    uint32_t before_f6a = before_plausible ? (uint32_t)player[0xf6au] : 0u;
    int16_t before_f06 = before_plausible ? kbo_read_player_i16(player, 0xf06u) : 0;
    uint32_t before_fec = before_plausible && memory_range_readable(player + 0xfecu, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0xfecu)
        : 0u;
    uint32_t before_role800 = before_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 0) : 0u;
    uint32_t before_role802 = before_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 1) : 0u;
    uint32_t before_role804 = before_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 2) : 0u;
    uint32_t before_role806 = before_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 3) : 0u;
    uint32_t before_role808 = before_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 4) : 0u;
    uint32_t before_role80a = before_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 5) : 0u;

    OotpKboAiRosterF65UpdateFn original = g_kbo_ai_roster_f65_update_trace_trampoline;
    if (original != NULL) {
        original(context_ptr, player_ptr, arg3);
    }

    int after_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    if (!after_plausible) {
        return;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int target_player = kbo_ai_roster_research_target_player_id(player_id);
    int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
    if (!target_player && !foreign_player) {
        return;
    }

    uint32_t after_f25 = (uint32_t)player[0xf25u];
    uint32_t after_f26 = (uint32_t)player[0xf26u];
    uint32_t after_f38 = (uint32_t)player[0xf38u];
    uint32_t after_f61 = (uint32_t)player[0xf61u];
    uint32_t after_f62 = (uint32_t)player[0xf62u];
    uint32_t after_f65 = (uint32_t)player[0xf65u];
    uint32_t after_f6a = (uint32_t)player[0xf6au];
    int16_t after_f06 = kbo_read_player_i16(player, 0xf06u);
    uint32_t after_fec = memory_range_readable(player + 0xfecu, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0xfecu)
        : 0u;
    uint32_t after_role800 = (uint32_t)kbo_ai_roster_role_slot_value(player, 0);
    uint32_t after_role802 = (uint32_t)kbo_ai_roster_role_slot_value(player, 1);
    uint32_t after_role804 = (uint32_t)kbo_ai_roster_role_slot_value(player, 2);
    uint32_t after_role806 = (uint32_t)kbo_ai_roster_role_slot_value(player, 3);
    uint32_t after_role808 = (uint32_t)kbo_ai_roster_role_slot_value(player, 4);
    uint32_t after_role80a = (uint32_t)kbo_ai_roster_role_slot_value(player, 5);
    uint32_t native_f25 = after_f25;
    uint32_t native_f65 = after_f65;
    int f25_bias_adjusted = 0;
    uint32_t f25_bias_team_id = 0u;
    int f25_bias_allowed = 0;
    if (foreign_player
            && kbo_ai_roster_foreign_f25_bias_enabled()
            && (after_f65 != 0u || (after_f25 > 0u && after_f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN))
            && kbo_ai_player_quality_minor_foreign_callup_allows(
                (int32_t)*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                player,
                &f25_bias_team_id,
                &f25_bias_allowed)) {
        if (after_f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN) {
            player[0xf25u] = (uint8_t)KBO_AI_ROSTER_FOREIGN_F25_MIN;
        }
        if (after_f65 != 0u) {
            player[0xf65u] = 0u;
        }
        after_f25 = (uint32_t)player[0xf25u];
        after_f65 = (uint32_t)player[0xf65u];
        f25_bias_adjusted = 1;
    }
    int changed = before_plausible
        && (before_f25 != after_f25
            || before_f26 != after_f26
            || before_f38 != after_f38
            || before_f61 != after_f61
            || before_f62 != after_f62
            || before_f65 != after_f65
            || before_f6a != after_f6a
            || before_f06 != after_f06
            || before_fec != after_fec
            || before_role800 != after_role800
            || before_role802 != after_role802
            || before_role804 != after_role804
            || before_role806 != after_role806
            || before_role808 != after_role808
            || before_role80a != after_role80a);
    if (!target_player && !changed && !f25_bias_adjusted) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 1500 && !target_player) {
        if (slot == 1501) {
            append_log_line("ootp ai roster f65 update trace suppressed after 1500 foreign entries");
        }
        return;
    }

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t default_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }

    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x30u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
    }

    append_logf(
        "ootp ai roster f65 update trace #%ld caller_rva=0x%x changed=%d context=%p arg3=%d f25_bias=%d bias_team=%u bias_allowed=%d native_f25=%u native_f65=%u player=%u foreign=%d nation=%u current=%u active=%u league=%u default_team=%u status_ptr=%p status24=%u status25=%u status26=%u f25=%u->%u f26=%u->%u f38=%u->%u f61=%u->%u f62=%u->%u f65=%u->%u f6a=%u->%u f06=%d->%d fec=%u->%u role800=%u->%u role802=%u->%u role804=%u->%u role806=%u->%u role808=%u->%u role80a=%u->%u overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        changed,
        (void*)context_ptr,
        arg3,
        f25_bias_adjusted,
        f25_bias_team_id,
        f25_bias_allowed,
        native_f25,
        native_f65,
        player_id,
        foreign_player,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        (void*)status_ptr,
        status24,
        status25,
        status26,
        before_f25,
        after_f25,
        before_f26,
        after_f26,
        before_f38,
        after_f38,
        before_f61,
        after_f61,
        before_f62,
        after_f62,
        before_f65,
        after_f65,
        before_f6a,
        after_f6a,
        before_f06,
        after_f06,
        before_fec,
        after_fec,
        before_role800,
        after_role800,
        before_role802,
        after_role802,
        before_role804,
        after_role804,
        before_role806,
        after_role806,
        before_role808,
        after_role808,
        before_role80a,
        after_role80a,
        kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
}

__declspec(noinline) uint8_t ootp_kbo_ai_roster_eligibility_trace_wrapper(
    uintptr_t player_ptr,
    uint8_t arg2,
    uint32_t arg3,
    uint8_t arg4,
    int32_t arg5,
    uint8_t arg6)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    int pre_f25_bias_adjusted = 0;
    uint32_t pre_f25_bias_team_id = 0u;
    int pre_f25_bias_allowed = 0;
    uint32_t pre_native_f25 = 0u;
    uint32_t pre_native_f65 = 0u;
    if (kbo_player_pointer_plausible(player_ptr)
            && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t* pre_player = (uint8_t*)player_ptr;
        pre_native_f25 = (uint32_t)pre_player[0xf25u];
        pre_native_f65 = (uint32_t)pre_player[0xf65u];
        if (kbo_player_is_foreign_for_kbo_rights(pre_player)
                && kbo_ai_roster_foreign_f25_bias_enabled()
                && (pre_native_f65 != 0u
                    || (pre_native_f25 > 0u && pre_native_f25 < KBO_AI_ROSTER_FOREIGN_F25_MIN))
                && kbo_ai_player_quality_minor_foreign_callup_allows(
                    (int32_t)*(uint32_t*)(pre_player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET),
                    pre_player,
                    &pre_f25_bias_team_id,
                    &pre_f25_bias_allowed)) {
            if (pre_player[0xf25u] < KBO_AI_ROSTER_FOREIGN_F25_MIN) {
                pre_player[0xf25u] = (uint8_t)KBO_AI_ROSTER_FOREIGN_F25_MIN;
            }
            if (pre_player[0xf65u] != 0u) {
                pre_player[0xf65u] = 0u;
            }
            pre_f25_bias_adjusted = 1;
        }
    }

    OotpKboAiRosterEligibilityFn original = g_kbo_ai_roster_eligibility_trace_trampoline;
    uint8_t result = original != NULL ? original(player_ptr, arg2, arg3, arg4, arg5, arg6) : 0u;
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int target_player = player_id == 5320u || player_id == 5381u;
    if (!target_player && !kbo_ai_roster_eligibility_trace_caller_is_relevant(caller_rva)) {
        return result;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 2000 && !target_player) {
        if (slot == 2001) {
            append_log_line("ootp ai roster eligibility trace suppressed after 2000 foreign entries");
        }
        return result;
    }

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t default_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }

    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr == 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
        status_ptr = g_kbo_player_team_status_by_id_lookup_fn(player_ptr, (int32_t)default_team_id);
    }

    uint32_t status21 = 0u;
    uint32_t status22 = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    uint32_t status27 = 0u;
    uint32_t status28 = 0u;
    uint32_t status2b = 0u;
    uint32_t status8 = 0u;
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x30u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status8 = status[0x08u];
        status21 = status[0x21u];
        status22 = status[0x22u];
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
        status27 = status[0x27u];
        status28 = status[0x28u];
        status2b = status[0x2bu];
    }

    uint32_t count1694 = memory_range_readable(player + 0x1694u, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0x1694u)
        : 0u;
    uint32_t normalized_type = arg3 != 0u ? arg3 : (player[0xc78u] == 1u ? 1u : 10u);
    const char* likely_gate = "none";
    if (player[0xf62u] != 0u) {
        likely_gate = "f62";
    } else if (arg2 != 0u && normalized_type == 1u && player[0xf65u] != 0u) {
        likely_gate = "f65";
    } else if (arg2 != 0u && normalized_type >= 11u && player[0xf65u] != 0u) {
        likely_gate = "f65";
    } else if (arg2 != 0u && normalized_type >= 2u && normalized_type <= 10u && player[0xf6au] != 0u) {
        likely_gate = "f6a";
    } else if (default_team_id == 0u) {
        likely_gate = "default_team_zero";
    } else if (arg6 == 0u && status21 > 0u) {
        likely_gate = "status21";
    } else if (arg6 != 0u && arg2 != 0u && status21 > 0u && count1694 > 0u) {
        likely_gate = "status21_count1694";
    } else if (result == 0u && arg4 != 0u) {
        likely_gate = "subgate_or_late";
    }

    append_logf(
        "ootp ai roster eligibility trace #%ld caller_rva=0x%x phase=%s result=%u likely_gate=%s pre_f25_bias=%d pre_bias_team=%u pre_bias_allowed=%d pre_native_f25=%u pre_native_f65=%u player=%u nation=%u current=%u active=%u league=%u default_team=%u normalized_type=%u args=%u,%u,%u,%d,%u status_ptr=%p status8=%u status21=%u status22=%u status24=%u status25=%u status26=%u status27=%u status28=%u status2b=%u c78=%u c79=%u f25=%u f62=%u f65=%u f6a=%u b4=%u ec4=%u flag874=%u flag896=%u count1694=%u pos_group=%u pos_role=%u value_ac=%u overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        kbo_ai_roster_eligibility_trace_phase(caller_rva),
        (uint32_t)result,
        likely_gate,
        pre_f25_bias_adjusted,
        pre_f25_bias_team_id,
        pre_f25_bias_allowed,
        pre_native_f25,
        pre_native_f65,
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        normalized_type,
        (uint32_t)arg2,
        arg3,
        (uint32_t)arg4,
        arg5,
        (uint32_t)arg6,
        (void*)status_ptr,
        status8,
        status21,
        status22,
        status24,
        status25,
        status26,
        status27,
        status28,
        status2b,
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        (uint32_t)player[0xf25u],
        (uint32_t)player[0xf62u],
        (uint32_t)player[0xf65u],
        (uint32_t)player[0xf6au],
        *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET),
        default_team_id,
        (uint32_t)player[0x874u],
        (uint32_t)player[0x896u],
        count1694,
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)player[0xacu],
        kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
    return result;
}

__declspec(noinline) uint8_t ootp_kbo_ai_roster_availability_trace_wrapper(
    uintptr_t player_ptr,
    int32_t arg2)
{
    OotpKboAiRosterAvailabilityFn original = g_kbo_ai_roster_availability_trace_trampoline;
    uint8_t result = original != NULL ? original(player_ptr, arg2) : 0u;

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int target_player = kbo_ai_roster_research_target_player_id(player_id);
    if (!target_player && !kbo_ai_roster_availability_trace_caller_is_relevant(caller_rva)) {
        return result;
    }

    int foreign_player = kbo_player_is_foreign_for_kbo_rights(player);
    if (!target_player && !foreign_player) {
        return result;
    }

    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int league_matches = kbo_league_id == 0u
        || league_id == kbo_league_id
        || league_id == kbo_league_id + 1u;
    if (!target_player && !league_matches) {
        return result;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 3000 && !target_player) {
        if (slot == 3001) {
            append_log_line("ootp ai roster availability trace suppressed after 3000 foreign entries");
        }
        return result;
    }

    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t default_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }

    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr == 0u && default_team_id != 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
        status_ptr = g_kbo_player_team_status_by_id_lookup_fn(player_ptr, (int32_t)default_team_id);
    }

    uint32_t status8 = 0u;
    uint32_t status21 = 0u;
    uint32_t status22 = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    uint32_t status27 = 0u;
    uint32_t status28 = 0u;
    uint32_t status2b = 0u;
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x30u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status8 = status[0x08u];
        status21 = status[0x21u];
        status22 = status[0x22u];
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
        status27 = status[0x27u];
        status28 = status[0x28u];
        status2b = status[0x2bu];
    }

    uint32_t count1694 = memory_range_readable(player + 0x1694u, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0x1694u)
        : 0u;
    uint32_t fec = memory_range_readable(player + 0xfecu, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0xfecu)
        : 0u;

    append_logf(
        "ootp ai roster availability trace #%ld caller_rva=0x%x phase=%s result=%u original=%d arg2=%d player=%u target=%d foreign=%d nation=%u current=%u active=%u league=%u default_team=%u status_ptr=%p status8=%u status21=%u status22=%u status24=%u status25=%u status26=%u status27=%u status28=%u status2b=%u c78=%u c79=%u f25=%u f62=%u f65=%u f6a=%u f06=%d fec=%u flag101f=%u count1694=%u pos_group=%u pos_role=%u role800=%u role802=%u role804=%u role806=%u role808=%u role80a=%u value_ac=%u fe0=%d fe4=%d overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        kbo_ai_roster_availability_trace_phase(caller_rva),
        (uint32_t)result,
        original != NULL ? 1 : 0,
        arg2,
        player_id,
        target_player,
        foreign_player,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        (void*)status_ptr,
        status8,
        status21,
        status22,
        status24,
        status25,
        status26,
        status27,
        status28,
        status2b,
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        (uint32_t)player[0xf25u],
        (uint32_t)player[0xf62u],
        (uint32_t)player[0xf65u],
        (uint32_t)player[0xf6au],
        kbo_read_player_i16(player, 0xf06u),
        fec,
        (uint32_t)player[0x101fu],
        count1694,
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)kbo_ai_roster_role_slot_value(player, 0),
        (uint32_t)kbo_ai_roster_role_slot_value(player, 1),
        (uint32_t)kbo_ai_roster_role_slot_value(player, 2),
        (uint32_t)kbo_ai_roster_role_slot_value(player, 3),
        (uint32_t)kbo_ai_roster_role_slot_value(player, 4),
        (uint32_t)kbo_ai_roster_role_slot_value(player, 5),
        (uint32_t)player[0xacu],
        kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET),
        kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
    return result;
}

__declspec(noinline) uintptr_t ootp_kbo_player_team_status_lookup_trace_wrapper(
    uintptr_t player_ptr,
    int32_t team_id)
{
    OotpKboPlayerTeamStatusLookupFn original = g_kbo_player_team_status_lookup_trace_trampoline;
    uintptr_t result = original != NULL ? original(player_ptr, team_id) : 0u;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        return result;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    int status_probe_player = kbo_ai_foreign_status_write_probe_allows_player(player_id);
    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    int suppress_trace_log = slot > 1200 && !status_probe_player;

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t status22 = 0u;
    uint32_t status23 = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    if (result != 0u && memory_range_readable((void*)result, 0x28u)) {
        uint8_t* status = (uint8_t*)result;
        status22 = status[0x22u];
        status23 = status[0x23u];
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
    }

    int status_adjusted = 0;
    const char* status_adjust_kind = "none";
    uint32_t adjusted_status24 = status24;
    uint32_t adjusted_status25 = status25;
    uint32_t adjusted_status26 = status26;
    uint32_t adjusted_player_c79 = player[0xc79u];
    uint32_t status_adjust_team_id = 0u;
    int status_adjust_allowed = 0;
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int minor_league_match = kbo_league_id != 0u && league_id == kbo_league_id + 1u;
    if (result != 0u
            && memory_range_readable((void*)result, 0x28u)
            && status24 == 4u
            && status25 == 11u
            && player[0xc78u] == 1u
            && kbo_ai_status25_11_rescue_enabled()
            && kbo_ai_foreign_status_lookup_caller_is_status25_11_rescue_target(caller_rva)
            && kbo_ai_player_quality_minor_foreign_callup_allows(
                team_id,
                player,
                &status_adjust_team_id,
                &status_adjust_allowed)) {
        uint8_t* status = (uint8_t*)result;
        status[0x25u] = 0x02u;
        adjusted_status24 = status[0x24u];
        adjusted_status25 = status[0x25u];
        adjusted_status26 = status[0x26u];
        adjusted_player_c79 = player[0xc79u];
        status_adjusted = 1;
        status_adjust_kind = "status25_11_to_2";
    } else if (result != 0u
            && memory_range_readable((void*)result, 0x28u)
            && minor_league_match
            && status_probe_player
            && !read_kbo_localappdata_flag_file("disable_foreign_status_write_probe.txt")) {
        uint8_t* status = (uint8_t*)result;
        status[0x24u] = 0u;
        status[0x26u] = 0u;
        if (kbo_ai_foreign_status_lookup_caller_is_final_candidate(caller_rva)
                && kbo_ai_foreign_final_roster_candidate_probe_enabled()) {
            status[0x24u] = 0x05u;
            status[0x25u] = 0x00u;
        }
        player[0xc79u] = 11u;
        adjusted_status24 = status[0x24u];
        adjusted_status25 = status[0x25u];
        adjusted_status26 = status[0x26u];
        adjusted_player_c79 = player[0xc79u];
        status_adjusted = 1;
        status_adjust_kind = "legacy_status_probe";
    }

    if (suppress_trace_log && !status_adjusted) {
        if (slot == 1201) {
            append_log_line("ootp player-team status lookup trace suppressed after 1200 foreign entries");
        }
        return result;
    }

    append_logf(
        "ootp player-team status lookup trace #%ld caller_rva=0x%x player=%u nation=%u current=%u active=%u league=%u team_arg=%d status_ptr=%p status22=%u status23=%u status24=%u status25=%u status26=%u adjusted=%d adjust_kind=%s adjust_team=%u adjust_allowed=%d adj_status24=%u adj_status25=%u adj_status26=%u player_c78=%u player_c79=%u adj_player_c79=%u pos_role=%u value_ac=%u",
        slot,
        caller_rva,
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        team_id,
        (void*)result,
        status22,
        status23,
        status24,
        status25,
        status26,
        status_adjusted,
        status_adjust_kind,
        status_adjust_team_id,
        status_adjust_allowed,
        adjusted_status24,
        adjusted_status25,
        adjusted_status26,
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        adjusted_player_c79,
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)player[0xacu]);
    return result;
}

__declspec(noinline) uintptr_t ootp_kbo_player_default_status_lookup_trace_wrapper(
    uintptr_t player_ptr)
{
    if (player_ptr == 0u
            || !memory_range_readable(
                (void*)player_ptr,
                OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET + sizeof(uint32_t))) {
        return 0u;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uintptr_t result = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    uint32_t default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    if (result == 0u) {
        OotpKboPlayerTeamStatusLookupFn by_id = g_kbo_player_team_status_by_id_lookup_fn;
        if (by_id != NULL) {
            result = by_id(player_ptr, (int32_t)default_team_id);
        }
    }

    if (!kbo_player_pointer_plausible(player_ptr)) {
        return result;
    }
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t status8 = 0u;
    uint32_t status21 = 0u;
    uint32_t status22 = 0u;
    uint32_t status23 = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    uint32_t status27 = 0u;
    uint32_t status28 = 0u;
    if (result != 0u && memory_range_readable((void*)result, 0x29u)) {
        uint8_t* status = (uint8_t*)result;
        status8 = status[0x08u];
        status21 = status[0x21u];
        status22 = status[0x22u];
        status23 = status[0x23u];
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
        status27 = status[0x27u];
        status28 = status[0x28u];
    }
    uint32_t count1694 = memory_range_readable(player + 0x1694u, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0x1694u)
        : 0u;

    int status_adjusted = 0;
    const char* status_adjust_kind = "none";
    uint32_t adjusted_status24 = status24;
    uint32_t adjusted_status25 = status25;
    uint32_t adjusted_status26 = status26;
    uint32_t status_adjust_team_id = 0u;
    int status_adjust_allowed = 0;
    if (result != 0u
            && memory_range_readable((void*)result, 0x28u)
            && status24 == 4u
            && status25 == 11u
            && player[0xc78u] == 1u
            && kbo_ai_status25_11_rescue_enabled()
            && kbo_ai_foreign_default_status_caller_is_status25_11_rescue_target(caller_rva)
            && kbo_ai_player_quality_minor_foreign_callup_allows(
                (int32_t)default_team_id,
                player,
                &status_adjust_team_id,
                &status_adjust_allowed)) {
        uint8_t* status = (uint8_t*)result;
        status[0x25u] = 0x02u;
        adjusted_status24 = status[0x24u];
        adjusted_status25 = status[0x25u];
        adjusted_status26 = status[0x26u];
        status_adjusted = 1;
        status_adjust_kind = "default_status25_11_to_2";
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int minor_league_match = kbo_league_id != 0u && league_id == kbo_league_id + 1u;
    int important_consumer = kbo_ai_foreign_default_status_caller_is_status25_11_rescue_target(caller_rva);
    if (!status_adjusted && !important_consumer && !minor_league_match) {
        return result;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 1000 && !status_adjusted && !important_consumer) {
        if (slot == 1001) {
            append_log_line("ootp player-default status lookup trace suppressed after 1000 foreign entries");
        }
        return result;
    }

    append_logf(
        "ootp player-default status lookup trace #%ld caller_rva=0x%x phase=%s player=%u nation=%u current=%u active=%u league=%u default_team=%u status_ptr=%p status8=%u status21=%u status22=%u status23=%u status24=%u status25=%u status26=%u status27=%u status28=%u adjusted=%d adjust_kind=%s adjust_team=%u adjust_allowed=%d adj_status24=%u adj_status25=%u adj_status26=%u player_c78=%u player_c79=%u f62=%u f65=%u f6a=%u count1694=%u pos_role=%u value_ac=%u",
        slot,
        caller_rva,
        kbo_player_default_status_trace_phase(caller_rva),
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        (void*)result,
        status8,
        status21,
        status22,
        status23,
        status24,
        status25,
        status26,
        status27,
        status28,
        status_adjusted,
        status_adjust_kind,
        status_adjust_team_id,
        status_adjust_allowed,
        adjusted_status24,
        adjusted_status25,
        adjusted_status26,
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        (uint32_t)player[0xf62u],
        (uint32_t)player[0xf65u],
        (uint32_t)player[0xf6au],
        count1694,
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)player[0xacu]);
    return result;
}

__declspec(noinline) uint8_t ootp_kbo_pointer_vector_push_trace_wrapper(
    uintptr_t vector_ptr,
    uintptr_t value_ptr)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    int roster_candidate_push = kbo_pointer_vector_push_caller_is_ai_roster_candidate(caller_rva);

    int32_t before_count = -1;
    int before_contains = 0;
    if (roster_candidate_push) {
        before_count = kbo_pointer_vector_count(vector_ptr);
        before_contains = kbo_pointer_vector_contains_value(vector_ptr, value_ptr, before_count);
    }

    OotpKboPointerVectorPushFn original = g_kbo_pointer_vector_push_trace_trampoline;
    uint8_t result = original != NULL ? original(vector_ptr, value_ptr) : 0u;
    if (!roster_candidate_push) {
        return result;
    }

    int32_t after_count = kbo_pointer_vector_count(vector_ptr);
    int after_contains = kbo_pointer_vector_contains_value(vector_ptr, value_ptr, after_count);
    int inserted = after_count >= 0
        && before_count >= 0
        && after_count > before_count
        && after_contains
        && !before_contains;
    kbo_ai_roster_select_live_trace_record_push(vector_ptr, value_ptr, inserted, caller_rva);

    if (!kbo_player_pointer_plausible(value_ptr)) {
        return result;
    }

    uint8_t* player = (uint8_t*)value_ptr;
    if (!kbo_player_is_foreign_for_kbo_rights(player)) {
        return result;
    }

    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int target_player = player_id == 5320u || player_id == 5381u || player_id == 5404u
        || player_id == 5368u || player_id == 5293u || player_id == 5417u;
    int league_matches = kbo_league_id == 0u
        || league_id == kbo_league_id
        || league_id == kbo_league_id + 1u;
    if (!target_player && !league_matches) {
        return result;
    }

    uintptr_t status_ptr = 0u;
    uint32_t default_team_id = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr == 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
        status_ptr = g_kbo_player_team_status_by_id_lookup_fn(value_ptr, (int32_t)default_team_id);
    }

    uint32_t status8 = 0u;
    uint32_t status21 = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status27 = 0u;
    uint32_t status28 = 0u;
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status8 = status[0x08u];
        status21 = status[0x21u];
        status24 = status[0x24u];
        status25 = status[0x25u];
        status27 = status[0x27u];
        status28 = status[0x28u];
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 2500 && !target_player) {
        if (slot == 2501) {
            append_log_line("ootp pointer-vector push trace suppressed after 2500 foreign roster candidates");
        }
        return result;
    }

    append_logf(
        "ootp pointer-vector push trace #%ld caller_rva=0x%x phase=%s result=%u inserted=%d before_count=%d after_count=%d before_contains=%d after_contains=%d vector=%p value=%p player=%u nation=%u current=%u active=%u league=%u default_team=%u status_ptr=%p status8=%u status21=%u status24=%u status25=%u status27=%u status28=%u c78=%u c79=%u f25=%u f62=%u f65=%u f6a=%u pos_group=%u pos_role=%u value_ac=%u overall=%d talent=%d ratings=%d",
        slot,
        caller_rva,
        kbo_pointer_vector_push_trace_phase(caller_rva),
        (uint32_t)result,
        inserted,
        before_count,
        after_count,
        before_contains,
        after_contains,
        (void*)vector_ptr,
        (void*)value_ptr,
        player_id,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        (void*)status_ptr,
        status8,
        status21,
        status24,
        status25,
        status27,
        status28,
        (uint32_t)player[0xc78u],
        (uint32_t)player[0xc79u],
        (uint32_t)player[0xf25u],
        (uint32_t)player[0xf62u],
        (uint32_t)player[0xf65u],
        (uint32_t)player[0xf6au],
        (uint32_t)player[OOTP27_PLAYER_POSITION_GROUP_OFFSET],
        (uint32_t)player[OOTP27_PLAYER_POSITION_ROLE_OFFSET],
        (uint32_t)player[0xacu],
        kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET),
        kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET));
    return result;
}

static uint32_t kbo_ai_roster_priority_compare_f06_key(
    uint8_t* player,
    int apply_foreign_bonus,
    uint32_t bonus)
{
    uint32_t key = (uint32_t)kbo_read_player_u16_or_zero(player, 0xf06u);
    if (apply_foreign_bonus && key <= 0xffffu - bonus) {
        key += bonus;
    } else if (apply_foreign_bonus) {
        key = 0xffffu;
    }
    return key;
}

static uint32_t kbo_ai_roster_type_compare_c98_key(
    uint8_t* player,
    int apply_foreign_bonus,
    uint32_t bonus)
{
    uint32_t key = (uint32_t)kbo_read_player_u16_or_zero(player, 0xc98u);
    if (apply_foreign_bonus && key <= 0xffffu - bonus) {
        key += bonus;
    } else if (apply_foreign_bonus) {
        key = 0xffffu;
    }
    return key;
}

static int64_t kbo_ai_roster_score_compare_key(
    uintptr_t player_ptr,
    uint32_t score_offset,
    int apply_foreign_bonus,
    uint32_t bonus)
{
    int64_t key = (int64_t)kbo_read_ai_roster_select_score(player_ptr, score_offset);
    if (apply_foreign_bonus && key <= (int64_t)INT32_MAX - (int64_t)bonus) {
        key += (int64_t)bonus;
    } else if (apply_foreign_bonus) {
        key = (int64_t)INT32_MAX;
    }
    return key;
}

static int kbo_ai_roster_priority_compare_result_desc(
    uint32_t left_key,
    uint32_t right_key)
{
    if (left_key < right_key) {
        return 1;
    }
    if (left_key > right_key) {
        return -1;
    }
    return 0;
}

static int kbo_ai_roster_score_compare_result_desc(
    int64_t left_key,
    int64_t right_key)
{
    if (left_key < right_key) {
        return 1;
    }
    if (left_key > right_key) {
        return -1;
    }
    return 0;
}

static int kbo_ai_roster_sort_bias_context_allows_compare(
    KboAiRosterSelectLiveTrace* live_trace,
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg)
{
    return sort_arg == 0u
        && live_trace != NULL
        && live_trace->active
        && live_trace->foreign_sort_bias_active
        && live_trace->foreign_sort_bias_bonus > 0
        && kbo_ai_roster_foreign_sort_bias_enabled()
        && kbo_player_pointer_plausible(left_player_ptr)
        && kbo_player_pointer_plausible(right_player_ptr)
        && memory_range_readable((void*)left_player_ptr, OOTP27_PLAYER_SCAN_BYTES)
        && memory_range_readable((void*)right_player_ptr, OOTP27_PLAYER_SCAN_BYTES);
}

__declspec(noinline) int32_t ootp_kbo_ai_roster_priority_compare_wrapper(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg)
{
    OotpKboAiRosterPriorityCompareFn original = g_kbo_ai_roster_priority_compare_trampoline;
    KboAiRosterSelectLiveTrace* live_trace = kbo_ai_roster_select_live_trace_get();
    if (original == NULL
            || !kbo_ai_roster_sort_bias_context_allows_compare(
                live_trace,
                left_player_ptr,
                right_player_ptr,
                sort_arg)) {
        return original != NULL ? original(left_player_ptr, right_player_ptr, sort_arg) : 0;
    }

    uint8_t* left = (uint8_t*)left_player_ptr;
    uint8_t* right = (uint8_t*)right_player_ptr;
    int left_foreign = kbo_player_is_foreign_for_kbo_rights(left);
    int right_foreign = kbo_player_is_foreign_for_kbo_rights(right);
    if (left_foreign == right_foreign) {
        return original(left_player_ptr, right_player_ptr, sort_arg);
    }

    live_trace->foreign_sort_bias_mixed_compare_count++;

    uint32_t bonus = (uint32_t)live_trace->foreign_sort_bias_bonus;
    uint32_t left_key = kbo_ai_roster_priority_compare_f06_key(left, left_foreign, bonus);
    uint32_t right_key = kbo_ai_roster_priority_compare_f06_key(right, right_foreign, bonus);
    int32_t result = kbo_ai_roster_priority_compare_result_desc(left_key, right_key);
    if (result != 0) {
        live_trace->foreign_sort_bias_decision_count++;
        return result;
    }

    uint32_t left_bce = (uint32_t)left[0xbceu];
    uint32_t right_bce = (uint32_t)right[0xbceu];
    result = kbo_ai_roster_priority_compare_result_desc(left_bce, right_bce);
    if (result != 0) {
        live_trace->foreign_sort_bias_decision_count++;
        return result;
    }

    uint32_t left_base_f06 = (uint32_t)kbo_read_player_u16_or_zero(left, 0xf06u);
    uint32_t right_base_f06 = (uint32_t)kbo_read_player_u16_or_zero(right, 0xf06u);
    if (left_base_f06 == right_base_f06) {
        return original(left_player_ptr, right_player_ptr, sort_arg);
    }

    return 0;
}

__declspec(noinline) int32_t ootp_kbo_ai_roster_type_compare_wrapper(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg)
{
    OotpKboAiRosterTypeCompareFn original = g_kbo_ai_roster_type_compare_trampoline;
    KboAiRosterSelectLiveTrace* live_trace = kbo_ai_roster_select_live_trace_get();
    if (original == NULL
            || !kbo_ai_roster_sort_bias_context_allows_compare(
                live_trace,
                left_player_ptr,
                right_player_ptr,
                sort_arg)) {
        return original != NULL ? original(left_player_ptr, right_player_ptr, sort_arg) : 0;
    }

    uint8_t* left = (uint8_t*)left_player_ptr;
    uint8_t* right = (uint8_t*)right_player_ptr;
    int left_foreign = kbo_player_is_foreign_for_kbo_rights(left);
    int right_foreign = kbo_player_is_foreign_for_kbo_rights(right);
    if (left_foreign == right_foreign) {
        return original(left_player_ptr, right_player_ptr, sort_arg);
    }

    live_trace->foreign_sort_bias_mixed_compare_count++;

    uint32_t bonus = (uint32_t)live_trace->foreign_sort_bias_bonus;
    uint32_t left_key = kbo_ai_roster_type_compare_c98_key(left, left_foreign, bonus);
    uint32_t right_key = kbo_ai_roster_type_compare_c98_key(right, right_foreign, bonus);
    int32_t result = kbo_ai_roster_priority_compare_result_desc(left_key, right_key);
    if (result != 0) {
        live_trace->foreign_sort_bias_decision_count++;
        return result;
    }

    uint32_t left_id = *(uint32_t*)(left + OOTP27_PLAYER_ID_OFFSET);
    uint32_t right_id = *(uint32_t*)(right + OOTP27_PLAYER_ID_OFFSET);
    result = kbo_ai_roster_priority_compare_result_desc(left_id, right_id);
    if (result != 0) {
        live_trace->foreign_sort_bias_decision_count++;
        return result;
    }

    return original(left_player_ptr, right_player_ptr, sort_arg);
}

__declspec(noinline) int32_t ootp_kbo_ai_roster_score_compare_wrapper(
    uintptr_t left_player_ptr,
    uintptr_t right_player_ptr,
    uintptr_t sort_arg)
{
    OotpKboAiRosterScoreCompareFn original = g_kbo_ai_roster_score_compare_trampoline;
    KboAiRosterSelectLiveTrace* live_trace = kbo_ai_roster_select_live_trace_get();
    if (original == NULL
            || !kbo_ai_roster_sort_bias_context_allows_compare(
                live_trace,
                left_player_ptr,
                right_player_ptr,
                sort_arg)) {
        return original != NULL ? original(left_player_ptr, right_player_ptr, sort_arg) : 0;
    }

    uint8_t* left = (uint8_t*)left_player_ptr;
    uint8_t* right = (uint8_t*)right_player_ptr;
    int left_foreign = kbo_player_is_foreign_for_kbo_rights(left);
    int right_foreign = kbo_player_is_foreign_for_kbo_rights(right);
    if (left_foreign == right_foreign) {
        return original(left_player_ptr, right_player_ptr, sort_arg);
    }

    live_trace->foreign_sort_bias_mixed_compare_count++;

    uint32_t bonus = (uint32_t)live_trace->foreign_sort_bias_bonus;
    int64_t left_fe0 = kbo_ai_roster_score_compare_key(
        left_player_ptr,
        KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET,
        left_foreign,
        bonus);
    int64_t right_fe0 = kbo_ai_roster_score_compare_key(
        right_player_ptr,
        KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET,
        right_foreign,
        bonus);
    int32_t result = kbo_ai_roster_score_compare_result_desc(left_fe0, right_fe0);
    if (result != 0) {
        live_trace->foreign_sort_bias_decision_count++;
        return result;
    }

    int64_t left_fe4 = kbo_ai_roster_score_compare_key(
        left_player_ptr,
        KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET,
        left_foreign,
        bonus);
    int64_t right_fe4 = kbo_ai_roster_score_compare_key(
        right_player_ptr,
        KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET,
        right_foreign,
        bonus);
    result = kbo_ai_roster_score_compare_result_desc(left_fe4, right_fe4);
    if (result != 0) {
        live_trace->foreign_sort_bias_decision_count++;
        return result;
    }

    return original(left_player_ptr, right_player_ptr, sort_arg);
}

__declspec(noinline) void ootp_kbo_pointer_vector_sort_trace_wrapper(
    uintptr_t vector_ptr,
    uintptr_t comparator_ptr,
    uintptr_t sort_arg)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    uint32_t comparator_rva = host_exe != NULL && comparator_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(comparator_ptr - (uintptr_t)host_exe)
        : 0u;

    int foreign_bias_compare_sort =
        (caller_rva == KBO_POINTER_VECTOR_SORT_CALLER_DDC8C5_RVA
            && comparator_rva == KBO_AI_ROSTER_SELECT_COMPARATOR_917380_RVA)
        || ((caller_rva == KBO_POINTER_VECTOR_SORT_CALLER_DDC76D_RVA
                || caller_rva == KBO_POINTER_VECTOR_SORT_CALLER_DDCA82_RVA)
            && comparator_rva == KBO_AI_ROSTER_SELECT_COMPARATOR_909FA0_RVA)
        || ((caller_rva == KBO_POINTER_VECTOR_SORT_CALLER_DDCCE1_RVA
                || caller_rva == KBO_POINTER_VECTOR_SORT_CALLER_DDD055_RVA)
            && comparator_rva == KBO_AI_ROSTER_SELECT_SCORE_COMPARATOR_RVA);
    KboAiRosterSelectLiveTrace* live_trace_before_sort = kbo_ai_roster_select_live_trace_get();
    int enable_foreign_sort_bias = live_trace_before_sort != NULL
        && live_trace_before_sort->active
        && foreign_bias_compare_sort
        && sort_arg == 0u
        && live_trace_before_sort->foreign_count > 0
        && kbo_ai_roster_foreign_sort_bias_enabled();
    if (live_trace_before_sort != NULL && live_trace_before_sort->active) {
        live_trace_before_sort->foreign_sort_bias_active = enable_foreign_sort_bias ? 1 : 0;
        if (!enable_foreign_sort_bias) {
            live_trace_before_sort->foreign_sort_bias_bonus = 0;
        } else if (comparator_rva == KBO_AI_ROSTER_SELECT_COMPARATOR_909FA0_RVA) {
            live_trace_before_sort->foreign_sort_bias_bonus =
                (int32_t)kbo_ai_roster_foreign_sort_c98_bonus();
        } else if (comparator_rva == KBO_AI_ROSTER_SELECT_SCORE_COMPARATOR_RVA) {
            live_trace_before_sort->foreign_sort_bias_bonus =
                (int32_t)kbo_ai_roster_foreign_score_fe_bonus();
        } else {
            live_trace_before_sort->foreign_sort_bias_bonus =
                (int32_t)kbo_ai_roster_foreign_sort_f06_bonus();
        }
        live_trace_before_sort->foreign_sort_bias_mixed_compare_count = 0;
        live_trace_before_sort->foreign_sort_bias_decision_count = 0;
    }

    OotpKboPointerVectorSortFn original = g_kbo_pointer_vector_sort_trace_trampoline;
    if (original != NULL) {
        original(vector_ptr, comparator_ptr, sort_arg);
    }
    if (live_trace_before_sort != NULL && live_trace_before_sort->active) {
        live_trace_before_sort->foreign_sort_bias_active = 0;
    }

    KboAiRosterSelectLiveTrace* live_trace = kbo_ai_roster_select_live_trace_get();
    if (live_trace == NULL || !live_trace->active) {
        return;
    }

    int roster_sort = kbo_pointer_vector_sort_trace_is_roster_sort(caller_rva);
    if (!roster_sort) {
        return;
    }

    int32_t count = kbo_pointer_vector_count(vector_ptr);
    if (count <= 0) {
        return;
    }
    int32_t scan_count = count > KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        ? KBO_AI_ROSTER_SELECT_SCAN_LIMIT
        : count;

    live_trace->sort_count++;
    live_trace->last_sort_caller_rva = caller_rva;
    live_trace->last_sort_comparator_rva = comparator_rva;
    live_trace->last_sort_count = count;
    live_trace->last_sort_foreign_count = 0;
    live_trace->last_sort_target_count = 0;
    if (comparator_rva == KBO_AI_ROSTER_SELECT_SCORE_COMPARATOR_RVA) {
        live_trace->score_sort_count++;
    }
    for (int32_t i = 0; i < KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT; i++) {
        live_trace->last_sorted_rank_by_candidate[i] = -1;
    }

    KboAiRosterSortCandidateSummary top[KBO_AI_ROSTER_SORT_TOP_SUMMARY_SLOTS];
    for (int32_t i = 0; i < KBO_AI_ROSTER_SORT_TOP_SUMMARY_SLOTS; i++) {
        kbo_ai_roster_sort_candidate_summary_init(&top[i]);
    }
    KboAiRosterSortCandidateSummary first_foreign;
    KboAiRosterSortCandidateSummary first_target;
    KboAiRosterSortCandidateSummary player5381;
    KboAiRosterSortCandidateSummary player5417;
    kbo_ai_roster_sort_candidate_summary_init(&first_foreign);
    kbo_ai_roster_sort_candidate_summary_init(&first_target);
    kbo_ai_roster_sort_candidate_summary_init(&player5381);
    kbo_ai_roster_sort_candidate_summary_init(&player5417);

    int relevant = live_trace->foreign_count > 0 || live_trace->target_count > 0;
    int32_t foreign_in_sorted = 0;
    int32_t target_in_sorted = 0;
    for (int32_t i = 0; i < scan_count; i++) {
        uintptr_t candidate_ptr = kbo_pointer_vector_value_at(vector_ptr, i, count);
        KboAiRosterSortCandidateSummary summary;
        kbo_fill_ai_roster_sort_candidate_summary(&summary, i, candidate_ptr, live_trace);
        if (summary.index < 0) {
            continue;
        }
        if (summary.push_index >= 0 && summary.push_index < KBO_AI_ROSTER_LOCAL_CANDIDATE_LIMIT) {
            live_trace->last_sorted_rank_by_candidate[summary.push_index] = i;
        }
        if (i < KBO_AI_ROSTER_SORT_TOP_SUMMARY_SLOTS) {
            top[i] = summary;
        }
        if (summary.foreign) {
            foreign_in_sorted++;
            relevant = 1;
            if (first_foreign.index < 0) {
                first_foreign = summary;
            }
        }
        if (summary.target) {
            target_in_sorted++;
            relevant = 1;
            if (first_target.index < 0) {
                first_target = summary;
            }
        }
        if (summary.player_id == 5381u && player5381.index < 0) {
            player5381 = summary;
        }
        if (summary.player_id == 5417u && player5417.index < 0) {
            player5417 = summary;
        }
    }
    live_trace->last_sort_foreign_count = foreign_in_sorted;
    live_trace->last_sort_target_count = target_in_sorted;
    if (!relevant) {
        return;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 4000 && live_trace->target_count == 0) {
        if (slot == 4001) {
            append_log_line("ootp pointer-vector sort trace suppressed after 4000 roster sorts");
        }
        return;
    }

    append_logf(
        "ootp pointer-vector sort trace #%ld caller_rva=0x%x phase=%s comparator_rva=0x%x comparator_phase=%s sort_arg=%p vector=%p count=%d scanned=%d live_context=%p live_slot=%d live_depth=%d live_candidates=%d live_foreign=%d live_targets=%d sort_count=%d score_sort_count=%d sorted_foreign=%d sorted_targets=%d first_foreign_idx=%d first_target_idx=%d p5381_idx=%d p5417_idx=%d foreign_sort_bias=%d foreign_sort_bonus=%d foreign_sort_mixed_compares=%d foreign_sort_decisions=%d",
        slot,
        caller_rva,
        kbo_pointer_vector_sort_trace_phase(caller_rva),
        comparator_rva,
        kbo_pointer_vector_sort_comparator_phase(comparator_rva),
        (void*)sort_arg,
        (void*)vector_ptr,
        count,
        scan_count,
        (void*)live_trace->context_ptr,
        live_trace->slot_index,
        live_trace->depth_hint,
        live_trace->candidate_count,
        live_trace->foreign_count,
        live_trace->target_count,
        live_trace->sort_count,
        live_trace->score_sort_count,
        foreign_in_sorted,
        target_in_sorted,
        first_foreign.index,
        first_target.index,
        player5381.index,
        player5417.index,
        live_trace->foreign_sort_bias_bonus > 0 ? 1 : 0,
        live_trace->foreign_sort_bias_bonus,
        live_trace->foreign_sort_bias_mixed_compare_count,
        live_trace->foreign_sort_bias_decision_count);

    append_logf(
        "ootp pointer-vector sort top #%ld caller_rva=0x%x phase=%s comparator_phase=%s t1_idx=%d t1=%u t1_for=%u t1_tgt=%u t1_push_idx=%d t1_push_rva=0x%x t1_fe0=%d t1_fe4=%d t1_f25=%u t1_f62=%u t1_f65=%u t1_role=0x%x t1_s24=%u t1_s25=%u t1_ovr=%d t1_rat=%d t2_idx=%d t2=%u t2_for=%u t2_tgt=%u t2_push_idx=%d t2_push_rva=0x%x t2_fe0=%d t2_fe4=%d t2_f25=%u t2_f62=%u t2_f65=%u t2_role=0x%x t2_s24=%u t2_s25=%u t2_ovr=%d t2_rat=%d t3_idx=%d t3=%u t3_for=%u t3_tgt=%u t3_push_idx=%d t3_push_rva=0x%x t3_fe0=%d t3_fe4=%d t3_f25=%u t3_f62=%u t3_f65=%u t3_role=0x%x t3_s24=%u t3_s25=%u t3_ovr=%d t3_rat=%d t4_idx=%d t4=%u t4_for=%u t4_tgt=%u t4_push_idx=%d t4_push_rva=0x%x t4_fe0=%d t4_fe4=%d t4_f25=%u t4_f62=%u t4_f65=%u t4_role=0x%x t4_s24=%u t4_s25=%u t4_ovr=%d t4_rat=%d t5_idx=%d t5=%u t5_for=%u t5_tgt=%u t5_push_idx=%d t5_push_rva=0x%x t5_fe0=%d t5_fe4=%d t5_f25=%u t5_f62=%u t5_f65=%u t5_role=0x%x t5_s24=%u t5_s25=%u t5_ovr=%d t5_rat=%d t6_idx=%d t6=%u t6_for=%u t6_tgt=%u t6_push_idx=%d t6_push_rva=0x%x t6_fe0=%d t6_fe4=%d t6_f25=%u t6_f62=%u t6_f65=%u t6_role=0x%x t6_s24=%u t6_s25=%u t6_ovr=%d t6_rat=%d",
        slot,
        caller_rva,
        kbo_pointer_vector_sort_trace_phase(caller_rva),
        kbo_pointer_vector_sort_comparator_phase(comparator_rva),
        top[0].index, top[0].player_id, top[0].foreign, top[0].target, top[0].push_index, top[0].push_rva,
        top[0].score_fe0, top[0].score_fe4, top[0].f25, top[0].f62, top[0].f65, top[0].role_mask,
        top[0].status24, top[0].status25, top[0].overall, top[0].ratings,
        top[1].index, top[1].player_id, top[1].foreign, top[1].target, top[1].push_index, top[1].push_rva,
        top[1].score_fe0, top[1].score_fe4, top[1].f25, top[1].f62, top[1].f65, top[1].role_mask,
        top[1].status24, top[1].status25, top[1].overall, top[1].ratings,
        top[2].index, top[2].player_id, top[2].foreign, top[2].target, top[2].push_index, top[2].push_rva,
        top[2].score_fe0, top[2].score_fe4, top[2].f25, top[2].f62, top[2].f65, top[2].role_mask,
        top[2].status24, top[2].status25, top[2].overall, top[2].ratings,
        top[3].index, top[3].player_id, top[3].foreign, top[3].target, top[3].push_index, top[3].push_rva,
        top[3].score_fe0, top[3].score_fe4, top[3].f25, top[3].f62, top[3].f65, top[3].role_mask,
        top[3].status24, top[3].status25, top[3].overall, top[3].ratings,
        top[4].index, top[4].player_id, top[4].foreign, top[4].target, top[4].push_index, top[4].push_rva,
        top[4].score_fe0, top[4].score_fe4, top[4].f25, top[4].f62, top[4].f65, top[4].role_mask,
        top[4].status24, top[4].status25, top[4].overall, top[4].ratings,
        top[5].index, top[5].player_id, top[5].foreign, top[5].target, top[5].push_index, top[5].push_rva,
        top[5].score_fe0, top[5].score_fe4, top[5].f25, top[5].f62, top[5].f65, top[5].role_mask,
        top[5].status24, top[5].status25, top[5].overall, top[5].ratings);

    uint32_t f06_bonus_for_log = comparator_rva == KBO_AI_ROSTER_SELECT_COMPARATOR_917380_RVA
        ? (uint32_t)live_trace->foreign_sort_bias_bonus
        : 0u;
    uint32_t c98_bonus_for_log = comparator_rva == KBO_AI_ROSTER_SELECT_COMPARATOR_909FA0_RVA
        ? (uint32_t)live_trace->foreign_sort_bias_bonus
        : 0u;

    append_logf(
        "ootp pointer-vector sort keys #%ld caller_rva=0x%x phase=%s comparator_phase=%s foreign_sort_bias=%d foreign_sort_bonus=%d t1_idx=%d t1=%u t1_f06=%u t1_eff_f06=%u t1_bce=%u t2_idx=%d t2=%u t2_f06=%u t2_eff_f06=%u t2_bce=%u t3_idx=%d t3=%u t3_f06=%u t3_eff_f06=%u t3_bce=%u t4_idx=%d t4=%u t4_f06=%u t4_eff_f06=%u t4_bce=%u t5_idx=%d t5=%u t5_f06=%u t5_eff_f06=%u t5_bce=%u t6_idx=%d t6=%u t6_f06=%u t6_eff_f06=%u t6_bce=%u first_foreign_idx=%d first_foreign=%u first_foreign_f06=%u first_foreign_eff_f06=%u first_foreign_bce=%u first_target_idx=%d first_target=%u first_target_f06=%u first_target_eff_f06=%u first_target_bce=%u p5381_idx=%d p5381_f06=%u p5381_eff_f06=%u p5381_bce=%u p5417_idx=%d p5417_f06=%u p5417_eff_f06=%u p5417_bce=%u",
        slot,
        caller_rva,
        kbo_pointer_vector_sort_trace_phase(caller_rva),
        kbo_pointer_vector_sort_comparator_phase(comparator_rva),
        live_trace->foreign_sort_bias_bonus > 0 ? 1 : 0,
        live_trace->foreign_sort_bias_bonus,
        top[0].index, top[0].player_id, top[0].sort_f06,
        top[0].sort_f06 + (top[0].foreign ? f06_bonus_for_log : 0u),
        top[0].sort_bce,
        top[1].index, top[1].player_id, top[1].sort_f06,
        top[1].sort_f06 + (top[1].foreign ? f06_bonus_for_log : 0u),
        top[1].sort_bce,
        top[2].index, top[2].player_id, top[2].sort_f06,
        top[2].sort_f06 + (top[2].foreign ? f06_bonus_for_log : 0u),
        top[2].sort_bce,
        top[3].index, top[3].player_id, top[3].sort_f06,
        top[3].sort_f06 + (top[3].foreign ? f06_bonus_for_log : 0u),
        top[3].sort_bce,
        top[4].index, top[4].player_id, top[4].sort_f06,
        top[4].sort_f06 + (top[4].foreign ? f06_bonus_for_log : 0u),
        top[4].sort_bce,
        top[5].index, top[5].player_id, top[5].sort_f06,
        top[5].sort_f06 + (top[5].foreign ? f06_bonus_for_log : 0u),
        top[5].sort_bce,
        first_foreign.index,
        first_foreign.player_id,
        first_foreign.sort_f06,
        first_foreign.sort_f06 + (first_foreign.foreign ? f06_bonus_for_log : 0u),
        first_foreign.sort_bce,
        first_target.index,
        first_target.player_id,
        first_target.sort_f06,
        first_target.sort_f06 + (first_target.foreign ? f06_bonus_for_log : 0u),
        first_target.sort_bce,
        player5381.index,
        player5381.sort_f06,
        player5381.sort_f06 + (player5381.foreign ? f06_bonus_for_log : 0u),
        player5381.sort_bce,
        player5417.index,
        player5417.sort_f06,
        player5417.sort_f06 + (player5417.foreign ? f06_bonus_for_log : 0u),
        player5417.sort_bce);

    append_logf(
        "ootp pointer-vector sort type keys #%ld caller_rva=0x%x phase=%s comparator_phase=%s foreign_sort_bias=%d foreign_sort_bonus=%d t1_idx=%d t1=%u t1_c98=%u t1_eff_c98=%u t2_idx=%d t2=%u t2_c98=%u t2_eff_c98=%u t3_idx=%d t3=%u t3_c98=%u t3_eff_c98=%u t4_idx=%d t4=%u t4_c98=%u t4_eff_c98=%u t5_idx=%d t5=%u t5_c98=%u t5_eff_c98=%u t6_idx=%d t6=%u t6_c98=%u t6_eff_c98=%u first_foreign_idx=%d first_foreign=%u first_foreign_c98=%u first_foreign_eff_c98=%u first_target_idx=%d first_target=%u first_target_c98=%u first_target_eff_c98=%u p5381_idx=%d p5381_c98=%u p5381_eff_c98=%u p5417_idx=%d p5417_c98=%u p5417_eff_c98=%u",
        slot,
        caller_rva,
        kbo_pointer_vector_sort_trace_phase(caller_rva),
        kbo_pointer_vector_sort_comparator_phase(comparator_rva),
        live_trace->foreign_sort_bias_bonus > 0 ? 1 : 0,
        live_trace->foreign_sort_bias_bonus,
        top[0].index, top[0].player_id, top[0].sort_c98,
        top[0].sort_c98 + (top[0].foreign ? c98_bonus_for_log : 0u),
        top[1].index, top[1].player_id, top[1].sort_c98,
        top[1].sort_c98 + (top[1].foreign ? c98_bonus_for_log : 0u),
        top[2].index, top[2].player_id, top[2].sort_c98,
        top[2].sort_c98 + (top[2].foreign ? c98_bonus_for_log : 0u),
        top[3].index, top[3].player_id, top[3].sort_c98,
        top[3].sort_c98 + (top[3].foreign ? c98_bonus_for_log : 0u),
        top[4].index, top[4].player_id, top[4].sort_c98,
        top[4].sort_c98 + (top[4].foreign ? c98_bonus_for_log : 0u),
        top[5].index, top[5].player_id, top[5].sort_c98,
        top[5].sort_c98 + (top[5].foreign ? c98_bonus_for_log : 0u),
        first_foreign.index,
        first_foreign.player_id,
        first_foreign.sort_c98,
        first_foreign.sort_c98 + (first_foreign.foreign ? c98_bonus_for_log : 0u),
        first_target.index,
        first_target.player_id,
        first_target.sort_c98,
        first_target.sort_c98 + (first_target.foreign ? c98_bonus_for_log : 0u),
        player5381.index,
        player5381.sort_c98,
        player5381.sort_c98 + (player5381.foreign ? c98_bonus_for_log : 0u),
        player5417.index,
        player5417.sort_c98,
        player5417.sort_c98 + (player5417.foreign ? c98_bonus_for_log : 0u));

    append_logf(
        "ootp pointer-vector sort focus #%ld caller_rva=0x%x phase=%s comparator_phase=%s first_foreign_idx=%d first_foreign=%u first_foreign_push_idx=%d first_foreign_push_rva=0x%x first_foreign_fe0=%d first_foreign_fe4=%d first_foreign_f25=%u first_foreign_f62=%u first_foreign_f65=%u first_foreign_role=0x%x first_foreign_s24=%u first_foreign_s25=%u first_target_idx=%d first_target=%u first_target_push_idx=%d first_target_push_rva=0x%x first_target_fe0=%d first_target_fe4=%d first_target_f25=%u first_target_role=0x%x p5381_idx=%d p5381_push_idx=%d p5381_push_rva=0x%x p5381_fe0=%d p5381_fe4=%d p5381_f25=%u p5381_f62=%u p5381_f65=%u p5381_role=0x%x p5381_s24=%u p5381_s25=%u p5417_idx=%d p5417_push_idx=%d p5417_push_rva=0x%x p5417_fe0=%d p5417_fe4=%d p5417_f25=%u p5417_f62=%u p5417_f65=%u p5417_role=0x%x p5417_s24=%u p5417_s25=%u",
        slot,
        caller_rva,
        kbo_pointer_vector_sort_trace_phase(caller_rva),
        kbo_pointer_vector_sort_comparator_phase(comparator_rva),
        first_foreign.index,
        first_foreign.player_id,
        first_foreign.push_index,
        first_foreign.push_rva,
        first_foreign.score_fe0,
        first_foreign.score_fe4,
        first_foreign.f25,
        first_foreign.f62,
        first_foreign.f65,
        first_foreign.role_mask,
        first_foreign.status24,
        first_foreign.status25,
        first_target.index,
        first_target.player_id,
        first_target.push_index,
        first_target.push_rva,
        first_target.score_fe0,
        first_target.score_fe4,
        first_target.f25,
        first_target.role_mask,
        player5381.index,
        player5381.push_index,
        player5381.push_rva,
        player5381.score_fe0,
        player5381.score_fe4,
        player5381.f25,
        player5381.f62,
        player5381.f65,
        player5381.role_mask,
        player5381.status24,
        player5381.status25,
        player5417.index,
        player5417.push_index,
        player5417.push_rva,
        player5417.score_fe0,
        player5417.score_fe4,
        player5417.f25,
        player5417.f62,
        player5417.f65,
        player5417.role_mask,
        player5417.status24,
        player5417.status25);
}

__declspec(noinline) uintptr_t ootp_kbo_ai_roster_select_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    KboAiRosterSelectLiveTrace live_trace;
    kbo_ai_roster_select_live_trace_init(
        &live_trace,
        context_ptr,
        slot_index,
        depth_hint,
        caller_rva);
    KboAiRosterSelectLiveTrace* previous_live_trace = kbo_ai_roster_select_live_trace_get();
    OotpKboAiRosterSelectFn original = g_kbo_ai_roster_select_trace_trampoline;
    uintptr_t result_ptr = 0u;
    if (original != NULL) {
        kbo_ai_roster_select_live_trace_set(&live_trace);
        result_ptr = original(context_ptr, slot_index, depth_hint);
        kbo_ai_roster_select_live_trace_set(previous_live_trace);
    }
    live_trace.active = 0;
    int32_t selected_local_index = kbo_ai_roster_select_live_trace_selected_index(&live_trace, result_ptr);
    int32_t selected_last_sort_rank = kbo_ai_roster_select_live_trace_last_sorted_rank(
        &live_trace,
        selected_local_index);

    uintptr_t slot_block_ptr = 0u;
    uintptr_t source_vector_ptr = 0u;
    uintptr_t source_vector_data = 0u;
    uintptr_t context_selected_ptr = 0u;
    uintptr_t league_info_ptr = 0u;
    uintptr_t rules_ptr = 0u;
    uint32_t slot_team_id = 0u;
    uint32_t slot_team_league_id = 0u;
    uint32_t league_type = 0u;
    uint32_t primary_slot = 0u;
    uint32_t secondary_slot = 0u;
    uint32_t slot_block_flag = 0u;
    uint32_t marked_player_id = 0u;
    int32_t source_count = -1;
    int32_t scanned_count = 0;
    int32_t foreign_count = 0;
    int32_t target_count = 0;
    int32_t selected_source_index = -1;
    KboAiRosterForeignCandidateSummary foreign_summaries[KBO_AI_ROSTER_SELECT_FOREIGN_SUMMARY_SLOTS] = {0};
    for (int i = 0; i < KBO_AI_ROSTER_SELECT_FOREIGN_SUMMARY_SLOTS; i++) {
        foreign_summaries[i].index = -1;
    }

    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        uintptr_t slot_block_slot = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET
            + (uintptr_t)slot_index * sizeof(uintptr_t);
        uintptr_t slot_team_slot = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET
            + (uintptr_t)slot_index * sizeof(uint32_t);
        if (memory_range_readable((void*)slot_block_slot, sizeof(uintptr_t))) {
            slot_block_ptr = *(uintptr_t*)slot_block_slot;
        }
        if (memory_range_readable((void*)slot_team_slot, sizeof(uint32_t))) {
            slot_team_id = *(uint32_t*)slot_team_slot;
        }
    }
    if (context_ptr != 0u) {
        if (memory_range_readable((void*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET), sizeof(uintptr_t))) {
            context_selected_ptr = *(uintptr_t*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET);
        }
        if (memory_range_readable((void*)(context_ptr + KBO_AI_ROSTER_CONTEXT_LEAGUE_INFO_PTR_OFFSET), sizeof(uintptr_t))) {
            league_info_ptr = *(uintptr_t*)(context_ptr + KBO_AI_ROSTER_CONTEXT_LEAGUE_INFO_PTR_OFFSET);
        }
        if (memory_range_readable((void*)(context_ptr + KBO_AI_ROSTER_CONTEXT_RULES_PTR_OFFSET), sizeof(uintptr_t))) {
            rules_ptr = *(uintptr_t*)(context_ptr + KBO_AI_ROSTER_CONTEXT_RULES_PTR_OFFSET);
        }
        if (memory_range_readable((void*)(context_ptr + KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET), sizeof(uint16_t))) {
            primary_slot = *(uint16_t*)(context_ptr + KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET);
        }
        if (memory_range_readable((void*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SECONDARY_SLOT_OFFSET), sizeof(uint16_t))) {
            secondary_slot = *(uint16_t*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SECONDARY_SLOT_OFFSET);
        }
    }
    if (league_info_ptr != 0u && memory_range_readable((void*)(league_info_ptr + 0x26u), sizeof(uint16_t))) {
        league_type = *(uint16_t*)(league_info_ptr + 0x26u);
    }
    if (slot_team_id != 0u) {
        uint8_t* slot_team = find_kbo_team_by_numeric_id_any_league(slot_team_id, 1);
        if (slot_team != NULL && memory_range_readable(slot_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET, sizeof(uint32_t))) {
            slot_team_league_id = *(uint32_t*)(slot_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
        }
    }
    if (slot_block_ptr != 0u) {
        if (memory_range_readable((void*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_FLAG_OFFSET), sizeof(uint8_t))) {
            slot_block_flag = *(uint8_t*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_FLAG_OFFSET);
        }
        if (memory_range_readable((void*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID_OFFSET), sizeof(uint32_t))) {
            marked_player_id = *(uint32_t*)(slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_MARKED_PLAYER_ID_OFFSET);
        }
        source_vector_ptr = slot_block_ptr + KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET;
        source_count = kbo_pointer_vector_count(source_vector_ptr);
        if (source_count > 0 && memory_range_readable((void*)source_vector_ptr, sizeof(uintptr_t))) {
            source_vector_data = *(uintptr_t*)source_vector_ptr;
        }
        scanned_count = source_count > KBO_AI_ROSTER_SELECT_SCAN_LIMIT
            ? KBO_AI_ROSTER_SELECT_SCAN_LIMIT
            : source_count;
        if (scanned_count < 0) {
            scanned_count = 0;
        }
        for (int32_t i = 0; i < scanned_count; i++) {
            uintptr_t candidate_ptr = kbo_pointer_vector_value_at(source_vector_ptr, i, source_count);
            if (candidate_ptr == result_ptr && selected_source_index < 0) {
                selected_source_index = i;
            }
            if (!kbo_player_pointer_plausible(candidate_ptr)
                    || !memory_range_readable((void*)candidate_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
                continue;
            }
            uint8_t* candidate = (uint8_t*)candidate_ptr;
            uint32_t candidate_id = *(uint32_t*)(candidate + OOTP27_PLAYER_ID_OFFSET);
            int target_candidate = candidate_id == 5320u || candidate_id == 5381u || candidate_id == 5404u
                || candidate_id == 5368u || candidate_id == 5293u || candidate_id == 5417u;
            if (target_candidate) {
                target_count++;
            }
            if (kbo_player_is_foreign_for_kbo_rights(candidate)) {
                if (foreign_count < KBO_AI_ROSTER_SELECT_FOREIGN_SUMMARY_SLOTS) {
                    kbo_fill_ai_roster_foreign_candidate_summary(&foreign_summaries[foreign_count], i, candidate);
                }
                foreign_count++;
            }
        }
    }

    uintptr_t native_result_ptr = result_ptr;
    int32_t rescue_source_index = -1;
    uint32_t rescue_active_team_id = 0u;
    int64_t rescue_score = 0;
    KboAiRosterForeignCandidateSummary rescue_summary;
    uintptr_t rescue_ptr = kbo_ai_roster_choose_source_select_rescue_candidate(
        source_vector_ptr,
        source_count,
        native_result_ptr,
        &rescue_source_index,
        &rescue_active_team_id,
        &rescue_score,
        &rescue_summary);
    if (rescue_ptr != 0u && rescue_ptr != native_result_ptr) {
        uint32_t native_player_id = 0u;
        int native_foreign = 0;
        int32_t native_score_fe0 = 0;
        int32_t native_score_fe4 = 0;
        int16_t native_overall = 0;
        int16_t native_ratings = 0;
        if (kbo_player_pointer_plausible(native_result_ptr)
                && memory_range_readable((void*)native_result_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
            uint8_t* native_player = (uint8_t*)native_result_ptr;
            native_player_id = *(uint32_t*)(native_player + OOTP27_PLAYER_ID_OFFSET);
            native_foreign = kbo_player_is_foreign_for_kbo_rights(native_player);
            native_score_fe0 = kbo_read_ai_roster_select_score(native_result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
            native_score_fe4 = kbo_read_ai_roster_select_score(native_result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
            native_overall = kbo_read_player_i16(native_player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
            native_ratings = kbo_read_player_i16(native_player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
        }

        static volatile LONG rescue_log_count = 0;
        LONG rescue_slot = InterlockedIncrement(&rescue_log_count);
        append_logf(
            "ootp ai roster foreign source-select rescue #%ld caller_rva=0x%x context=%p slot_index=%d depth_hint=%d source_count=%d native=%u native_foreign=%d native_fe0=%d native_fe4=%d native_overall=%d native_ratings=%d override=%u source_idx=%d active_team=%u score=%lld current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f25=%u f62=%u f65=%u f06=%d fe0=%d fe4=%d overall=%d talent=%d ratings=%d local_candidates=%d local_foreign=%d local_targets=%d",
            rescue_slot,
            caller_rva,
            (void*)context_ptr,
            slot_index,
            depth_hint,
            source_count,
            native_player_id,
            native_foreign,
            native_score_fe0,
            native_score_fe4,
            native_overall,
            native_ratings,
            rescue_summary.player_id,
            rescue_source_index,
            rescue_active_team_id,
            (long long)rescue_score,
            rescue_summary.current_team_id,
            rescue_summary.active_team_id,
            rescue_summary.league_id,
            rescue_summary.default_team_id,
            rescue_summary.status24,
            rescue_summary.status25,
            rescue_summary.status26,
            rescue_summary.f25,
            (uint32_t)((uint8_t*)rescue_ptr)[0xf62u],
            (uint32_t)((uint8_t*)rescue_ptr)[0xf65u],
            kbo_read_player_i16((uint8_t*)rescue_ptr, 0xf06u),
            rescue_summary.score_fe0,
            rescue_summary.score_fe4,
            rescue_summary.overall,
            rescue_summary.talent,
            rescue_summary.ratings,
            live_trace.candidate_count,
            live_trace.foreign_count,
            live_trace.target_count);
        result_ptr = rescue_ptr;
        selected_source_index = rescue_source_index;
    }
    selected_local_index = kbo_ai_roster_select_live_trace_selected_index(&live_trace, result_ptr);
    selected_last_sort_rank = kbo_ai_roster_select_live_trace_last_sorted_rank(
        &live_trace,
        selected_local_index);

    uint8_t* player = NULL;
    uint32_t player_id = 0u;
    uint32_t nation_id = 0u;
    uint32_t current_team_id = 0u;
    uint32_t active_team_id = 0u;
    uint32_t league_id = 0u;
    uint32_t default_team_id = 0u;
    int result_plausible = kbo_player_pointer_plausible(result_ptr)
        && memory_range_readable((void*)result_ptr, OOTP27_PLAYER_SCAN_BYTES);
    if (result_plausible) {
        player = (uint8_t*)result_ptr;
        player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
        current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
        }
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    int league_matches = kbo_league_id == 0u
        || slot_team_league_id == kbo_league_id
        || slot_team_league_id == kbo_league_id + 1u
        || league_id == kbo_league_id
        || league_id == kbo_league_id + 1u;
    int selected_foreign = result_plausible && kbo_player_is_foreign_for_kbo_rights(player);
    int target_player = player_id == 5320u || player_id == 5381u || player_id == 5404u
        || player_id == 5368u || player_id == 5293u || player_id == 5417u;
    if (!league_matches && !selected_foreign && !target_player && foreign_count == 0 && target_count == 0) {
        return result_ptr;
    }

    static volatile LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 3000 && !target_player) {
        if (slot == 3001) {
            append_log_line("ootp ai roster select trace suppressed after 3000 entries");
        }
        return result_ptr;
    }

    uintptr_t status_ptr = 0u;
    uint32_t status8 = 0u;
    uint32_t status21 = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    uint32_t status27 = 0u;
    uint32_t status28 = 0u;
    if (result_plausible) {
        if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
            status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
        }
        if (status_ptr == 0u && g_kbo_player_team_status_by_id_lookup_fn != NULL) {
            status_ptr = g_kbo_player_team_status_by_id_lookup_fn(result_ptr, (int32_t)default_team_id);
        }
        if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
            uint8_t* status = (uint8_t*)status_ptr;
            status8 = status[0x08u];
            status21 = status[0x21u];
            status24 = status[0x24u];
            status25 = status[0x25u];
            status26 = status[0x26u];
            status27 = status[0x27u];
            status28 = status[0x28u];
        }
    }
    int32_t selected_score_fe0 = result_plausible
        ? kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET)
        : 0;
    int32_t selected_score_fe4 = result_plausible
        ? kbo_read_ai_roster_select_score(result_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET)
        : 0;

    append_logf(
        "ootp ai roster select trace #%ld caller_rva=0x%x context=%p slot_index=%d depth_hint=%d slot_block=%p slot_team=%u slot_team_league=%u league_type=%u primary_slot=%u secondary_slot=%u context_selected=%p rules=%p slot_flag=%u marked_player=%u source_vec=%p source_data=%p source_count=%d scanned=%d source_foreign=%d source_targets=%d selected_source_index=%d local_first_vec=%p local_last_vec=%p local_switches=%d first_push_rva=0x%x first_push_phase=%s first_push_order=%d local_pushes=%d local_inserted=%d local_candidates=%d local_foreign=%d local_targets=%d selected_local_index=%d selected_last_sort_rank=%d sort_count=%d score_sort_count=%d last_sort_caller=0x%x last_sort_phase=%s last_sort_cmp=0x%x last_sort_cmp_phase=%s last_sort_count=%d last_sort_foreign=%d last_sort_targets=%d result_ptr=%p selected=%u selected_foreign=%d nation=%u current=%u active=%u league=%u default_team=%u status_ptr=%p status8=%u status21=%u status24=%u status25=%u status26=%u status27=%u status28=%u c78=%u c79=%u f25=%u f62=%u f65=%u f6a=%u value_ac=%u overall=%d talent=%d ratings=%d score_fe0=%d score_fe4=%d",
        slot,
        caller_rva,
        (void*)context_ptr,
        slot_index,
        depth_hint,
        (void*)slot_block_ptr,
        slot_team_id,
        slot_team_league_id,
        league_type,
        primary_slot,
        secondary_slot,
        (void*)context_selected_ptr,
        (void*)rules_ptr,
        slot_block_flag,
        marked_player_id,
        (void*)source_vector_ptr,
        (void*)source_vector_data,
        source_count,
        scanned_count,
        foreign_count,
        target_count,
        selected_source_index,
        (void*)live_trace.first_vector_ptr,
        (void*)live_trace.last_vector_ptr,
        live_trace.vector_switches,
        live_trace.first_push_rva,
        kbo_pointer_vector_push_trace_phase(live_trace.first_push_rva),
        kbo_ai_roster_push_phase_order(live_trace.first_push_rva),
        live_trace.push_count,
        live_trace.inserted_count,
        live_trace.candidate_count,
        live_trace.foreign_count,
        live_trace.target_count,
        selected_local_index,
        selected_last_sort_rank,
        live_trace.sort_count,
        live_trace.score_sort_count,
        live_trace.last_sort_caller_rva,
        kbo_pointer_vector_sort_trace_phase(live_trace.last_sort_caller_rva),
        live_trace.last_sort_comparator_rva,
        kbo_pointer_vector_sort_comparator_phase(live_trace.last_sort_comparator_rva),
        live_trace.last_sort_count,
        live_trace.last_sort_foreign_count,
        live_trace.last_sort_target_count,
        (void*)result_ptr,
        player_id,
        selected_foreign,
        nation_id,
        current_team_id,
        active_team_id,
        league_id,
        default_team_id,
        (void*)status_ptr,
        status8,
        status21,
        status24,
        status25,
        status26,
        status27,
        status28,
        result_plausible ? (uint32_t)player[0xc78u] : 0u,
        result_plausible ? (uint32_t)player[0xc79u] : 0u,
        result_plausible ? (uint32_t)player[0xf25u] : 0u,
        result_plausible ? (uint32_t)player[0xf62u] : 0u,
        result_plausible ? (uint32_t)player[0xf65u] : 0u,
        result_plausible ? (uint32_t)player[0xf6au] : 0u,
        result_plausible ? (uint32_t)player[0xacu] : 0u,
        result_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET) : 0,
        result_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET) : 0,
        result_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET) : 0,
        selected_score_fe0,
        selected_score_fe4);
    if (foreign_count > 0 || target_count > 0) {
        append_logf(
            "ootp ai roster select source foreign summary #%ld caller_rva=0x%x slot_index=%d source_count=%d source_foreign=%d source_targets=%d selected_source_index=%d f1_idx=%d f1=%u f1_nat=%u f1_cur=%u f1_act=%u f1_lg=%u f1_def=%u f1_s24=%u f1_s25=%u f1_s26=%u f1_c78=%u f1_c79=%u f1_f25=%u f1_ac=%u f1_ovr=%d f1_tal=%d f1_rat=%d f2_idx=%d f2=%u f2_nat=%u f2_cur=%u f2_act=%u f2_lg=%u f2_def=%u f2_s24=%u f2_s25=%u f2_s26=%u f2_c78=%u f2_c79=%u f2_f25=%u f2_ac=%u f2_ovr=%d f2_tal=%d f2_rat=%d f3_idx=%d f3=%u f3_nat=%u f3_cur=%u f3_act=%u f3_lg=%u f3_def=%u f3_s24=%u f3_s25=%u f3_s26=%u f3_c78=%u f3_c79=%u f3_f25=%u f3_ac=%u f3_ovr=%d f3_tal=%d f3_rat=%d",
            slot,
            caller_rva,
            slot_index,
            source_count,
            foreign_count,
            target_count,
            selected_source_index,
            foreign_summaries[0].index,
            foreign_summaries[0].player_id,
            foreign_summaries[0].nation_id,
            foreign_summaries[0].current_team_id,
            foreign_summaries[0].active_team_id,
            foreign_summaries[0].league_id,
            foreign_summaries[0].default_team_id,
            foreign_summaries[0].status24,
            foreign_summaries[0].status25,
            foreign_summaries[0].status26,
            foreign_summaries[0].c78,
            foreign_summaries[0].c79,
            foreign_summaries[0].f25,
            foreign_summaries[0].value_ac,
            foreign_summaries[0].overall,
            foreign_summaries[0].talent,
            foreign_summaries[0].ratings,
            foreign_summaries[1].index,
            foreign_summaries[1].player_id,
            foreign_summaries[1].nation_id,
            foreign_summaries[1].current_team_id,
            foreign_summaries[1].active_team_id,
            foreign_summaries[1].league_id,
            foreign_summaries[1].default_team_id,
            foreign_summaries[1].status24,
            foreign_summaries[1].status25,
            foreign_summaries[1].status26,
            foreign_summaries[1].c78,
            foreign_summaries[1].c79,
            foreign_summaries[1].f25,
            foreign_summaries[1].value_ac,
            foreign_summaries[1].overall,
            foreign_summaries[1].talent,
            foreign_summaries[1].ratings,
            foreign_summaries[2].index,
            foreign_summaries[2].player_id,
            foreign_summaries[2].nation_id,
            foreign_summaries[2].current_team_id,
            foreign_summaries[2].active_team_id,
            foreign_summaries[2].league_id,
            foreign_summaries[2].default_team_id,
            foreign_summaries[2].status24,
            foreign_summaries[2].status25,
            foreign_summaries[2].status26,
            foreign_summaries[2].c78,
            foreign_summaries[2].c79,
            foreign_summaries[2].f25,
            foreign_summaries[2].value_ac,
            foreign_summaries[2].overall,
            foreign_summaries[2].talent,
            foreign_summaries[2].ratings);
    }
    if (live_trace.foreign_count > 0 || live_trace.target_count > 0) {
        append_logf(
            "ootp ai roster select local foreign summary #%ld caller_rva=0x%x slot_index=%d local_candidates=%d local_foreign=%d local_targets=%d selected_local_index=%d local_first_vec=%p local_last_vec=%p f1_idx=%d f1=%u f1_nat=%u f1_cur=%u f1_act=%u f1_lg=%u f1_def=%u f1_s24=%u f1_s25=%u f1_s26=%u f1_c78=%u f1_c79=%u f1_f25=%u f1_ac=%u f1_ovr=%d f1_tal=%d f1_rat=%d f2_idx=%d f2=%u f2_nat=%u f2_cur=%u f2_act=%u f2_lg=%u f2_def=%u f2_s24=%u f2_s25=%u f2_s26=%u f2_c78=%u f2_c79=%u f2_f25=%u f2_ac=%u f2_ovr=%d f2_tal=%d f2_rat=%d f3_idx=%d f3=%u f3_nat=%u f3_cur=%u f3_act=%u f3_lg=%u f3_def=%u f3_s24=%u f3_s25=%u f3_s26=%u f3_c78=%u f3_c79=%u f3_f25=%u f3_ac=%u f3_ovr=%d f3_tal=%d f3_rat=%d",
            slot,
            caller_rva,
            slot_index,
            live_trace.candidate_count,
            live_trace.foreign_count,
            live_trace.target_count,
            selected_local_index,
            (void*)live_trace.first_vector_ptr,
            (void*)live_trace.last_vector_ptr,
            live_trace.foreign_summaries[0].index,
            live_trace.foreign_summaries[0].player_id,
            live_trace.foreign_summaries[0].nation_id,
            live_trace.foreign_summaries[0].current_team_id,
            live_trace.foreign_summaries[0].active_team_id,
            live_trace.foreign_summaries[0].league_id,
            live_trace.foreign_summaries[0].default_team_id,
            live_trace.foreign_summaries[0].status24,
            live_trace.foreign_summaries[0].status25,
            live_trace.foreign_summaries[0].status26,
            live_trace.foreign_summaries[0].c78,
            live_trace.foreign_summaries[0].c79,
            live_trace.foreign_summaries[0].f25,
            live_trace.foreign_summaries[0].value_ac,
            live_trace.foreign_summaries[0].overall,
            live_trace.foreign_summaries[0].talent,
            live_trace.foreign_summaries[0].ratings,
            live_trace.foreign_summaries[1].index,
            live_trace.foreign_summaries[1].player_id,
            live_trace.foreign_summaries[1].nation_id,
            live_trace.foreign_summaries[1].current_team_id,
            live_trace.foreign_summaries[1].active_team_id,
            live_trace.foreign_summaries[1].league_id,
            live_trace.foreign_summaries[1].default_team_id,
            live_trace.foreign_summaries[1].status24,
            live_trace.foreign_summaries[1].status25,
            live_trace.foreign_summaries[1].status26,
            live_trace.foreign_summaries[1].c78,
            live_trace.foreign_summaries[1].c79,
            live_trace.foreign_summaries[1].f25,
            live_trace.foreign_summaries[1].value_ac,
            live_trace.foreign_summaries[1].overall,
            live_trace.foreign_summaries[1].talent,
            live_trace.foreign_summaries[1].ratings,
            live_trace.foreign_summaries[2].index,
            live_trace.foreign_summaries[2].player_id,
            live_trace.foreign_summaries[2].nation_id,
            live_trace.foreign_summaries[2].current_team_id,
            live_trace.foreign_summaries[2].active_team_id,
            live_trace.foreign_summaries[2].league_id,
            live_trace.foreign_summaries[2].default_team_id,
            live_trace.foreign_summaries[2].status24,
            live_trace.foreign_summaries[2].status25,
            live_trace.foreign_summaries[2].status26,
            live_trace.foreign_summaries[2].c78,
            live_trace.foreign_summaries[2].c79,
            live_trace.foreign_summaries[2].f25,
            live_trace.foreign_summaries[2].value_ac,
            live_trace.foreign_summaries[2].overall,
            live_trace.foreign_summaries[2].talent,
            live_trace.foreign_summaries[2].ratings);
        kbo_log_ai_roster_local_candidate_detail(
            slot,
            caller_rva,
            slot_index,
            &live_trace,
            result_ptr);
    }
    if (foreign_count > 0 || target_count > 0 || live_trace.foreign_count > 0 || live_trace.target_count > 0) {
        const KboAiRosterForeignCandidateSummary* source_f1 = &foreign_summaries[0];
        const KboAiRosterForeignCandidateSummary* local_f1 = &live_trace.foreign_summaries[0];
        int32_t source_f1_score_fe0 =
            kbo_read_ai_roster_select_score(source_f1->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
        int32_t source_f1_score_fe4 =
            kbo_read_ai_roster_select_score(source_f1->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
        int32_t local_f1_score_fe0 =
            kbo_read_ai_roster_select_score(local_f1->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
        int32_t local_f1_score_fe4 =
            kbo_read_ai_roster_select_score(local_f1->player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
        append_logf(
            "ootp ai roster select score probe #%ld caller_rva=0x%x slot_index=%d source_count=%d source_foreign=%d source_targets=%d local_candidates=%d local_foreign=%d local_targets=%d selected_source_index=%d selected_local_index=%d selected=%u selected_foreign=%d selected_fe0=%d selected_fe4=%d selected_f25=%u selected_ovr=%d selected_rat=%d source_f1_idx=%d source_f1=%u source_f1_fe0=%d source_f1_fe4=%d source_f1_f25=%u source_f1_ovr=%d source_f1_rat=%d local_f1_idx=%d local_f1=%u local_f1_fe0=%d local_f1_fe4=%d local_f1_f25=%u local_f1_ovr=%d local_f1_rat=%d",
            slot,
            caller_rva,
            slot_index,
            source_count,
            foreign_count,
            target_count,
            live_trace.candidate_count,
            live_trace.foreign_count,
            live_trace.target_count,
            selected_source_index,
            selected_local_index,
            player_id,
            selected_foreign,
            selected_score_fe0,
            selected_score_fe4,
            result_plausible ? (uint32_t)player[0xf25u] : 0u,
            result_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET) : 0,
            result_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET) : 0,
            source_f1->index,
            source_f1->player_id,
            source_f1_score_fe0,
            source_f1_score_fe4,
            source_f1->f25,
            source_f1->overall,
            source_f1->ratings,
            local_f1->index,
            local_f1->player_id,
            local_f1_score_fe0,
            local_f1_score_fe4,
            local_f1->f25,
            local_f1->overall,
            local_f1->ratings);
        kbo_log_ai_roster_source_local_gate_trace(
            slot,
            caller_rva,
            slot_index,
            depth_hint,
            source_vector_ptr,
            source_count,
            &live_trace);
    }
    return result_ptr;
}

static int32_t kbo_ai_roster_slot_index_for_player(
    uintptr_t slot_block_ptr,
    uint32_t player_id,
    uint32_t* out_code)
{
    if (out_code != NULL) {
        *out_code = 0u;
    }
    if (slot_block_ptr == 0u || player_id == 0u) {
        return -1;
    }

    for (int32_t i = 0; i <= 8; i++) {
        uintptr_t slot_player_addr = slot_block_ptr + 0x14d4u + (uintptr_t)i * 8u;
        uintptr_t slot_code_addr = slot_block_ptr + 0x14d8u + (uintptr_t)i * 8u;
        if (!memory_range_readable((void*)slot_player_addr, sizeof(uint32_t))) {
            continue;
        }
        if (*(uint32_t*)slot_player_addr != player_id) {
            continue;
        }
        if (out_code != NULL && memory_range_readable((void*)slot_code_addr, sizeof(uint8_t))) {
            *out_code = (uint32_t)*(uint8_t*)slot_code_addr;
        }
        return i;
    }

    return -1;
}

static int32_t kbo_ai_roster_first_empty_slot(uintptr_t slot_block_ptr)
{
    if (slot_block_ptr == 0u) {
        return -1;
    }

    for (int32_t i = 0; i <= 8; i++) {
        uintptr_t slot_player_addr = slot_block_ptr + 0x14d4u + (uintptr_t)i * 8u;
        uintptr_t slot_code_addr = slot_block_ptr + 0x14d8u + (uintptr_t)i * 8u;
        if (!memory_range_readable((void*)slot_player_addr, sizeof(uint32_t))
                || !memory_range_readable((void*)slot_code_addr, sizeof(uint8_t))) {
            continue;
        }
        if (*(uint32_t*)slot_player_addr == 0u && *(uint8_t*)slot_code_addr == 0u) {
            return i;
        }
    }

    return -1;
}

static uint32_t kbo_ai_roster_slot_code_at(uintptr_t slot_block_ptr, uint16_t target_slot)
{
    if (slot_block_ptr == 0u || target_slot > 8u) {
        return 0u;
    }

    uintptr_t slot_code_addr = slot_block_ptr + 0x14d8u + (uintptr_t)target_slot * 8u;
    if (!memory_range_readable((void*)slot_code_addr, sizeof(uint8_t))) {
        return 0u;
    }
    return (uint32_t)*(uint8_t*)slot_code_addr;
}

static uint32_t kbo_ai_roster_slot_player_at(uintptr_t slot_block_ptr, uint16_t target_slot)
{
    if (slot_block_ptr == 0u || target_slot > 8u) {
        return 0u;
    }

    uintptr_t slot_player_addr = slot_block_ptr + 0x14d4u + (uintptr_t)target_slot * 8u;
    if (!memory_range_readable((void*)slot_player_addr, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)slot_player_addr;
}

typedef struct KboAiRosterFlowPlayerSnapshot {
    uintptr_t ptr;
    int plausible;
    uint32_t player_id;
    int foreign;
    int target;
    uint32_t nation_id;
    uint32_t current_team_id;
    uint32_t active_team_id;
    uint32_t league_id;
    uint32_t default_team_id;
    uint32_t status24;
    uint32_t status25;
    uint32_t status26;
    uint32_t f61;
    uint32_t f62;
    uint32_t f65;
    uint32_t f68;
    uint32_t f1a;
    uint32_t f3e;
    int16_t f06;
    uint32_t fec;
    int16_t ef8;
    int32_t score_fe0;
    int32_t score_fe4;
    int16_t overall;
    int16_t ratings;
} KboAiRosterFlowPlayerSnapshot;

typedef struct KboAiRosterFlowContextSnapshot {
    uint16_t primary_slot;
    uint16_t secondary_slot;
    uint16_t active_slot;
    uint16_t primary_target_slot;
    uint16_t primary_expected_slot;
    uint16_t secondary_target_slot;
    uint16_t secondary_expected_slot;
    uint8_t flag4c0;
    uint8_t flag4c1;
    uint8_t flag4c2;
    uint8_t flag4c4;
    uint8_t flag7ec;
    uint8_t flag86c;
    uint8_t flag86d;
    uint8_t primary_flag4c7;
    uint8_t primary_flag4cd;
    uint8_t secondary_flag4c9;
    uint8_t secondary_flag4cb;
    int32_t state8d0;
    int32_t state8d8;
    uintptr_t primary_slot_block;
    uintptr_t secondary_slot_block;
    int32_t primary_first_empty;
    int32_t secondary_first_empty;
    KboAiRosterFlowPlayerSnapshot selected;
    KboAiRosterFlowPlayerSnapshot ptr528;
    KboAiRosterFlowPlayerSnapshot ptr530;
    KboAiRosterFlowPlayerSnapshot ptr538;
    KboAiRosterFlowPlayerSnapshot ptr540;
    KboAiRosterFlowPlayerSnapshot ptr548;
} KboAiRosterFlowContextSnapshot;

static uint8_t kbo_ai_roster_context_u8(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(uint8_t))) {
        return 0u;
    }
    return *(uint8_t*)(context_ptr + offset);
}

static uint16_t kbo_ai_roster_context_u16(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(context_ptr + offset);
}

static int32_t kbo_ai_roster_context_i32(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(context_ptr + offset);
}

static uintptr_t kbo_ai_roster_context_ptr(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(uintptr_t))) {
        return 0u;
    }
    return *(uintptr_t*)(context_ptr + offset);
}

static uint8_t kbo_ai_roster_context_slot_u8(
    uintptr_t context_ptr,
    uintptr_t base_offset,
    uint16_t slot)
{
    if (slot >= 64u) {
        return 0u;
    }
    return kbo_ai_roster_context_u8(context_ptr, base_offset + (uintptr_t)slot);
}

static uint16_t kbo_ai_roster_context_slot_u16(
    uintptr_t context_ptr,
    uintptr_t base_offset,
    uint16_t slot)
{
    if (slot >= 64u) {
        return 0u;
    }
    return kbo_ai_roster_context_u16(context_ptr, base_offset + (uintptr_t)slot * sizeof(uint16_t));
}

static uintptr_t kbo_ai_roster_context_slot_block(uintptr_t context_ptr, uint16_t slot)
{
    if (slot >= 64u) {
        return 0u;
    }
    return kbo_ai_roster_context_ptr(
        context_ptr,
        KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET + (uintptr_t)slot * sizeof(uintptr_t));
}

static uint32_t kbo_ai_roster_context_slot_team_id(uintptr_t context_ptr, uint16_t slot)
{
    if (slot >= 64u) {
        return 0u;
    }
    uintptr_t addr = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET
        + (uintptr_t)slot * sizeof(uint32_t);
    if (context_ptr == 0u || !memory_range_readable((void*)addr, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)addr;
}

static void kbo_ai_roster_flow_read_player(
    uintptr_t player_ptr,
    KboAiRosterFlowPlayerSnapshot* out)
{
    memset(out, 0, sizeof(*out));
    out->ptr = player_ptr;

    int plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    if (!plausible) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    out->plausible = 1;
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->foreign = kbo_player_is_foreign_for_kbo_rights(player);
    out->target = kbo_ai_roster_research_target_player_id(out->player_id);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        out->default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    out->f61 = (uint32_t)player[0xf61u];
    out->f62 = (uint32_t)player[0xf62u];
    out->f65 = (uint32_t)player[0xf65u];
    out->f68 = (uint32_t)player[0xf68u];
    out->f1a = (uint32_t)player[0xf1au];
    out->f3e = (uint32_t)player[0xf3eu];
    out->f06 = kbo_read_player_i16(player, 0xf06u);
    if (memory_range_readable(player + 0xfecu, sizeof(uint32_t))) {
        out->fec = *(uint32_t*)(player + 0xfecu);
    }
    out->ef8 = kbo_read_player_i16(player, 0xef8u);
    out->score_fe0 = kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    out->score_fe4 = kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    out->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    out->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);

    uintptr_t status_ptr = 0u;
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        out->status24 = status[0x24u];
        out->status25 = status[0x25u];
        out->status26 = status[0x26u];
    }
}

static int kbo_ai_roster_flow_player_focus(const KboAiRosterFlowPlayerSnapshot* player)
{
    return player != NULL && player->plausible && (player->foreign || player->target);
}

static int kbo_ai_roster_flow_player_changed_focus(
    const KboAiRosterFlowPlayerSnapshot* before,
    const KboAiRosterFlowPlayerSnapshot* after)
{
    if (!kbo_ai_roster_flow_player_focus(before) && !kbo_ai_roster_flow_player_focus(after)) {
        return 0;
    }

    return before->ptr != after->ptr
        || before->player_id != after->player_id
        || before->current_team_id != after->current_team_id
        || before->active_team_id != after->active_team_id
        || before->status24 != after->status24
        || before->status25 != after->status25
        || before->status26 != after->status26
        || before->f62 != after->f62
        || before->f65 != after->f65
        || before->f68 != after->f68
        || before->f1a != after->f1a
        || before->f3e != after->f3e
        || before->ef8 != after->ef8
        || before->score_fe0 != after->score_fe0
        || before->score_fe4 != after->score_fe4;
}

static void kbo_ai_roster_flow_read_context(
    uintptr_t context_ptr,
    KboAiRosterFlowContextSnapshot* out)
{
    memset(out, 0, sizeof(*out));

    out->primary_slot = kbo_ai_roster_context_u16(context_ptr, KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET);
    out->secondary_slot = kbo_ai_roster_context_u16(context_ptr, KBO_AI_ROSTER_CONTEXT_SECONDARY_SLOT_OFFSET);
    out->active_slot = kbo_ai_roster_context_u16(context_ptr, 0x4a8u);
    out->primary_target_slot = kbo_ai_roster_context_slot_u16(context_ptr, 0x7bau, out->primary_slot);
    out->primary_expected_slot = kbo_ai_roster_context_slot_u16(context_ptr, 0x7b4u, out->primary_slot);
    out->secondary_target_slot = kbo_ai_roster_context_slot_u16(context_ptr, 0x7bau, out->secondary_slot);
    out->secondary_expected_slot = kbo_ai_roster_context_slot_u16(context_ptr, 0x7b4u, out->secondary_slot);
    out->flag4c0 = kbo_ai_roster_context_u8(context_ptr, 0x4c0u);
    out->flag4c1 = kbo_ai_roster_context_u8(context_ptr, 0x4c1u);
    out->flag4c2 = kbo_ai_roster_context_u8(context_ptr, 0x4c2u);
    out->flag4c4 = kbo_ai_roster_context_u8(context_ptr, 0x4c4u);
    out->flag7ec = kbo_ai_roster_context_u8(context_ptr, 0x7ecu);
    out->flag86c = kbo_ai_roster_context_u8(context_ptr, 0x86cu);
    out->flag86d = kbo_ai_roster_context_u8(context_ptr, 0x86du);
    out->primary_flag4c7 = kbo_ai_roster_context_slot_u8(context_ptr, 0x4c7u, out->primary_slot);
    out->primary_flag4cd = kbo_ai_roster_context_slot_u8(context_ptr, 0x4cdu, out->primary_slot);
    out->secondary_flag4c9 = kbo_ai_roster_context_slot_u8(context_ptr, 0x4c9u, out->secondary_slot);
    out->secondary_flag4cb = kbo_ai_roster_context_slot_u8(context_ptr, 0x4cbu, out->secondary_slot);
    out->state8d0 = kbo_ai_roster_context_i32(context_ptr, 0x8d0u);
    out->state8d8 = kbo_ai_roster_context_i32(context_ptr, 0x8d8u);
    out->primary_slot_block = kbo_ai_roster_context_slot_block(context_ptr, out->primary_slot);
    out->secondary_slot_block = kbo_ai_roster_context_slot_block(context_ptr, out->secondary_slot);
    out->primary_first_empty = kbo_ai_roster_first_empty_slot(out->primary_slot_block);
    out->secondary_first_empty = kbo_ai_roster_first_empty_slot(out->secondary_slot_block);

    kbo_ai_roster_flow_read_player(
        kbo_ai_roster_context_ptr(context_ptr, KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET),
        &out->selected);
    kbo_ai_roster_flow_read_player(kbo_ai_roster_context_ptr(context_ptr, 0x528u), &out->ptr528);
    kbo_ai_roster_flow_read_player(kbo_ai_roster_context_ptr(context_ptr, 0x530u), &out->ptr530);
    kbo_ai_roster_flow_read_player(kbo_ai_roster_context_ptr(context_ptr, 0x538u), &out->ptr538);
    kbo_ai_roster_flow_read_player(kbo_ai_roster_context_ptr(context_ptr, 0x540u), &out->ptr540);
    kbo_ai_roster_flow_read_player(kbo_ai_roster_context_ptr(context_ptr, 0x548u), &out->ptr548);
}

static int kbo_ai_roster_flow_context_focus(
    const KboAiRosterFlowContextSnapshot* before,
    const KboAiRosterFlowContextSnapshot* after)
{
    return kbo_ai_roster_flow_player_focus(&before->selected)
        || kbo_ai_roster_flow_player_focus(&after->selected)
        || kbo_ai_roster_flow_player_changed_focus(&before->ptr528, &after->ptr528)
        || kbo_ai_roster_flow_player_changed_focus(&before->ptr530, &after->ptr530)
        || kbo_ai_roster_flow_player_changed_focus(&before->ptr538, &after->ptr538)
        || kbo_ai_roster_flow_player_changed_focus(&before->ptr540, &after->ptr540)
        || kbo_ai_roster_flow_player_changed_focus(&before->ptr548, &after->ptr548)
        || ((kbo_ai_roster_flow_player_focus(&before->ptr528)
                || kbo_ai_roster_flow_player_focus(&before->ptr530)
                || kbo_ai_roster_flow_player_focus(&before->ptr538)
                || kbo_ai_roster_flow_player_focus(&before->ptr540)
                || kbo_ai_roster_flow_player_focus(&before->ptr548))
            && (before->state8d0 != after->state8d0
                || before->state8d8 != after->state8d8
                || before->active_slot != after->active_slot
                || before->flag86c != after->flag86c
                || before->flag86d != after->flag86d));
}

static void kbo_ai_roster_log_context_flow_trace(
    const char* label,
    uint32_t caller_rva,
    uintptr_t context_ptr,
    int original_installed,
    const KboAiRosterFlowContextSnapshot* before,
    const KboAiRosterFlowContextSnapshot* after)
{
    if (!kbo_ai_roster_flow_context_focus(before, after)) {
        return;
    }

    int target_focus = before->selected.target || after->selected.target
        || before->ptr528.target || after->ptr528.target
        || before->ptr530.target || after->ptr530.target
        || before->ptr538.target || after->ptr538.target
        || before->ptr540.target || after->ptr540.target
        || before->ptr548.target || after->ptr548.target;

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot > 3000 && !target_focus) {
        if (trace_slot == 3001) {
            append_log_line("ootp ai roster context flow trace suppressed after 3000 entries");
        }
        return;
    }

    append_logf(
        "ootp ai roster context flow trace #%ld label=%s caller_rva=0x%x original=%d context=%p primary=%u secondary=%u active_slot=%u->%u p_target=%u/%u s_target=%u/%u flags4c=0:%u 1:%u 2:%u 4:%u p4c7=%u p4cd=%u s4c9=%u s4cb=%u flag7ec=%u flag86c=%u flag86d=%u state8d0=%d->%d state8d8=%d->%d p_empty=%d->%d s_empty=%d->%d selected=%u->%u selected_for=%d->%d selected_target=%d->%d selected_f62=%u->%u selected_f68=%u->%u selected_f1a=%u->%u selected_ef8=%d->%d ptr528=%u->%u ptr528_for=%d->%d ptr528_f1a=%u->%u ptr530=%u->%u ptr530_for=%d->%d ptr530_f1a=%u->%u ptr538=%u->%u ptr540=%u->%u ptr548=%u->%u",
        trace_slot,
        label != NULL ? label : "",
        caller_rva,
        original_installed,
        (void*)context_ptr,
        before->primary_slot,
        before->secondary_slot,
        before->active_slot,
        after->active_slot,
        before->primary_target_slot,
        before->primary_expected_slot,
        before->secondary_target_slot,
        before->secondary_expected_slot,
        before->flag4c0,
        before->flag4c1,
        before->flag4c2,
        before->flag4c4,
        before->primary_flag4c7,
        before->primary_flag4cd,
        before->secondary_flag4c9,
        before->secondary_flag4cb,
        before->flag7ec,
        before->flag86c,
        before->flag86d,
        before->state8d0,
        after->state8d0,
        before->state8d8,
        after->state8d8,
        before->primary_first_empty,
        after->primary_first_empty,
        before->secondary_first_empty,
        after->secondary_first_empty,
        before->selected.player_id,
        after->selected.player_id,
        before->selected.foreign,
        after->selected.foreign,
        before->selected.target,
        after->selected.target,
        before->selected.f62,
        after->selected.f62,
        before->selected.f68,
        after->selected.f68,
        before->selected.f1a,
        after->selected.f1a,
        before->selected.ef8,
        after->selected.ef8,
        before->ptr528.player_id,
        after->ptr528.player_id,
        before->ptr528.foreign,
        after->ptr528.foreign,
        before->ptr528.f1a,
        after->ptr528.f1a,
        before->ptr530.player_id,
        after->ptr530.player_id,
        before->ptr530.foreign,
        after->ptr530.foreign,
        before->ptr530.f1a,
        after->ptr530.f1a,
        before->ptr538.player_id,
        after->ptr538.player_id,
        before->ptr540.player_id,
        after->ptr540.player_id,
        before->ptr548.player_id,
        after->ptr548.player_id);
}

static void kbo_ai_roster_context_flow_apply_rescue_skip_log(
    const char* reason,
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    const KboAiRosterFlowContextSnapshot* after,
    uint32_t active_team_id,
    int callup_allowed)
{
    if (before == NULL || after == NULL || !before->selected.target) {
        return;
    }

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot > 240) {
        return;
    }

    append_logf(
        "ootp ai roster foreign apply rescue skip #%ld reason=%s context=%p primary=%u target_slot=%u active_team=%u callup_allowed=%d selected=%u foreign=%d current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f61=%u f62=%u f65=%u f68=%u f1a=%u f3e=%u f06=%d fec=%u ef8=%d ptr528=%u->%u ptr528_for=%d->%d",
        trace_slot,
        reason != NULL ? reason : "",
        (void*)context_ptr,
        before->primary_slot,
        before->primary_target_slot,
        active_team_id,
        callup_allowed,
        before->selected.player_id,
        before->selected.foreign,
        before->selected.current_team_id,
        before->selected.active_team_id,
        before->selected.league_id,
        before->selected.default_team_id,
        before->selected.status24,
        before->selected.status25,
        before->selected.status26,
        before->selected.f61,
        before->selected.f62,
        before->selected.f65,
        before->selected.f68,
        before->selected.f1a,
        before->selected.f3e,
        before->selected.f06,
        before->selected.fec,
        before->selected.ef8,
        before->ptr528.player_id,
        after->ptr528.player_id,
        before->ptr528.foreign,
        after->ptr528.foreign);
}

static void kbo_ai_roster_record_foreign_apply_rescue(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uintptr_t player_ptr,
    uint32_t player_id,
    uint16_t slot_index,
    uint16_t target_slot)
{
    if (slot_block_ptr == 0u || player_id == 0u || slot_index >= 64u || target_slot > 8u) {
        return;
    }

    LONG next = InterlockedIncrement(&g_kbo_ai_roster_apply_rescue_slot_next);
    LONG slot = (next - 1) % KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT;
    if (slot < 0) {
        slot += KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT;
    }

    g_kbo_ai_roster_apply_rescue_slots[slot].context_ptr = context_ptr;
    g_kbo_ai_roster_apply_rescue_slots[slot].slot_block_ptr = slot_block_ptr;
    g_kbo_ai_roster_apply_rescue_slots[slot].player_ptr = player_ptr;
    g_kbo_ai_roster_apply_rescue_slots[slot].player_id = player_id;
    g_kbo_ai_roster_apply_rescue_slots[slot].slot_index = slot_index;
    g_kbo_ai_roster_apply_rescue_slots[slot].target_slot = target_slot;
    g_kbo_ai_roster_apply_rescue_slots[slot].tick = GetTickCount();
}

static int kbo_ai_roster_recent_foreign_apply_rescue_match(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uint16_t target_slot,
    uint32_t player_id,
    DWORD* out_age_ms)
{
    if (out_age_ms != NULL) {
        *out_age_ms = 0u;
    }
    if (slot_block_ptr == 0u || target_slot > 8u || player_id == 0u) {
        return 0;
    }

    DWORD now = GetTickCount();
    for (int i = 0; i < KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT; i++) {
        KboAiRosterApplyRescueSlot entry = g_kbo_ai_roster_apply_rescue_slots[i];
        if (entry.player_id != player_id
                || entry.slot_block_ptr != slot_block_ptr
                || entry.target_slot != target_slot) {
            continue;
        }
        if (entry.context_ptr != 0u && context_ptr != 0u && entry.context_ptr != context_ptr) {
            continue;
        }

        DWORD age_ms = now - entry.tick;
        if (age_ms > KBO_AI_ROSTER_APPLY_RESCUE_SHIELD_MS) {
            continue;
        }

        if (out_age_ms != NULL) {
            *out_age_ms = age_ms;
        }
        return 1;
    }

    return 0;
}

static uint8_t kbo_ai_roster_context_flow_apply_rescue_active_move(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    uint32_t active_team_id,
    uintptr_t slot_block_ptr,
    uint16_t target_slot)
{
    if (context_ptr == 0u
            || before == NULL
            || !before->selected.plausible
            || before->selected.ptr == 0u
            || before->selected.player_id == 0u
            || active_team_id == 0u
            || g_kbo_roster_move_active_trace_trampoline == NULL
            || !read_kbo_localappdata_flag_file("enable_ai_roster_foreign_apply_rescue_move.txt")
            || read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue_move.txt")) {
        return 0u;
    }

    uint8_t* active_team = find_kbo_team_by_numeric_id_any_league(active_team_id, 1);
    if (active_team == NULL || !memory_range_readable(active_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        append_logf(
            "ootp ai roster foreign apply rescue active move skip reason=no_active_team context=%p player=%u active_team=%u slot_block=%p target_slot=%u",
            (void*)context_ptr,
            before->selected.player_id,
            active_team_id,
            (void*)slot_block_ptr,
            target_slot);
        return 0u;
    }

    uint32_t team_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_id != active_team_id) {
        append_logf(
            "ootp ai roster foreign apply rescue active move skip reason=team_mismatch context=%p player=%u active_team=%u found_team=%u team_league=%u slot_block=%p target_slot=%u",
            (void*)context_ptr,
            before->selected.player_id,
            active_team_id,
            team_id,
            team_league_id,
            (void*)slot_block_ptr,
            target_slot);
        return 0u;
    }

    uint32_t before_current_team_id = before->selected.current_team_id;
    uint32_t before_active_team_id = before->selected.active_team_id;
    uint32_t before_league_id = before->selected.league_id;
    uint32_t before_status24 = before->selected.status24;
    uint32_t before_status25 = before->selected.status25;
    uint32_t before_status26 = before->selected.status26;
    uint32_t before_f62 = before->selected.f62;
    uint32_t before_f65 = before->selected.f65;
    uint32_t before_f68 = before->selected.f68;
    uint32_t before_f1a = before->selected.f1a;

    uint8_t result = g_kbo_roster_move_active_trace_trampoline(
        (uintptr_t)active_team,
        before->selected.ptr,
        1u,
        0u,
        0u,
        0u,
        0u,
        0u);

    KboAiRosterFlowPlayerSnapshot after_player;
    kbo_ai_roster_flow_read_player(before->selected.ptr, &after_player);

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot <= 300 || before->selected.target || after_player.target) {
        append_logf(
            "ootp ai roster foreign apply rescue active move #%ld result=%u context=%p slot_block=%p target_slot=%u team=%u team_league=%u player=%u target=%d foreign=%d nation=%u current=%u->%u active=%u->%u league=%u->%u default_team=%u status24=%u->%u status25=%u->%u status26=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u f3e=%u f06=%d fec=%u ef8=%d overall=%d ratings=%d",
            trace_slot,
            (uint32_t)result,
            (void*)context_ptr,
            (void*)slot_block_ptr,
            target_slot,
            team_id,
            team_league_id,
            before->selected.player_id,
            before->selected.target,
            before->selected.foreign,
            before->selected.nation_id,
            before_current_team_id,
            after_player.current_team_id,
            before_active_team_id,
            after_player.active_team_id,
            before_league_id,
            after_player.league_id,
            before->selected.default_team_id,
            before_status24,
            after_player.status24,
            before_status25,
            after_player.status25,
            before_status26,
            after_player.status26,
            before_f62,
            after_player.f62,
            before_f65,
            after_player.f65,
            before_f68,
            after_player.f68,
            before_f1a,
            after_player.f1a,
            after_player.f3e,
            after_player.f06,
            after_player.fec,
            after_player.ef8,
            after_player.overall,
            after_player.ratings);
    }

    return result;
}

static uint8_t kbo_ai_roster_context_flow_apply_rescue_team_add(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    uint32_t active_team_id,
    int callup_allowed,
    uintptr_t slot_block_ptr,
    uint16_t target_slot)
{
    int team_add_enabled = kbo_ai_roster_foreign_apply_rescue_team_add_enabled();
    if (context_ptr == 0u
            || before == NULL
            || !before->selected.plausible
            || before->selected.ptr == 0u
            || before->selected.player_id == 0u
            || active_team_id == 0u
            || !callup_allowed
            || g_kbo_team_add_player_guard_trampoline == NULL
            || !team_add_enabled) {
        static volatile LONG precheck_skip_log_count = 0;
        LONG precheck_slot = InterlockedIncrement(&precheck_skip_log_count);
        if (precheck_slot <= 300
                || (before != NULL && before->selected.target)) {
            append_logf(
                "ootp ai roster foreign apply rescue team-add skip #%ld reason=precheck context=%p before=%p plausible=%d ptr=%p player=%u target=%d foreign=%d current=%u active=%u league=%u default_team=%u status24=%u status25=%u status26=%u f62=%u f65=%u f06=%d active_team=%u callup_allowed=%d trampoline=%p enabled=%d custom_policy=%d enable_flag=%d disable_flag=%d source_select_enabled=%d slot_block=%p target_slot=%u",
                precheck_slot,
                (void*)context_ptr,
                (const void*)before,
                before != NULL ? before->selected.plausible : 0,
                before != NULL ? (void*)before->selected.ptr : NULL,
                before != NULL ? before->selected.player_id : 0u,
                before != NULL ? before->selected.target : 0,
                before != NULL ? before->selected.foreign : 0,
                before != NULL ? before->selected.current_team_id : 0u,
                before != NULL ? before->selected.active_team_id : 0u,
                before != NULL ? before->selected.league_id : 0u,
                before != NULL ? before->selected.default_team_id : 0u,
                before != NULL ? before->selected.status24 : 0u,
                before != NULL ? before->selected.status25 : 0u,
                before != NULL ? before->selected.status26 : 0u,
                before != NULL ? before->selected.f62 : 0u,
                before != NULL ? before->selected.f65 : 0u,
                before != NULL ? before->selected.f06 : 0,
                active_team_id,
                callup_allowed,
                (void*)g_kbo_team_add_player_guard_trampoline,
                team_add_enabled,
                kbo_custom_foreign_policy_enabled(),
                read_kbo_localappdata_flag_file("enable_ai_roster_foreign_apply_rescue_team_add.txt"),
                read_kbo_localappdata_flag_file("disable_ai_roster_foreign_apply_rescue_team_add.txt"),
                kbo_ai_roster_foreign_source_select_rescue_enabled(),
                (void*)slot_block_ptr,
                target_slot);
        }
        return 0u;
    }

    uint8_t* active_team = find_kbo_team_by_numeric_id_any_league(active_team_id, 1);
    if (active_team == NULL || !memory_range_readable(active_team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        append_logf(
            "ootp ai roster foreign apply rescue team-add skip reason=no_active_team context=%p player=%u active_team=%u slot_block=%p target_slot=%u",
            (void*)context_ptr,
            before->selected.player_id,
            active_team_id,
            (void*)slot_block_ptr,
            target_slot);
        return 0u;
    }

    uint32_t team_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_ID_OFFSET);
    uint32_t team_league_id = *(uint32_t*)(active_team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET);
    if (team_id != active_team_id) {
        append_logf(
            "ootp ai roster foreign apply rescue team-add skip reason=team_mismatch context=%p player=%u active_team=%u found_team=%u team_league=%u slot_block=%p target_slot=%u",
            (void*)context_ptr,
            before->selected.player_id,
            active_team_id,
            team_id,
            team_league_id,
            (void*)slot_block_ptr,
            target_slot);
        return 0u;
    }

    if (before->selected.current_team_id == team_id
            && before->selected.active_team_id == team_id
            && before->selected.league_id == team_league_id) {
        append_logf(
            "ootp ai roster foreign apply rescue team-add skip reason=already_active context=%p player=%u team=%u team_league=%u slot_block=%p target_slot=%u",
            (void*)context_ptr,
            before->selected.player_id,
            team_id,
            team_league_id,
            (void*)slot_block_ptr,
            target_slot);
        return 0u;
    }

    uint32_t before_current_team_id = before->selected.current_team_id;
    uint32_t before_active_team_id = before->selected.active_team_id;
    uint32_t before_league_id = before->selected.league_id;
    uint32_t before_status24 = before->selected.status24;
    uint32_t before_status25 = before->selected.status25;
    uint32_t before_status26 = before->selected.status26;
    uint32_t before_f62 = before->selected.f62;
    uint32_t before_f65 = before->selected.f65;
    uint32_t before_f68 = before->selected.f68;
    uint32_t before_f1a = before->selected.f1a;

    uint8_t result = g_kbo_team_add_player_guard_trampoline(
        (uintptr_t)active_team,
        before->selected.ptr,
        0u,
        0u,
        0u,
        0u,
        0u,
        0u);

    KboAiRosterFlowPlayerSnapshot after_player;
    kbo_ai_roster_flow_read_player(before->selected.ptr, &after_player);

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot <= 300 || before->selected.target || after_player.target) {
        append_logf(
            "ootp ai roster foreign apply rescue team-add #%ld result=%u context=%p slot_block=%p target_slot=%u team=%u team_league=%u player=%u target=%d foreign=%d nation=%u current=%u->%u active=%u->%u league=%u->%u default_team=%u status24=%u->%u status25=%u->%u status26=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u f3e=%u f06=%d fec=%u ef8=%d overall=%d ratings=%d",
            trace_slot,
            (uint32_t)result,
            (void*)context_ptr,
            (void*)slot_block_ptr,
            target_slot,
            team_id,
            team_league_id,
            before->selected.player_id,
            before->selected.target,
            before->selected.foreign,
            before->selected.nation_id,
            before_current_team_id,
            after_player.current_team_id,
            before_active_team_id,
            after_player.active_team_id,
            before_league_id,
            after_player.league_id,
            before->selected.default_team_id,
            before_status24,
            after_player.status24,
            before_status25,
            after_player.status25,
            before_status26,
            after_player.status26,
            before_f62,
            after_player.f62,
            before_f65,
            after_player.f65,
            before_f68,
            after_player.f68,
            before_f1a,
            after_player.f1a,
            after_player.f3e,
            after_player.f06,
            after_player.fec,
            after_player.ef8,
            after_player.overall,
            after_player.ratings);
    }

    return result;
}

static int kbo_ai_roster_context_flow_apply_rescue(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    const KboAiRosterFlowContextSnapshot* after)
{
    if (context_ptr == 0u
            || before == NULL
            || after == NULL
            || g_kbo_ai_roster_apply_selection_trace_trampoline == NULL
            || !kbo_ai_roster_foreign_apply_rescue_enabled()) {
        return 0;
    }

    if (!before->selected.plausible
            || !before->selected.foreign
            || after->selected.ptr != 0u) {
        return 0;
    }

    if (before->selected.ptr != before->ptr528.ptr
            && before->selected.ptr != after->ptr528.ptr) {
        return 0;
    }

    if (before->primary_slot >= 64u || before->primary_target_slot > 8u) {
        kbo_ai_roster_context_flow_apply_rescue_skip_log(
            "bad_slot",
            context_ptr,
            before,
            after,
            0u,
            0);
        return 0;
    }

    if (before->primary_slot_block == 0u) {
        kbo_ai_roster_context_flow_apply_rescue_skip_log(
            "no_slot_block",
            context_ptr,
            before,
            after,
            0u,
            0);
        return 0;
    }

    int team_add_rescue_enabled = kbo_ai_roster_foreign_apply_rescue_team_add_enabled();
    if (before->selected.f65 != 0u
            || (before->selected.status26 != 0u
                && (!team_add_rescue_enabled || before->selected.status26 != 1u))) {
        kbo_ai_roster_context_flow_apply_rescue_skip_log(
            "not_clean_candidate",
            context_ptr,
            before,
            after,
            0u,
            0);
        return 0;
    }

    uint32_t active_team_id = 0u;
    int callup_allowed = 0;
    if (!kbo_ai_player_quality_minor_foreign_callup_allows(
            (int32_t)before->selected.league_id,
            (uint8_t*)before->selected.ptr,
            &active_team_id,
            &callup_allowed)) {
        kbo_ai_roster_context_flow_apply_rescue_skip_log(
            "callup_not_allowed_or_not_minor",
            context_ptr,
            before,
            after,
            active_team_id,
            callup_allowed);
        return 0;
    }

    uint16_t target_slot = before->primary_target_slot;
    uintptr_t slot_block_ptr = before->primary_slot_block;
    uint32_t slot_team_id = kbo_ai_roster_context_slot_team_id(context_ptr, before->primary_slot);
    uint32_t before_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, target_slot);
    uint32_t before_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, target_slot);

    g_kbo_ai_roster_apply_selection_trace_trampoline(
        context_ptr,
        (int32_t)before->primary_slot,
        before->selected.ptr,
        (int32_t)target_slot,
        11);

    uint32_t after_slot_code = kbo_ai_roster_slot_code_at(slot_block_ptr, target_slot);
    uint32_t after_slot_player_id = kbo_ai_roster_slot_player_at(slot_block_ptr, target_slot);
    if (after_slot_player_id == before->selected.player_id) {
        kbo_ai_roster_context_flow_apply_rescue_team_add(
            context_ptr,
            before,
            active_team_id,
            callup_allowed,
            slot_block_ptr,
            target_slot);
        kbo_ai_roster_context_flow_apply_rescue_active_move(
            context_ptr,
            before,
            active_team_id,
            slot_block_ptr,
            target_slot);
        kbo_ai_roster_record_foreign_apply_rescue(
            context_ptr,
            slot_block_ptr,
            before->selected.ptr,
            before->selected.player_id,
            before->primary_slot,
            target_slot);
    }

    KboAiRosterFlowPlayerSnapshot after_player;
    kbo_ai_roster_flow_read_player(before->selected.ptr, &after_player);

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot <= 1000 || before->selected.target || after_player.target) {
        append_logf(
            "ootp ai roster foreign apply rescue #%ld context=%p slot_index=%u target_slot=%u roster_code=11 slot_block=%p slot_team=%u active_team=%u before_slot_code=%u after_slot_code=%u before_slot_player=%u after_slot_player=%u player=%u target=%d foreign=%d nation=%u current=%u->%u active=%u->%u league=%u default_team=%u status24=%u->%u status25=%u->%u status26=%u->%u f61=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u f3e=%u->%u f06=%d->%d fec=%u->%u ef8=%d->%d score_fe0=%d score_fe4=%d overall=%d ratings=%d",
            trace_slot,
            (void*)context_ptr,
            before->primary_slot,
            target_slot,
            (void*)slot_block_ptr,
            slot_team_id,
            active_team_id,
            before_slot_code,
            after_slot_code,
            before_slot_player_id,
            after_slot_player_id,
            before->selected.player_id,
            before->selected.target,
            before->selected.foreign,
            before->selected.nation_id,
            before->selected.current_team_id,
            after_player.current_team_id,
            before->selected.active_team_id,
            after_player.active_team_id,
            before->selected.league_id,
            before->selected.default_team_id,
            before->selected.status24,
            after_player.status24,
            before->selected.status25,
            after_player.status25,
            before->selected.status26,
            after_player.status26,
            before->selected.f61,
            after_player.f61,
            before->selected.f62,
            after_player.f62,
            before->selected.f65,
            after_player.f65,
            before->selected.f68,
            after_player.f68,
            before->selected.f1a,
            after_player.f1a,
            before->selected.f3e,
            after_player.f3e,
            before->selected.f06,
            after_player.f06,
            before->selected.fec,
            after_player.fec,
            before->selected.ef8,
            after_player.ef8,
            before->selected.score_fe0,
            before->selected.score_fe4,
            after_player.overall,
            after_player.ratings);
    }

    return 1;
}

static void kbo_ai_roster_context_flow_trace_run(
    const char* label,
    OotpKboAiRosterContextFlowFn original,
    uintptr_t context_ptr,
    uint32_t caller_rva,
    int allow_apply_rescue)
{
    KboAiRosterFlowContextSnapshot before;
    KboAiRosterFlowContextSnapshot after;
    kbo_ai_roster_flow_read_context(context_ptr, &before);

    if (original != NULL) {
        original(context_ptr);
    }

    kbo_ai_roster_flow_read_context(context_ptr, &after);
    if (allow_apply_rescue && kbo_ai_roster_context_flow_apply_rescue(context_ptr, &before, &after)) {
        kbo_ai_roster_flow_read_context(context_ptr, &after);
    }
    kbo_ai_roster_log_context_flow_trace(
        label,
        caller_rva,
        context_ptr,
        original != NULL ? 1 : 0,
        &before,
        &after);
}

__declspec(noinline) void ootp_kbo_ai_roster_primary_apply_flow_trace_wrapper(
    uintptr_t context_ptr)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    kbo_ai_roster_context_flow_trace_run(
        "primary_dd1b20",
        g_kbo_ai_roster_primary_apply_flow_trace_trampoline,
        context_ptr,
        caller_rva,
        1);
}

__declspec(noinline) void ootp_kbo_ai_roster_secondary_main_flow_trace_wrapper(
    uintptr_t context_ptr)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    kbo_ai_roster_context_flow_trace_run(
        "secondary_main_dce3b0",
        g_kbo_ai_roster_secondary_main_flow_trace_trampoline,
        context_ptr,
        caller_rva,
        0);
}

__declspec(noinline) void ootp_kbo_ai_roster_secondary_alt_flow_trace_wrapper(
    uintptr_t context_ptr)
{
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;
    kbo_ai_roster_context_flow_trace_run(
        "secondary_alt_dcddc0",
        g_kbo_ai_roster_secondary_alt_flow_trace_trampoline,
        context_ptr,
        caller_rva,
        0);
}

__declspec(noinline) void ootp_kbo_ai_roster_mark_selected_trace_wrapper(
    uintptr_t context_ptr,
    uintptr_t player_ptr,
    uintptr_t slot_block_ptr)
{
    OotpKboAiRosterMarkSelectedFn original = g_kbo_ai_roster_mark_selected_trace_trampoline;

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    KboAiRosterFlowPlayerSnapshot before;
    KboAiRosterFlowPlayerSnapshot after;
    kbo_ai_roster_flow_read_player(player_ptr, &before);

    if (original != NULL) {
        original(context_ptr, player_ptr, slot_block_ptr);
    }

    kbo_ai_roster_flow_read_player(player_ptr, &after);
    if (!kbo_ai_roster_flow_player_focus(&before) && !kbo_ai_roster_flow_player_focus(&after)) {
        return;
    }

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot > 3000 && !before.target && !after.target) {
        if (trace_slot == 3001) {
            append_log_line("ootp ai roster mark-selected trace suppressed after 3000 entries");
        }
        return;
    }

    uint16_t primary_slot = kbo_ai_roster_context_u16(context_ptr, KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET);
    uint16_t secondary_slot = kbo_ai_roster_context_u16(context_ptr, KBO_AI_ROSTER_CONTEXT_SECONDARY_SLOT_OFFSET);
    append_logf(
        "ootp ai roster mark-selected trace #%ld caller_rva=0x%x original=%d context=%p primary=%u secondary=%u slot_block=%p player=%u foreign=%d target=%d nation=%u current=%u->%u active=%u->%u league=%u status24=%u status25=%u status26=%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u f3e=%u->%u ef8=%d->%d f06=%d fe0=%d fe4=%d overall=%d ratings=%d",
        trace_slot,
        caller_rva,
        original != NULL ? 1 : 0,
        (void*)context_ptr,
        primary_slot,
        secondary_slot,
        (void*)slot_block_ptr,
        before.player_id,
        before.foreign,
        before.target,
        before.nation_id,
        before.current_team_id,
        after.current_team_id,
        before.active_team_id,
        after.active_team_id,
        before.league_id,
        before.status24,
        before.status25,
        before.status26,
        before.f62,
        after.f62,
        before.f65,
        after.f65,
        before.f68,
        after.f68,
        before.f1a,
        after.f1a,
        before.f3e,
        after.f3e,
        before.ef8,
        after.ef8,
        kbo_read_player_i16((uint8_t*)player_ptr, 0xf06u),
        before.score_fe0,
        before.score_fe4,
        before.overall,
        before.ratings);
}

__declspec(noinline) void ootp_kbo_ai_roster_selection_reconcile_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr)
{
    OotpKboAiRosterSelectionReconcileFn original =
        g_kbo_ai_roster_selection_reconcile_trace_trampoline;

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    uintptr_t slot_block_ptr = 0u;
    uint32_t slot_team_id = 0u;
    uintptr_t context_selected_before = 0u;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        uintptr_t slot_block_slot = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET
            + (uintptr_t)slot_index * sizeof(uintptr_t);
        uintptr_t slot_team_slot = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET
            + (uintptr_t)slot_index * sizeof(uint32_t);
        if (memory_range_readable((void*)slot_block_slot, sizeof(uintptr_t))) {
            slot_block_ptr = *(uintptr_t*)slot_block_slot;
        }
        if (memory_range_readable((void*)slot_team_slot, sizeof(uint32_t))) {
            slot_team_id = *(uint32_t*)slot_team_slot;
        }
    }
    if (context_ptr != 0u
            && memory_range_readable((void*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET), sizeof(uintptr_t))) {
        context_selected_before = *(uintptr_t*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET);
    }

    int player_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    uint8_t* player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t player_id = player_plausible ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) : 0u;
    uint32_t nation_id = player_plausible ? *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) : 0u;
    uint32_t before_current_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t before_active_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET)
        : 0u;
    uint32_t league_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET)
        : 0u;
    uint32_t default_team_id = 0u;
    uintptr_t status_ptr = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    if (player_plausible && memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    if (player_plausible && memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
    }

    uint32_t before_f62 = player_plausible ? (uint32_t)player[0xf62u] : 0u;
    uint32_t before_f65 = player_plausible ? (uint32_t)player[0xf65u] : 0u;
    uint32_t before_f68 = player_plausible ? (uint32_t)player[0xf68u] : 0u;
    uint32_t before_f78 = player_plausible ? (uint32_t)player[0xf78u] : 0u;
    uint32_t before_f1a = player_plausible ? (uint32_t)player[0xf1au] : 0u;
    int16_t before_ef8 = player_plausible ? kbo_read_player_i16(player, 0xef8u) : 0;
    uint32_t before_slot_code = 0u;
    int32_t before_player_slot = kbo_ai_roster_slot_index_for_player(
        slot_block_ptr,
        player_id,
        &before_slot_code);
    int32_t before_first_empty_slot = kbo_ai_roster_first_empty_slot(slot_block_ptr);

    if (original != NULL) {
        original(context_ptr, slot_index, player_ptr);
    }

    uintptr_t context_selected_after = 0u;
    if (context_ptr != 0u
            && memory_range_readable((void*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET), sizeof(uintptr_t))) {
        context_selected_after = *(uintptr_t*)(context_ptr + KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET);
    }

    player_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t after_current_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t after_active_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET)
        : 0u;
    uint32_t after_f62 = player_plausible ? (uint32_t)player[0xf62u] : 0u;
    uint32_t after_f65 = player_plausible ? (uint32_t)player[0xf65u] : 0u;
    uint32_t after_f68 = player_plausible ? (uint32_t)player[0xf68u] : 0u;
    uint32_t after_f78 = player_plausible ? (uint32_t)player[0xf78u] : 0u;
    uint32_t after_f1a = player_plausible ? (uint32_t)player[0xf1au] : 0u;
    uint32_t after_f3e = player_plausible ? (uint32_t)player[0xf3eu] : 0u;
    int16_t after_ef8 = player_plausible ? kbo_read_player_i16(player, 0xef8u) : 0;
    uint32_t after_slot_code = 0u;
    int32_t after_player_slot = kbo_ai_roster_slot_index_for_player(
        slot_block_ptr,
        player_id,
        &after_slot_code);
    int32_t after_first_empty_slot = kbo_ai_roster_first_empty_slot(slot_block_ptr);

    int selected_foreign = player_plausible && kbo_player_is_foreign_for_kbo_rights(player);
    int target_player = kbo_ai_roster_research_target_player_id(player_id);
    int focus = selected_foreign || target_player || before_player_slot >= 0 || after_player_slot >= 0;
    if (!focus) {
        return;
    }

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot > 2000 && !target_player) {
        if (trace_slot == 2001) {
            append_log_line("ootp ai roster reconcile trace suppressed after 2000 entries");
        }
        return;
    }

    append_logf(
        "ootp ai roster reconcile trace #%ld caller_rva=0x%x original=%d context=%p slot_index=%d slot_block=%p slot_team=%u context_selected=%p->%p player=%u selected_foreign=%d target=%d nation=%u current=%u->%u active=%u->%u league=%u default_team=%u status_ptr=%p status24=%u status25=%u status26=%u player_slot=%d->%d slot_code=%u->%u first_empty=%d->%d f62=%u->%u f65=%u->%u f68=%u->%u f78=%u->%u f1a=%u->%u f3e=%u ef8=%d->%d overall=%d talent=%d ratings=%d",
        trace_slot,
        caller_rva,
        original != NULL ? 1 : 0,
        (void*)context_ptr,
        slot_index,
        (void*)slot_block_ptr,
        slot_team_id,
        (void*)context_selected_before,
        (void*)context_selected_after,
        player_id,
        selected_foreign,
        target_player,
        nation_id,
        before_current_team_id,
        after_current_team_id,
        before_active_team_id,
        after_active_team_id,
        league_id,
        default_team_id,
        (void*)status_ptr,
        status24,
        status25,
        status26,
        before_player_slot,
        after_player_slot,
        before_slot_code,
        after_slot_code,
        before_first_empty_slot,
        after_first_empty_slot,
        before_f62,
        after_f62,
        before_f65,
        after_f65,
        before_f68,
        after_f68,
        before_f78,
        after_f78,
        before_f1a,
        after_f1a,
        after_f3e,
        before_ef8,
        after_ef8,
        player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET) : 0,
        player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET) : 0,
        player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET) : 0);
}

__declspec(noinline) void ootp_kbo_ai_roster_apply_selection_trace_wrapper(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code)
{
    OotpKboAiRosterApplySelectionFn original = g_kbo_ai_roster_apply_selection_trace_trampoline;

    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    uintptr_t slot_block_ptr = 0u;
    uint32_t slot_team_id = 0u;
    if (context_ptr != 0u && slot_index >= 0 && slot_index < 64) {
        uintptr_t slot_block_slot = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET
            + (uintptr_t)slot_index * sizeof(uintptr_t);
        uintptr_t slot_team_slot = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET
            + (uintptr_t)slot_index * sizeof(uint32_t);
        if (memory_range_readable((void*)slot_block_slot, sizeof(uintptr_t))) {
            slot_block_ptr = *(uintptr_t*)slot_block_slot;
        }
        if (memory_range_readable((void*)slot_team_slot, sizeof(uint32_t))) {
            slot_team_id = *(uint32_t*)slot_team_slot;
        }
    }

    uint32_t before_slot_code = 0u;
    uint32_t after_slot_code = 0u;
    uint32_t before_slot_player_id = 0u;
    uint32_t after_slot_player_id = 0u;
    if (slot_block_ptr != 0u && target_slot >= 0 && target_slot <= 8) {
        uintptr_t slot_code_addr = slot_block_ptr + 0x14d8u + (uintptr_t)target_slot * 8u;
        uintptr_t slot_player_addr = slot_block_ptr + 0x14d4u + (uintptr_t)target_slot * 8u;
        if (memory_range_readable((void*)slot_code_addr, sizeof(uint8_t))) {
            before_slot_code = *(uint8_t*)slot_code_addr;
        }
        if (memory_range_readable((void*)slot_player_addr, sizeof(uint32_t))) {
            before_slot_player_id = *(uint32_t*)slot_player_addr;
        }
    }

    int player_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    uint8_t* player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t player_id = player_plausible ? *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET) : 0u;
    uint32_t nation_id = player_plausible ? *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) : 0u;
    uint32_t before_current_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t before_active_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET)
        : 0u;
    uint32_t league_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET)
        : 0u;
    uint32_t default_team_id = 0u;
    if (player_plausible && memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }

    uintptr_t status_ptr = 0u;
    uint32_t status24 = 0u;
    uint32_t status25 = 0u;
    uint32_t status26 = 0u;
    if (player_plausible && memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    }
    if (status_ptr != 0u && memory_range_readable((void*)status_ptr, 0x29u)) {
        uint8_t* status = (uint8_t*)status_ptr;
        status24 = status[0x24u];
        status25 = status[0x25u];
        status26 = status[0x26u];
    }

    uint32_t before_f61 = player_plausible ? (uint32_t)player[0xf61u] : 0u;
    uint32_t before_f62 = player_plausible ? (uint32_t)player[0xf62u] : 0u;
    uint32_t before_f65 = player_plausible ? (uint32_t)player[0xf65u] : 0u;
    uint32_t before_f68 = player_plausible ? (uint32_t)player[0xf68u] : 0u;
    uint32_t before_f1a = player_plausible ? (uint32_t)player[0xf1au] : 0u;
    int16_t before_f06 = player_plausible ? kbo_read_player_i16(player, 0xf06u) : 0;
    uint32_t before_fec = player_plausible && memory_range_readable(player + 0xfecu, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0xfecu)
        : 0u;
    uint32_t before_role800 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 0) : 0u;
    uint32_t before_role802 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 1) : 0u;
    uint32_t before_role804 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 2) : 0u;
    uint32_t before_role806 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 3) : 0u;
    uint32_t before_role808 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 4) : 0u;
    uint32_t before_role80a = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 5) : 0u;
    int16_t before_ef8 = player_plausible ? kbo_read_player_i16(player, 0xef8u) : 0;
    int32_t before_score_fe0 = player_plausible
        ? kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET)
        : 0;
    int32_t before_score_fe4 = player_plausible
        ? kbo_read_ai_roster_select_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET)
        : 0;

    int incoming_foreign_for_shield = player_plausible && kbo_player_is_foreign_for_kbo_rights(player);
    int incoming_target_for_shield = kbo_ai_roster_research_target_player_id(player_id);
    DWORD rescue_age_ms = 0u;
    if (original != NULL
            && slot_block_ptr != 0u
            && target_slot >= 0
            && target_slot <= 8
            && player_plausible
            && player_id != 0u
            && !incoming_foreign_for_shield
            && !incoming_target_for_shield
            && before_slot_player_id != 0u
            && before_slot_player_id != player_id
            && kbo_ai_roster_recent_foreign_apply_rescue_match(
                context_ptr,
                slot_block_ptr,
                (uint16_t)target_slot,
                before_slot_player_id,
                &rescue_age_ms)) {
        static volatile LONG shield_log_count = 0;
        LONG shield_slot = InterlockedIncrement(&shield_log_count);
        if (shield_slot <= 1000 || kbo_ai_roster_research_target_player_id(before_slot_player_id)) {
            append_logf(
                "ootp ai roster foreign apply rescue shield #%ld caller_rva=0x%x context=%p slot_index=%d target_slot=%d roster_code=%d age_ms=%lu slot_block=%p slot_team=%u before_slot_code=%u before_slot_player=%u incoming_player=%u incoming_foreign=%d incoming_target=%d incoming_nation=%u incoming_current=%u incoming_active=%u incoming_league=%u incoming_default_team=%u incoming_status24=%u incoming_status25=%u incoming_status26=%u incoming_f61=%u incoming_f62=%u incoming_f65=%u incoming_f68=%u incoming_f1a=%u incoming_f06=%d incoming_fec=%u incoming_score_fe0=%d incoming_score_fe4=%d incoming_overall=%d incoming_ratings=%d",
                shield_slot,
                caller_rva,
                (void*)context_ptr,
                slot_index,
                target_slot,
                roster_code,
                (unsigned long)rescue_age_ms,
                (void*)slot_block_ptr,
                slot_team_id,
                before_slot_code,
                before_slot_player_id,
                player_id,
                incoming_foreign_for_shield,
                incoming_target_for_shield,
                nation_id,
                before_current_team_id,
                before_active_team_id,
                league_id,
                default_team_id,
                status24,
                status25,
                status26,
                before_f61,
                before_f62,
                before_f65,
                before_f68,
                before_f1a,
                before_f06,
                before_fec,
                before_score_fe0,
                before_score_fe4,
                player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET) : 0,
                player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET) : 0);
        }
        return;
    }

    if (original != NULL) {
        original(context_ptr, slot_index, player_ptr, target_slot, roster_code);
    }

    if (slot_block_ptr != 0u && target_slot >= 0 && target_slot <= 8) {
        uintptr_t slot_code_addr = slot_block_ptr + 0x14d8u + (uintptr_t)target_slot * 8u;
        uintptr_t slot_player_addr = slot_block_ptr + 0x14d4u + (uintptr_t)target_slot * 8u;
        if (memory_range_readable((void*)slot_code_addr, sizeof(uint8_t))) {
            after_slot_code = *(uint8_t*)slot_code_addr;
        }
        if (memory_range_readable((void*)slot_player_addr, sizeof(uint32_t))) {
            after_slot_player_id = *(uint32_t*)slot_player_addr;
        }
    }

    player_plausible = kbo_player_pointer_plausible(player_ptr)
        && memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES);
    player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t after_current_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET)
        : 0u;
    uint32_t after_active_team_id = player_plausible
        ? *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET)
        : 0u;
    uint32_t after_f61 = player_plausible ? (uint32_t)player[0xf61u] : 0u;
    uint32_t after_f62 = player_plausible ? (uint32_t)player[0xf62u] : 0u;
    uint32_t after_f65 = player_plausible ? (uint32_t)player[0xf65u] : 0u;
    uint32_t after_f68 = player_plausible ? (uint32_t)player[0xf68u] : 0u;
    uint32_t after_f1a = player_plausible ? (uint32_t)player[0xf1au] : 0u;
    uint32_t after_f3e = player_plausible ? (uint32_t)player[0xf3eu] : 0u;
    int16_t after_f06 = player_plausible ? kbo_read_player_i16(player, 0xf06u) : 0;
    uint32_t after_fec = player_plausible && memory_range_readable(player + 0xfecu, sizeof(uint32_t))
        ? *(uint32_t*)(player + 0xfecu)
        : 0u;
    uint32_t after_role800 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 0) : 0u;
    uint32_t after_role802 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 1) : 0u;
    uint32_t after_role804 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 2) : 0u;
    uint32_t after_role806 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 3) : 0u;
    uint32_t after_role808 = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 4) : 0u;
    uint32_t after_role80a = player_plausible ? (uint32_t)kbo_ai_roster_role_slot_value(player, 5) : 0u;
    int16_t after_ef8 = player_plausible ? kbo_read_player_i16(player, 0xef8u) : 0;

    int selected_foreign = player_plausible && kbo_player_is_foreign_for_kbo_rights(player);
    int target_player = kbo_ai_roster_research_target_player_id(player_id);
    int target_slot_player = kbo_ai_roster_research_target_player_id(before_slot_player_id)
        || kbo_ai_roster_research_target_player_id(after_slot_player_id);
    if (!selected_foreign && !target_player && !target_slot_player) {
        return;
    }

    static volatile LONG trace_log_count = 0;
    LONG trace_slot = InterlockedIncrement(&trace_log_count);
    if (trace_slot > 2000 && !target_player && !target_slot_player) {
        if (trace_slot == 2001) {
            append_log_line("ootp ai roster apply-selection trace suppressed after 2000 entries");
        }
        return;
    }

    append_logf(
        "ootp ai roster apply-selection trace #%ld caller_rva=0x%x original=%d context=%p slot_index=%d target_slot=%d roster_code=%d slot_block=%p slot_team=%u before_slot_code=%u after_slot_code=%u before_slot_player=%u after_slot_player=%u player=%u selected_foreign=%d nation=%u current=%u->%u active=%u->%u league=%u default_team=%u status_ptr=%p status24=%u status25=%u status26=%u f61=%u->%u f62=%u->%u f65=%u->%u f68=%u->%u f1a=%u->%u f3e=%u f06=%d->%d fec=%u->%u role800=%u->%u role802=%u->%u role804=%u->%u role806=%u->%u role808=%u->%u role80a=%u->%u ef8=%d->%d score_fe0=%d score_fe4=%d overall=%d talent=%d ratings=%d",
        trace_slot,
        caller_rva,
        original != NULL ? 1 : 0,
        (void*)context_ptr,
        slot_index,
        target_slot,
        roster_code,
        (void*)slot_block_ptr,
        slot_team_id,
        before_slot_code,
        after_slot_code,
        before_slot_player_id,
        after_slot_player_id,
        player_id,
        selected_foreign,
        nation_id,
        before_current_team_id,
        after_current_team_id,
        before_active_team_id,
        after_active_team_id,
        league_id,
        default_team_id,
        (void*)status_ptr,
        status24,
        status25,
        status26,
        before_f61,
        after_f61,
        before_f62,
        after_f62,
        before_f65,
        after_f65,
        before_f68,
        after_f68,
        before_f1a,
        after_f1a,
        after_f3e,
        before_f06,
        after_f06,
        before_fec,
        after_fec,
        before_role800,
        after_role800,
        before_role802,
        after_role802,
        before_role804,
        after_role804,
        before_role806,
        after_role806,
        before_role808,
        after_role808,
        before_role80a,
        after_role80a,
        before_ef8,
        after_ef8,
        before_score_fe0,
        before_score_fe4,
        player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET) : 0,
        player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET) : 0,
        player_plausible ? kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET) : 0);
}

__declspec(noinline) uint8_t ootp_kbo_team_add_player_guard_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uintptr_t arg6,
    uintptr_t arg7,
    uintptr_t arg8)
{
    KBO_PROFILE_BEGIN(profile_team_add_guard_wrapper);
    uintptr_t caller_ptr = (uintptr_t)__builtin_return_address(0);
    HMODULE host_exe = GetModuleHandleA(NULL);
    uint32_t caller_rva = host_exe != NULL && caller_ptr >= (uintptr_t)host_exe
        ? (uint32_t)(caller_ptr - (uintptr_t)host_exe)
        : 0u;

    OotpKboTeamAddPlayerExFn original = g_kbo_team_add_player_guard_trampoline;
    if (original == NULL) {
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.no_original");
        return 0;
    }

    int team_readable = team_ptr != 0
        && memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES);
    int player_plausible = kbo_player_pointer_plausible(player_ptr);
    uint8_t* team = team_readable ? (uint8_t*)team_ptr : NULL;
    uint8_t* player = player_plausible ? (uint8_t*)player_ptr : NULL;
    uint32_t team_id = team_readable
        ? *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET)
        : 0u;
    int is_military_team = team_id != 0u && kbo_team_id_is_military_service_team(team_id);
    int amateur_generation_call = kbo_amateur_generation_team_add_caller(caller_rva);

    uint32_t before_current_team_id = 0u;
    uint32_t before_active_team_id = 0u;
    uint32_t before_original_team_id = 0u;
    if (player_plausible) {
        before_current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        before_active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
        if (memory_range_readable(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET, sizeof(uint32_t))) {
            before_original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
        }
    }

    if (is_military_team && kbo_military_team_add_player_should_block(team_ptr, player_ptr)) {
        kbo_log_foreign_team_add_trace(
            caller_rva,
            "military_blocked",
            0u,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.blocked");
        return 0;
    }

    if (amateur_generation_call && kbo_amateur_defer_team_add_if_generation(
            caller_rva,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8)) {
        kbo_log_foreign_team_add_trace(
            caller_rva,
            "amateur_deferred",
            1u,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.amateur_deferred");
        return 1;
    }

    if (!is_military_team
            && !amateur_generation_call
            && kbo_team_add_foreign_policy_should_block(
                team_ptr,
                player_ptr,
                team_id,
                before_current_team_id,
                before_active_team_id)) {
        kbo_log_foreign_team_add_trace(
            caller_rva,
            "foreign_policy_blocked",
            0u,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, "team_add_guard.foreign_policy_blocked");
        return 0;
    }

    if (!is_military_team
            && !amateur_generation_call
            && before_current_team_id != 0u
            && before_active_team_id != 0u) {
        KBO_PROFILE_BEGIN(profile_team_add_original);
        uint8_t result = original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original.fast_success" : "team_add_guard.original.fast_rejected");
        kbo_log_foreign_team_add_trace(
            caller_rva,
            result != 0u ? "fast_success" : "fast_rejected",
            result,
            team_ptr,
            player_ptr,
            arg3,
            arg4,
            arg5,
            arg6,
            arg7,
            arg8,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_guard_wrapper, result != 0u ? "team_add_guard.fast_success" : "team_add_guard.fast_rejected");
        return result;
    }

    uint32_t amateur_league_id = amateur_generation_call && team_readable
        ? kbo_team_add_cached_amateur_league_id(team)
        : 0u;
    uintptr_t effective_team_ptr = amateur_generation_call && amateur_league_id != 0u
        ? kbo_amateur_team_add_player_reroute_before_original(
            team_ptr,
            player_ptr,
            "team_add_player_before_original")
        : team_ptr;
    int amateur_pre_rerouted = effective_team_ptr != team_ptr;
    if (amateur_generation_call && player_plausible && team_readable) {
        uint32_t league_id = amateur_league_id;
        int16_t age = *(int16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        if (league_id != 0u && kbo_amateur_player_age_eligible(league_id, age)) {
            static volatile LONG amateur_caller_log_count = 0;
            LONG slot = InterlockedIncrement(&amateur_caller_log_count);
            if (slot <= 200 || kbo_team_add_amateur_verbose_log_enabled_cached()) {
                uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
                uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
                append_logf(
                    "amateur team_add caller trace #%ld caller_rva=0x%x player=%u league=%u age=%d original_team=%u rerouted=%d",
                    slot,
                    caller_rva,
                    player_id,
                    league_id,
                    (int)age,
                    team_id,
                    amateur_pre_rerouted);
            }
        }
    }

    KBO_PROFILE_BEGIN(profile_team_add_original);
    uint8_t result = original(effective_team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
    KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original.success" : "team_add_guard.original.rejected");
    kbo_log_foreign_team_add_trace(
        caller_rva,
        result != 0u ? (effective_team_ptr != team_ptr ? "rerouted_success" : "success") : (effective_team_ptr != team_ptr ? "rerouted_rejected" : "rejected"),
        result,
        effective_team_ptr,
        player_ptr,
        arg3,
        arg4,
        arg5,
        arg6,
        arg7,
        arg8,
        before_current_team_id,
        before_active_team_id,
        before_original_team_id);
    int retry_rejected_targets = kbo_team_add_retry_rejected_targets_enabled_cached();
    for (int amateur_retry = 0; result == 0u && amateur_pre_rerouted && retry_rejected_targets && amateur_retry < 4; amateur_retry++) {
        static volatile LONG fallback_log_count = 0;
        LONG fallback_slot = InterlockedIncrement(&fallback_log_count);
        uint32_t original_team_id = 0u;
        uint32_t effective_team_id = 0u;
        uint32_t player_id = 0u;
        if (memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            original_team_id = *(uint32_t*)((uint8_t*)team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        if (memory_range_readable((void*)effective_team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            effective_team_id = *(uint32_t*)((uint8_t*)effective_team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
        }
        if (kbo_player_pointer_plausible(player_ptr)) {
            player_id = *(uint32_t*)((uint8_t*)player_ptr + OOTP27_PLAYER_ID_OFFSET);
        }
        if (effective_team_id != 0u) {
            uint8_t* rejected_team = (uint8_t*)effective_team_ptr;
            uint32_t rejected_league_id = kbo_resolve_amateur_assignment_league_id_for_team_and_player(
                rejected_team,
                (uint8_t*)player_ptr);
            kbo_amateur_assignment_mark_rejected_target(rejected_league_id, effective_team_id);
        }
        if (fallback_slot <= 80) {
            append_logf(
                "amateur assignment reroute target rejected; retrying alternate player=%u original_team=%u rejected_team=%u attempt=%d",
                player_id,
                original_team_id,
                effective_team_id,
                amateur_retry + 1);
        }

        uintptr_t retry_team_ptr = kbo_amateur_team_add_player_reroute_before_original(
            team_ptr,
            player_ptr,
            "team_add_player_reroute_retry");
        if (retry_team_ptr == team_ptr || retry_team_ptr == effective_team_ptr) {
            break;
        }
        effective_team_ptr = retry_team_ptr;
        KBO_PROFILE_BEGIN(profile_team_add_original);
        result = original(effective_team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original_retry.success" : "team_add_guard.original_retry.rejected");
    }
    if (result == 0u && amateur_pre_rerouted) {
        KBO_PROFILE_BEGIN(profile_team_add_original);
        result = original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
        KBO_PROFILE_END(profile_team_add_original, result != 0u ? "team_add_guard.original_fallback.success" : "team_add_guard.original_fallback.rejected");
        if (result != 0u) {
            effective_team_ptr = team_ptr;
            amateur_pre_rerouted = 0;
        }
    }
    if (result != 0u && amateur_generation_call && amateur_league_id != 0u) {
        KBO_PROFILE_BEGIN(profile_team_add_amateur_assignment);
        if (amateur_pre_rerouted) {
            kbo_amateur_team_add_player_note_original_success(
                effective_team_ptr,
                player_ptr,
                "team_add_player_pre_rerouted_original_success",
                result);
        } else {
            kbo_amateur_team_add_player_note_original_success(
                team_ptr,
                player_ptr,
                "team_add_player_original_success",
                result);
        }
        KBO_PROFILE_END(profile_team_add_amateur_assignment, "team_add_guard.amateur_assignment_after_original");
    }
    if (result != 0u) {
        KBO_PROFILE_BEGIN(profile_team_add_fa_comp);
        kbo_team_add_player_record_fa_compensation_success(
            effective_team_ptr,
            player_ptr,
            before_current_team_id,
            before_active_team_id,
            before_original_team_id);
        KBO_PROFILE_END(profile_team_add_fa_comp, "team_add_guard.fa_comp_probe");
    }
    KBO_PROFILE_END(profile_team_add_guard_wrapper, result != 0u ? "team_add_guard.success" : "team_add_guard.original_rejected");
    return result;
}
