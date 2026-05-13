#include "../../hotkey_window_webview_internal.h"
#include "../../../hotkey_window_domain_contract.h"

int kbo_webview_handle_event_and_fa_command(const char* cmd)
{

    if (strncmp(cmd, "agames/", 7) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 7);
        if (subview >= 0 && subview < KBO_HUB_AGAMES_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_ASIAN_GAMES;
            g_kbo_hub_selected_agames_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa/", 3) == 0) {
        if (!kbo_hub_selected_league_is_kbo()) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_MOD_INFO;
            g_kbo_hub_open_dropdown = 0;
            kbo_webview_navigate_current();
            return 1;
        }
        int subview = atoi(cmd + 3);
        if (subview >= 0 && subview < KBO_HUB_FA_SUBVIEW_COUNT) {
            g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
            g_kbo_hub_selected_fa_subview = subview;
            g_kbo_hub_open_dropdown = 0;
        }
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/select/", 15) == 0) {
        const char* text = cmd + 15;
        uint32_t fa_player_id = (uint32_t)strtoul(text, NULL, 10);
        const char* slash = strchr(text, '/');
        uint32_t selected_player_id = slash != NULL ? (uint32_t)strtoul(slash + 1, NULL, 10) : 0u;
        if (fa_player_id != 0u && selected_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_original_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_manual_compensation_select")) {
                kbo_manual_select_fa_compensation_player(fa_player_id, selected_player_id, "hub_manual_compensation_select");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/cash-only/", 18) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 18, NULL, 10);
        if (fa_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_original_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_cash_only_select")) {
                kbo_manual_select_fa_compensation_cash_only(fa_player_id, "hub_cash_only_select");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/submit/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            uint32_t action_team_id = kbo_fa_compensation_signing_team_for_player(fa_player_id);
            if (kbo_webview_team_action_allowed(action_team_id, "hub_protected_list_submit")) {
                kbo_manual_submit_fa_compensation_protected_list(fa_player_id, "hub_protected_list_submit");
            }
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    if (strncmp(cmd, "fa-comp/detail/", 15) == 0) {
        uint32_t fa_player_id = (uint32_t)strtoul(cmd + 15, NULL, 10);
        if (fa_player_id != 0u) {
            g_kbo_hub_selected_fa_compensation_player_id = fa_player_id;
        }
        g_kbo_hub_selected_view = KBO_HUB_VIEW_FA_CASES;
        g_kbo_hub_selected_fa_subview = KBO_HUB_FA_SUBVIEW_COMPENSATION;
        g_kbo_hub_open_dropdown = 0;
        kbo_webview_navigate_current();
        return 1;
    }
    return 0;
}
