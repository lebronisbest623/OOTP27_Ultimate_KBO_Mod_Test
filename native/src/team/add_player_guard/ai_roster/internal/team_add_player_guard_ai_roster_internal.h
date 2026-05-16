#ifndef KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_AI_ROSTER_INTERNAL_H_
#define KBOFIX_SRC_TEAM_ADD_PLAYER_GUARD_AI_ROSTER_INTERNAL_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

typedef uintptr_t (__fastcall *OotpKboAiRosterSelectFn)(
    uintptr_t context_ptr,
    int32_t slot_index,
    int32_t depth_hint);
typedef void (__fastcall *OotpKboAiRosterContextFlowFn)(
    uintptr_t context_ptr);
typedef void (__fastcall *OotpKboAiRosterApplySelectionFn)(
    uintptr_t context_ptr,
    int32_t slot_index,
    uintptr_t player_ptr,
    int32_t target_slot,
    int32_t roster_code);

#define KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET 0x108u
#define KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET 0x578u
#define KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET 0x4f8u
#define KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET 0x7b0u
#define KBO_AI_ROSTER_CONTEXT_TARGET_SLOT_TABLE_OFFSET 0x7bau
#define KBO_AI_ROSTER_CONTEXT_PTR528_OFFSET 0x528u
#define KBO_AI_ROSTER_SLOT_BLOCK_CANDIDATE_VECTOR_OFFSET 0x3ca8u
#define KBO_AI_ROSTER_SLOT_PLAYER_BASE_OFFSET 0x14d4u
#define KBO_AI_ROSTER_SLOT_CODE_BASE_OFFSET 0x14d8u
#define KBO_AI_ROSTER_SLOT_MAX 8
#define KBO_AI_ROSTER_SELECT_SCAN_LIMIT 256
#define KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET 0xfe0u
#define KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET 0xfe4u
#define KBO_AI_ROSTER_FOREIGN_F25_MIN 100u
#define KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT 16
#define KBO_AI_ROSTER_APPLY_RESCUE_SHIELD_MS 5000u

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
    uint32_t f25;
    uint32_t f62;
    uint32_t f65;
    int16_t f06;
    int32_t score_fe0;
    int32_t score_fe4;
    int16_t overall;
    int16_t talent;
    int16_t ratings;
} KboAiRosterForeignCandidateSummary;

typedef struct KboAiRosterFlowPlayerSnapshot {
    uintptr_t ptr;
    int plausible;
    uint32_t player_id;
    int foreign;
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
    uint16_t primary_target_slot;
    uintptr_t primary_slot_block;
    KboAiRosterFlowPlayerSnapshot selected;
    KboAiRosterFlowPlayerSnapshot ptr528;
} KboAiRosterFlowContextSnapshot;

typedef struct KboAiRosterApplyRescueSlot {
    uintptr_t context_ptr;
    uintptr_t slot_block_ptr;
    uintptr_t player_ptr;
    uint32_t player_id;
    uint16_t slot_index;
    uint16_t target_slot;
    DWORD tick;
} KboAiRosterApplyRescueSlot;

void kbo_ai_roster_record_foreign_apply_rescue(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uintptr_t player_ptr,
    uint32_t player_id,
    uint16_t slot_index,
    uint16_t target_slot);
int kbo_ai_roster_recent_foreign_apply_rescue_match(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uint16_t target_slot,
    uint32_t player_id,
    DWORD* out_age_ms);
uintptr_t kbo_ai_roster_context_ptr(uintptr_t context_ptr, uintptr_t offset);
uintptr_t kbo_ai_roster_context_slot_block(uintptr_t context_ptr, uint16_t slot);
uint32_t kbo_ai_roster_context_slot_team_id(uintptr_t context_ptr, uint16_t slot);
uint32_t kbo_ai_roster_slot_code_at(uintptr_t slot_block_ptr, uint16_t target_slot);
uint32_t kbo_ai_roster_slot_player_at(uintptr_t slot_block_ptr, uint16_t target_slot);
void kbo_ai_roster_flow_read_player(uintptr_t player_ptr, KboAiRosterFlowPlayerSnapshot* out);
void kbo_ai_roster_flow_read_context(uintptr_t context_ptr, KboAiRosterFlowContextSnapshot* out);
int32_t kbo_pointer_vector_count(uintptr_t vector_ptr);
int kbo_ai_roster_foreign_apply_rescue_enabled(void);
int kbo_ai_roster_minor_foreign_callup_allows(
    int32_t team_arg,
    uint8_t* player,
    uint32_t* out_team_id,
    int* out_callup_allowed);
uintptr_t kbo_ai_roster_choose_source_select_rescue_candidate(
    uintptr_t source_vector_ptr,
    int32_t source_count,
    uintptr_t native_result_ptr,
    int32_t* out_source_index,
    uint32_t* out_active_team_id,
    int64_t* out_score,
    KboAiRosterForeignCandidateSummary* out_summary);
int kbo_ai_roster_context_flow_apply_rescue(
    uintptr_t context_ptr,
    const KboAiRosterFlowContextSnapshot* before,
    const KboAiRosterFlowContextSnapshot* after,
    OotpKboAiRosterApplySelectionFn apply_selection_trampoline);

#endif
