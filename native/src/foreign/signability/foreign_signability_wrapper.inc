/* Player/team signability hook wrapper. Included from native/KBOFix.c. */

typedef int (__fastcall *OotpPlayerTeamSignabilityFn)(void* player, int32_t team_id, uint16_t year_hint);

__declspec(noinline) int ootp_kbo_player_team_signability_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uint16_t year_hint,
    uintptr_t original_func_ptr)
{
    OotpPlayerTeamSignabilityFn original_func = (OotpPlayerTeamSignabilityFn)original_func_ptr;

    if (kbo_fast_block_fa_candidate_before_original(player_ptr, team_id, "signability", NULL)) {
        return 0; /* OOTP signability enum: 0 = Impossible. */
    }

    int original_signability = 0;
    if (original_func != NULL) {
        original_signability = original_func((void*)player_ptr, team_id, year_hint);
    }
    uintptr_t caller_rva = 0;
    HMODULE exe = GetModuleHandleA(NULL);
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    if (exe != NULL && caller > (uintptr_t)exe) {
        caller_rva = caller - (uintptr_t)exe;
    }
    return kbo_enforce_foreign_waiver_signability(player_ptr, team_id, year_hint, original_signability, caller_rva);
}
