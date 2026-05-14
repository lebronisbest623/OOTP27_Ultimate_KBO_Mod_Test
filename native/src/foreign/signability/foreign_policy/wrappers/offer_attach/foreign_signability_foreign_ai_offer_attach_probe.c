#include "../../internal/foreign_signability_internal.h"
#include "../../../../../build_verify/build_verify.h"

#define KBO_OFFER_READABLE_BYTES 0xE0u
#define KBO_OFFER_MAJOR_FLAG_OFFSET 0x08u
#define KBO_OFFER_MINOR_FLAG_OFFSET 0x0Au
#define KBO_OFFER_SALARY_PRIMARY_OFFSET 0x24u
#define KBO_OFFER_TEAM_ID_OFFSET 0x28u
#define KBO_OFFER_TEAM_ORG_ID_OFFSET 0x30u
#define KBO_OFFER_SALARY_FIRST_YEAR_OFFSET 0x38u
#define KBO_OFFER_YEAR_COUNT_OFFSET 0x74u
#define KBO_OFFER_FLAG_AA_OFFSET 0xAAu
#define KBO_OFFER_FLAG_AB_OFFSET 0xABu
#define KBO_OFFER_FLAG_AC_OFFSET 0xACu
#define KBO_OFFER_TYPE_AE_OFFSET 0xAEu
#define KBO_OFFER_TYPE_B0_OFFSET 0xB0u
#define KBO_OFFER_FLAG_D0_OFFSET 0xD0u
#define KBO_OFFER_FLAG_D8_OFFSET 0xD8u

#define KBO_PLAYER_SELECTED_OFFER_ID_OFFSET 0x1750u

typedef void (__fastcall *KboOotpForeignAiOfferAttachFn)(uintptr_t player_ptr, uintptr_t offer_slot_ptr);
typedef uintptr_t (__fastcall *KboOotpForeignAiOfferBuildFn)(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t zero_arg,
    uintptr_t flag_ptr,
    uint8_t stack_flag);
typedef uint8_t (__fastcall *KboOotpForeignAiOfferFinalGateFn)(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary);

int kbo_apply_foreign_reserve_demand_floor(uintptr_t player_ptr, const char* source);
void kbo_prepare_foreign_fa_offer_demand_baseline(uintptr_t player_ptr, const char* source);
void kbo_restore_foreign_fa_demand_salary_ladder(const char* source);

static uint32_t kbo_foreign_ai_offer_attach_caller_rva(uintptr_t caller_return_ptr)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL || caller_return_ptr < (uintptr_t)exe) {
        return 0u;
    }
    return (uint32_t)(caller_return_ptr - (uintptr_t)exe);
}

static uint8_t kbo_offer_read_u8(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0 || offset + sizeof(uint8_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(uint8_t))) {
        return 0u;
    }
    return *(uint8_t*)(offer_ptr + offset);
}

static uint16_t kbo_offer_read_u16(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0 || offset + sizeof(uint16_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(uint16_t))) {
        return 0u;
    }
    return *(uint16_t*)(offer_ptr + offset);
}

static int32_t kbo_offer_read_i32(uintptr_t offer_ptr, uint32_t offset)
{
    if (offer_ptr == 0 || offset + sizeof(int32_t) > KBO_OFFER_READABLE_BYTES
            || !memory_range_readable((void*)(offer_ptr + offset), sizeof(int32_t))) {
        return 0;
    }
    return *(int32_t*)(offer_ptr + offset);
}

static uint32_t kbo_offer_probe_team_id_from_ptr(uintptr_t team_ptr)
{
    if (team_ptr == 0
            || !memory_range_readable((void*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET), sizeof(uint32_t))) {
        return 0u;
    }
    return *(uint32_t*)(team_ptr + OOTP27_KBO_TEAM_ID_OFFSET);
}

static uint32_t kbo_offer_probe_team_id_from_offer(uintptr_t offer_ptr)
{
    int32_t team_id = kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET);
    return team_id > 0 ? (uint32_t)team_id : 0u;
}

static void* kbo_offer_probe_resolve_rva(uint32_t rva)
{
    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return NULL;
    }
    return kbo_resolve_build_specific_rva_ptr(exe, rva);
}

