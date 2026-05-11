#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

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

static OotpKboTeamAddPlayerExFn g_kbo_team_add_player_guard_trampoline = NULL;
static OotpKboTeamAddPlayerExFn g_kbo_roster_move_active_trace_trampoline = NULL;
static OotpKboTeamAddPlayerExFn g_kbo_roster_move_secondary_trace_trampoline = NULL;
static OotpKboTeamAddPlayerExFn g_kbo_roster_move_assignment_trace_trampoline = NULL;
static volatile LONG g_kbo_team_add_amateur_verbose_cached = -1;
static volatile LONG g_kbo_team_add_amateur_verbose_tick = 0;
static volatile LONG g_kbo_team_add_retry_rejected_cached = -1;
static volatile LONG g_kbo_team_add_retry_rejected_tick = 0;

#define KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX 512

typedef struct KboTeamAddAmateurLeagueCacheEntry {
    uintptr_t team_ptr;
    uint32_t league_id;
} KboTeamAddAmateurLeagueCacheEntry;

static KboTeamAddAmateurLeagueCacheEntry g_kbo_team_add_amateur_league_cache[KBO_TEAM_ADD_AMATEUR_LEAGUE_CACHE_MAX];
static volatile LONG g_kbo_team_add_amateur_league_cache_count = 0;
static volatile LONG g_kbo_team_add_amateur_league_cache_lock = 0;

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
