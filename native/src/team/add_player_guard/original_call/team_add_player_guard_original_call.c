#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../lookup/team_lookup.h"
#include "../team_add_player_guard.h"
#include "team_add_player_guard_original_call.h"

static KboTeamAddPlayerOriginalFn g_kbo_team_add_player_guard_trampoline = NULL;

KboTeamAddPlayerOriginalFn kbo_team_add_player_guard_get_original(void)
{
    return g_kbo_team_add_player_guard_trampoline;
}

void kbo_set_team_add_player_guard_trampoline(void* trampoline)
{
    g_kbo_team_add_player_guard_trampoline = (KboTeamAddPlayerOriginalFn)(uintptr_t)trampoline;
}

void kbo_clear_team_add_player_guard_trampoline(void)
{
    g_kbo_team_add_player_guard_trampoline = NULL;
}

int kbo_team_add_original_args_readable(uintptr_t team_ptr, uintptr_t player_ptr)
{
    return team_ptr != 0
        && player_ptr != 0
        && memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)
        && kbo_player_pointer_plausible(player_ptr);
}

void kbo_team_add_log_skipped_bad_original_args(
    uint32_t caller_rva,
    const char* stage,
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    uintptr_t original_team_ptr)
{
    static volatile LONG bad_args_log_count = 0;
    LONG slot = InterlockedIncrement(&bad_args_log_count);
    if (slot > 120) {
        return;
    }
    append_logf(
        "team_add guard: skipped original bad_args stage=%s caller_rva=0x%x team_ptr=%p player_ptr=%p original_team_ptr=%p team_readable=%d player_plausible=%d",
        stage != NULL ? stage : "unknown",
        caller_rva,
        (void*)team_ptr,
        (void*)player_ptr,
        (void*)original_team_ptr,
        team_ptr != 0 && memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES),
        kbo_player_pointer_plausible(player_ptr));
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
    KboTeamAddPlayerOriginalFn original = g_kbo_team_add_player_guard_trampoline;
    if (original == NULL) {
        return 0;
    }
    if (!kbo_team_add_original_args_readable(team_ptr, player_ptr)) {
        kbo_team_add_log_skipped_bad_original_args(
            0u,
            "direct_call",
            team_ptr,
            player_ptr,
            team_ptr);
        return 0;
    }
    return original(team_ptr, player_ptr, arg3, arg4, arg5, arg6, arg7, arg8);
}