static int kbo_foreign_ai_offer_attach_should_log(
    uint8_t* player,
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_holder_team_id)
{
    if (out_holder_team_id != NULL) {
        *out_holder_team_id = 0u;
    }
    if (player == NULL || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t holder_team_id = 0u;
    int has_holder = player_id != 0u
        && today != 0u
        && kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u;
    if (out_holder_team_id != NULL && has_holder) {
        *out_holder_team_id = holder_team_id;
    }

    return has_holder || kbo_player_is_foreign_for_kbo_rights(player);
}

static void kbo_log_foreign_ai_offer_attach(
    uintptr_t player_ptr,
    uintptr_t offer_slot_ptr,
    uintptr_t caller_return_ptr)
{
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)
            || offer_slot_ptr == 0
            || !memory_range_readable((void*)offer_slot_ptr, sizeof(uintptr_t))) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t holder_team_id = 0u;
    if (!kbo_foreign_ai_offer_attach_should_log(player, player_id, today, &holder_team_id)) {
        return;
    }

    static LONG log_count = 0;
    LONG slot = InterlockedIncrement(&log_count);
    if (slot > 600) {
        return;
    }

    uintptr_t offer_ptr = *(uintptr_t*)offer_slot_ptr;
    uint32_t caller_rva = kbo_foreign_ai_offer_attach_caller_rva(caller_return_ptr);
    uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
    uint32_t active_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET);
    uint32_t original_team_id = *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET);
    uint32_t nation_id = *(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET);
    uint8_t position_group = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_GROUP_OFFSET);
    uint8_t position_role = *(uint8_t*)(player + OOTP27_PLAYER_POSITION_ROLE_OFFSET);
    int32_t demand_salary = *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET);
    int32_t score = kbo_foreign_waiver_value_score(player);
    int32_t selected_offer_id = *(int32_t*)(player + KBO_PLAYER_SELECTED_OFFER_ID_OFFSET);

    append_logf(
        "foreign ai offer attach probe #%ld caller_rva=0x%x player=%u nation=%u pos_group=%u pos_role=%u holder_team=%u today=%u current=%u active=%u original=%u demand=%d score=%d selected_offer=%d offer=%p offer_team=%d offer_org=%d salary24=%d salary38=%d years=%u flags_aa=%u ab=%u ac=%u d0=%u d8=%u type_ae=%u type_b0=%u",
        slot,
        caller_rva,
        player_id,
        nation_id,
        (uint32_t)position_group,
        (uint32_t)position_role,
        holder_team_id,
        today,
        current_team_id,
        active_team_id,
        original_team_id,
        demand_salary,
        score,
        selected_offer_id,
        (void*)offer_ptr,
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AA_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AB_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AC_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_D0_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_D8_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_AE_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_B0_OFFSET));
}

static int kbo_foreign_ai_offer_decision_should_log(
    uint8_t* player,
    uint32_t player_id,
    uint32_t today,
    uint32_t* out_holder_team_id)
{
    if (out_holder_team_id != NULL) {
        *out_holder_team_id = 0u;
    }
    if (player == NULL
            || player_id == 0u
            || today == 0u
            || !memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        return 0;
    }

    uint32_t holder_team_id = 0u;
    int has_holder = kbo_find_active_foreign_waiver_holder(player_id, today, &holder_team_id)
        && holder_team_id != 0u;
    if (out_holder_team_id != NULL && has_holder) {
        *out_holder_team_id = holder_team_id;
    }
    if (has_holder) {
        return 1;
    }

    return read_kbo_localappdata_flag_file("enable_kbo_custom_foreign_offer_logs.txt")
        && kbo_player_is_foreign_for_kbo_rights(player);
}

static void kbo_log_foreign_ai_offer_build(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t flag_ptr,
    uintptr_t offer_ptr)
{
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t holder_team_id = 0u;
    if (!kbo_foreign_ai_offer_decision_should_log(player, player_id, today, &holder_team_id)) {
        return;
    }

    static LONG build_log_count = 0;
    LONG slot = InterlockedIncrement(&build_log_count);
    if (slot > 1000) {
        return;
    }

    append_logf(
        "foreign ai offer build probe #%ld player=%u holder_team=%u today=%u team_arg=%d current=%u active=%u original=%u demand=%d score=%d flag_ptr=%p flag_value=%u offer=%p offer_major=%u offer_minor=%u offer_team=%d offer_org=%d salary24=%d salary38=%d years=%u flags_aa=%u ab=%u ac=%u type_ae=%u type_b0=%u",
        slot,
        player_id,
        holder_team_id,
        today,
        team_id,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET),
        kbo_foreign_waiver_value_score(player),
        (void*)flag_ptr,
        flag_ptr != 0 && memory_range_readable((void*)flag_ptr, sizeof(uint8_t)) ? (uint32_t)*(uint8_t*)flag_ptr : 0u,
        (void*)offer_ptr,
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AA_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AB_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AC_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_AE_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_B0_OFFSET));
}

