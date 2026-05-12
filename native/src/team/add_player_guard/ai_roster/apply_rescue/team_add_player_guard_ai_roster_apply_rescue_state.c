#include "../internal/team_add_player_guard_ai_roster_internal.h"

static KboAiRosterApplyRescueSlot g_kbo_ai_roster_apply_rescue_slots[KBO_AI_ROSTER_APPLY_RESCUE_SLOT_COUNT];
static volatile LONG g_kbo_ai_roster_apply_rescue_slot_next = 0;

void kbo_ai_roster_record_foreign_apply_rescue(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uintptr_t player_ptr,
    uint32_t player_id,
    uint16_t slot_index,
    uint16_t target_slot)
{
    if (slot_block_ptr == 0u || player_id == 0u || slot_index >= 64u || target_slot > KBO_AI_ROSTER_SLOT_MAX) {
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

int kbo_ai_roster_recent_foreign_apply_rescue_match(
    uintptr_t context_ptr,
    uintptr_t slot_block_ptr,
    uint16_t target_slot,
    uint32_t player_id,
    DWORD* out_age_ms)
{
    if (out_age_ms != NULL) {
        *out_age_ms = 0u;
    }
    if (slot_block_ptr == 0u || target_slot > KBO_AI_ROSTER_SLOT_MAX || player_id == 0u) {
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
