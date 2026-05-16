#include "capture/player_hover_manager_probe_internal.h"
#include "../../../bootstrap/profiling/profiler.h"

uintptr_t kbo_player_hover_manager_ptr(void)
{
    return (uintptr_t)g_kbo_player_hover_manager_ptr;
}

void kbo_set_player_tooltip_text_append_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_text_append_original = original_func_ptr;
}

void kbo_set_player_tooltip_string_format_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_string_format_original = original_func_ptr;
}

void kbo_set_player_tooltip_rating_common_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_rating_common_original = original_func_ptr;
}

void kbo_set_player_tooltip_rating_panel_ctor_original(uintptr_t original_func_ptr)
{
    g_kbo_player_tooltip_rating_panel_ctor_original = original_func_ptr;
}

static int kbo_player_hover_active_now(void)
{
    if (InterlockedCompareExchange(&g_kbo_player_hover_active, 0, 0) == 0) {
        return 0;
    }

    ULONGLONG now = GetTickCount64();
    LONG64 until = InterlockedCompareExchange64(&g_kbo_player_hover_active_until_ms, 0, 0);
    if ((LONG64)now <= until) {
        return 1;
    }

    InterlockedExchange(&g_kbo_player_hover_active, 0);
    return 0;
}

void kbo_clear_ootp_player_hover_popup(uint32_t player_id)
{
    LONG active_player = InterlockedCompareExchange(&g_kbo_player_hover_active_player, 0, 0);
    if (player_id == 0u || active_player == 0 || active_player == (LONG)player_id) {
        InterlockedExchange(&g_kbo_player_hover_active, 0);
        InterlockedExchange(&g_kbo_player_hover_active_player, 0);
        InterlockedExchange64(&g_kbo_player_hover_active_until_ms, 0);
    }
}

int kbo_show_ootp_player_hover_popup(uint32_t player_id, int screen_x, int screen_y)
{
    uintptr_t manager = kbo_player_hover_manager_ptr();
    if (manager == 0u || !memory_range_readable((uint8_t*)manager, 0x98u)) {
        kbo_log_runtimef(
            "KBO player hover popup skipped reason=manager_unavailable player=%u manager=%p",
            player_id,
            (void*)manager);
        return 0;
    }

    HMODULE exe = GetModuleHandleA(NULL);
    if (exe == NULL) {
        return 0;
    }

    OotpOperatorNewFn ootp_new =
        (OotpOperatorNewFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_UI_OPERATOR_NEW_RVA);
    OotpPlayerTooltipFactoryFn factory =
        (OotpPlayerTooltipFactoryFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_FACTORY_RVA);
    OotpPlayerTooltipRenderFn render =
        (OotpPlayerTooltipRenderFn)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_RENDER_RVA);
    void** current_tooltip =
        (void**)kbo_resolve_build_specific_rva_ptr(exe, OOTP27_PLAYER_TOOLTIP_CURRENT_GLOBAL_RVA);
    if (ootp_new == NULL || factory == NULL || render == NULL || current_tooltip == NULL) {
        kbo_log_runtimef(
            "KBO player hover popup skipped reason=resolve_failed player=%u new=%p factory=%p render=%p current=%p",
            player_id,
            (void*)ootp_new,
            (void*)factory,
            (void*)render,
            (void*)current_tooltip);
        return 0;
    }

    void* vtable = *(void**)manager;
    if (vtable == NULL || !memory_range_readable((uint8_t*)vtable, 0x18u)) {
        kbo_log_runtimef(
            "KBO player hover popup skipped reason=manager_vtable_unreadable player=%u manager=%p vtable=%p",
            player_id,
            (void*)manager,
            vtable);
        return 0;
    }

    OotpPlayerTooltipAttachFn attach = *(OotpPlayerTooltipAttachFn*)((uint8_t*)vtable + 0x08u);
    if (attach == NULL) {
        kbo_log_runtimef("KBO player hover popup skipped reason=attach_unavailable player=%u", player_id);
        return 0;
    }

    void* raw = ootp_new(0xD20u);
    if (raw == NULL) {
        kbo_log_runtimef("KBO player hover popup skipped reason=allocation_failed player=%u", player_id);
        return 0;
    }

    void* tooltip = factory(
        raw,
        player_id,
        (uint32_t)screen_y,
        (uint32_t)screen_x,
        NULL,
        0u);
    if (tooltip == NULL) {
        kbo_log_runtimef("KBO player hover popup skipped reason=factory_failed player=%u raw=%p", player_id, raw);
        return 0;
    }

    *current_tooltip = tooltip;
    render(tooltip, 8u);
    attach(manager, tooltip, 1u);
    InterlockedExchange(&g_kbo_player_hover_active_player, (LONG)player_id);
    InterlockedExchange64(&g_kbo_player_hover_active_until_ms, (LONG64)(GetTickCount64() + 1800u));
    InterlockedExchange(&g_kbo_player_hover_active, 1);

    kbo_log_runtimef(
        "KBO player hover popup shown player=%u tooltip=%p manager=%p x=%d y=%d",
        player_id,
        tooltip,
        (void*)manager,
        screen_x,
        screen_y);
    return 1;
}

__declspec(noinline) int ootp_kbo_player_hover_manager_probe_wrapper(
    uintptr_t manager_ptr,
    uintptr_t original_func_ptr)
{
    KBO_HOOK_PROFILE_BEGIN(profile_hook);
    if (kbo_player_hover_active_now()) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.player_hover_manager", 5);
    }

    if (manager_ptr != 0u) {
        PVOID previous = InterlockedExchangePointer(
            &g_kbo_player_hover_manager_ptr,
            (PVOID)manager_ptr);
        if ((uintptr_t)previous != manager_ptr) {
            LONG count = InterlockedIncrement(&g_kbo_player_hover_manager_log_count);
            if (count <= 8) {
                kbo_log_runtimef("KBO player hover manager captured manager=%p", (void*)manager_ptr);
            }
        }
    }

    OotpPlayerHoverManagerFn original = (OotpPlayerHoverManagerFn)original_func_ptr;
    if (original == NULL) {
        KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.player_hover_manager", 0);
    }
    KBO_HOOK_PROFILE_PAUSE(profile_hook);
    int result = original(manager_ptr);
    KBO_HOOK_PROFILE_RESUME(profile_hook);
    KBO_HOOK_PROFILE_RETURN(profile_hook, "ui.player_hover_manager", result);
}