static void kbo_log_foreign_ai_offer_final_gate(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr,
    uint8_t result)
{
    if (player_ptr == 0
            || !memory_range_readable((void*)player_ptr, OOTP27_PLAYER_SCAN_BYTES)) {
        return;
    }

    uint8_t* player = (uint8_t*)player_ptr;
    uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
    uint32_t today = 0u;
    if (!kbo_get_foreign_waiver_current_yyyymmdd(&today)) {
        today = 0u;
    }

    uint32_t holder_team_id = 0u;
    if (!kbo_foreign_ai_offer_decision_should_log(player, player_id, today, &holder_team_id)) {
        return;
    }

    static LONG gate_log_count = 0;
    LONG slot = InterlockedIncrement(&gate_log_count);
    if (slot > 1000) {
        return;
    }

    append_logf(
        "foreign ai offer final gate probe #%ld result=%u player=%u holder_team=%u today=%u team=%u salary_arg=%d current=%u active=%u original=%u demand=%d score=%d offer=%p offer_major=%u offer_minor=%u offer_team=%d offer_org=%d salary24=%d salary38=%d years=%u flags_aa=%u ab=%u ac=%u type_ae=%u type_b0=%u",
        slot,
        (uint32_t)result,
        player_id,
        holder_team_id,
        today,
        kbo_offer_probe_team_id_from_ptr(team_ptr),
        salary,
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET),
        *(uint32_t*)(player + OOTP27_PLAYER_ORIGINAL_TEAM_ID_OFFSET),
        *(int32_t*)(player + OOTP27_PLAYER_FA_DEMAND_SALARY_OFFSET),
        kbo_foreign_waiver_value_score(player),
        (void*)offer_ptr,
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MAJOR_FLAG_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_MINOR_FLAG_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_TEAM_ORG_ID_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_PRIMARY_OFFSET),
        kbo_offer_read_i32(offer_ptr, KBO_OFFER_SALARY_FIRST_YEAR_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_YEAR_COUNT_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AA_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AB_OFFSET),
        (uint32_t)kbo_offer_read_u8(offer_ptr, KBO_OFFER_FLAG_AC_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_AE_OFFSET),
        (uint32_t)kbo_offer_read_u16(offer_ptr, KBO_OFFER_TYPE_B0_OFFSET));
}

__declspec(noinline) void ootp_kbo_foreign_ai_offer_attach_probe_wrapper(
    uintptr_t player_ptr,
    uintptr_t offer_slot_ptr,
    uintptr_t caller_return_ptr,
    uintptr_t original_func_ptr)
{
    KboOotpForeignAiOfferAttachFn original_func = (KboOotpForeignAiOfferAttachFn)original_func_ptr;
    uintptr_t offer_ptr = 0;
    if (offer_slot_ptr != 0
            && memory_range_readable((void*)offer_slot_ptr, sizeof(uintptr_t))) {
        offer_ptr = *(uintptr_t*)offer_slot_ptr;
    }
    if (original_func != NULL) {
        original_func(player_ptr, offer_slot_ptr);
    }
    if (read_kbo_localappdata_flag_file("enable_foreign_ai_roster_research_hooks.txt")
            || read_kbo_localappdata_flag_file("enable_kbo_foreign_ai_offer_attach_probe.txt")) {
        kbo_log_foreign_ai_offer_attach(player_ptr, offer_slot_ptr, caller_return_ptr);
    }
}

__declspec(noinline) uintptr_t ootp_kbo_foreign_ai_offer_build_probe_wrapper(
    uintptr_t player_ptr,
    int32_t team_id,
    uintptr_t zero_arg,
    uintptr_t flag_ptr)
{
    KboOotpForeignAiOfferBuildFn original_func =
        (KboOotpForeignAiOfferBuildFn)kbo_offer_probe_resolve_rva(OOTP27_AI_FA_OFFER_BUILD_FUNC_RVA);

    uintptr_t offer_ptr = 0;
    if (original_func != NULL) {
        kbo_apply_foreign_reserve_demand_floor(player_ptr, "ai_offer_build");
        kbo_prepare_foreign_fa_offer_demand_baseline(player_ptr, "ai_offer_build");
        offer_ptr = original_func(player_ptr, team_id, zero_arg, flag_ptr, 0u);
        kbo_restore_foreign_fa_demand_salary_ladder("ai_offer_build");
    }
    if (read_kbo_localappdata_flag_file("enable_foreign_ai_roster_research_hooks.txt")
            || read_kbo_localappdata_flag_file("enable_kbo_foreign_ai_offer_attach_probe.txt")) {
        kbo_log_foreign_ai_offer_build(player_ptr, team_id, flag_ptr, offer_ptr);
    }
    return offer_ptr;
}

__declspec(noinline) uint8_t ootp_kbo_foreign_ai_offer_final_gate_probe_wrapper(
    uintptr_t team_ptr,
    uintptr_t player_ptr,
    int32_t salary,
    uintptr_t offer_ptr)
{
    KboOotpForeignAiOfferFinalGateFn original_func =
        (KboOotpForeignAiOfferFinalGateFn)kbo_offer_probe_resolve_rva(OOTP27_AI_FA_OFFER_FINAL_GATE_FUNC_RVA);

    uint8_t result = 0u;
    if (original_func != NULL) {
        result = original_func(team_ptr, player_ptr, salary);
    }
    if (read_kbo_localappdata_flag_file("enable_foreign_ai_roster_research_hooks.txt")
            || read_kbo_localappdata_flag_file("enable_kbo_foreign_ai_offer_attach_probe.txt")) {
        kbo_log_foreign_ai_offer_final_gate(team_ptr, player_ptr, salary, offer_ptr, result);
    }
    return result;
}
