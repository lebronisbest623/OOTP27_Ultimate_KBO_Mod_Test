#include "../internal/team_add_player_guard_ai_roster_internal.h"

#include "../../../../bootstrap/abi/ootp_offsets.h"
#include "../../../../foreign/common/player_eval/foreign_waiver_player_eval.h"
#include "../../../../runtime_memory/runtime_memory.h"
#include "../../../lookup/team_lookup.h"

static int32_t kbo_ai_roster_context_read_score(uintptr_t player_ptr, uint32_t offset)
{
    if (player_ptr == 0u
            || !kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)(player_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(player_ptr + offset);
}

static void kbo_ai_roster_context_read_default_status(
    uint8_t* player,
    uint32_t* out_status24,
    uint32_t* out_status25,
    uint32_t* out_status26)
{
    if (out_status24 != NULL) { *out_status24 = 0u; }
    if (out_status25 != NULL) { *out_status25 = 0u; }
    if (out_status26 != NULL) { *out_status26 = 0u; }
    if (player == NULL || !memory_range_readable(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET, sizeof(uintptr_t))) {
        return;
    }

    uintptr_t status_ptr = *(uintptr_t*)(player + OOTP27_PLAYER_DEFAULT_STATUS_PTR_OFFSET);
    if (status_ptr == 0u || !memory_range_readable((void*)status_ptr, 0x29u)) {
        return;
    }

    uint8_t* status = (uint8_t*)status_ptr;
    if (out_status24 != NULL) { *out_status24 = status[0x24u]; }
    if (out_status25 != NULL) { *out_status25 = status[0x25u]; }
    if (out_status26 != NULL) { *out_status26 = status[0x26u]; }
}

static uint16_t kbo_ai_roster_context_u16(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(context_ptr + offset);
}

uintptr_t kbo_ai_roster_context_ptr(uintptr_t context_ptr, uintptr_t offset)
{
    if (context_ptr == 0u || !memory_range_readable((void*)(context_ptr + offset), sizeof(uintptr_t))) {
        return 0u;
    }
    return *(uintptr_t*)(context_ptr + offset);
}

uintptr_t kbo_ai_roster_context_slot_block(uintptr_t context_ptr, uint16_t slot)
{
    if (slot >= 64u) {
        return 0u;
    }
    return kbo_ai_roster_context_ptr(
        context_ptr,
        KBO_AI_ROSTER_CONTEXT_SLOT_BLOCK_TABLE_OFFSET + (uintptr_t)slot * sizeof(uintptr_t));
}

uint32_t kbo_ai_roster_context_slot_team_id(uintptr_t context_ptr, uint16_t slot)
{
    if (context_ptr == 0u || slot >= 64u) {
        return 0u;
    }

    uintptr_t addr = context_ptr + KBO_AI_ROSTER_CONTEXT_SLOT_TEAM_ID_TABLE_OFFSET
        + (uintptr_t)slot * sizeof(uint32_t);
    if (!memory_range_readable((void*)addr, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)addr;
}

uint32_t kbo_ai_roster_slot_code_at(uintptr_t slot_block_ptr, uint16_t target_slot)
{
    if (slot_block_ptr == 0u || target_slot > KBO_AI_ROSTER_SLOT_MAX) {
        return 0u;
    }

    uintptr_t addr = slot_block_ptr + KBO_AI_ROSTER_SLOT_CODE_BASE_OFFSET
        + (uintptr_t)target_slot * 8u;
    if (!memory_range_readable((void*)addr, sizeof(uint8_t))) {
        return 0u;
    }
    return (uint32_t)*(uint8_t*)addr;
}

uint32_t kbo_ai_roster_slot_player_at(uintptr_t slot_block_ptr, uint16_t target_slot)
{
    if (slot_block_ptr == 0u || target_slot > KBO_AI_ROSTER_SLOT_MAX) {
        return 0u;
    }

    uintptr_t addr = slot_block_ptr + KBO_AI_ROSTER_SLOT_PLAYER_BASE_OFFSET
        + (uintptr_t)target_slot * 8u;
    if (!memory_range_readable((void*)addr, sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)addr;
}

void kbo_ai_roster_flow_read_player(uintptr_t player_ptr, KboAiRosterFlowPlayerSnapshot* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->ptr = player_ptr;
    if (!kbo_player_pointer_plausible(player_ptr)
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    out->plausible = 1;
    out->player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    out->foreign = kbo_player_is_foreign_for_kbo_rights(player);
    out->nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    out->current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    out->active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    out->league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
    if (memory_range_readable(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        out->default_team_id = *(uint32_t*)(player + OOTP27_PLAYER_DEFAULT_TEAM_ID_OFFSET);
    }
    kbo_ai_roster_context_read_default_status(player, &out->status24, &out->status25, &out->status26);
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
    out->score_fe0 = kbo_ai_roster_context_read_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE0_OFFSET);
    out->score_fe4 = kbo_ai_roster_context_read_score(player_ptr, KBO_AI_ROSTER_SELECT_SCORE_FE4_OFFSET);
    out->overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    out->ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
}

void kbo_ai_roster_flow_read_context(uintptr_t context_ptr, KboAiRosterFlowContextSnapshot* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->primary_slot = kbo_ai_roster_context_u16(context_ptr, KBO_AI_ROSTER_CONTEXT_PRIMARY_SLOT_OFFSET);
    out->primary_target_slot = out->primary_slot < 64u
        ? kbo_ai_roster_context_u16(
            context_ptr,
            KBO_AI_ROSTER_CONTEXT_TARGET_SLOT_TABLE_OFFSET + (uintptr_t)out->primary_slot * sizeof(uint16_t))
        : 0u;
    out->primary_slot_block = kbo_ai_roster_context_slot_block(context_ptr, out->primary_slot);
    kbo_ai_roster_flow_read_player(
        kbo_ai_roster_context_ptr(context_ptr, KBO_AI_ROSTER_CONTEXT_SELECTED_PTR_OFFSET),
        &out->selected);
    kbo_ai_roster_flow_read_player(
        kbo_ai_roster_context_ptr(context_ptr, KBO_AI_ROSTER_CONTEXT_PTR528_OFFSET),
        &out->ptr528);
}
