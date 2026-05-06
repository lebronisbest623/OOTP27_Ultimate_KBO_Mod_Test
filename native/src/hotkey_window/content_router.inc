static void kbo_build_hub_window_text(char* out, size_t out_size)
{
    switch (g_kbo_hub_selected_view) {
    case KBO_HUB_VIEW_MILITARY: kbo_build_military_service_window_text(out, out_size); return;
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: kbo_build_foreign_rights_window_text(out, out_size); return;
    case KBO_HUB_VIEW_ASIAN_QUOTA: kbo_build_foreign_policy_hub_text(out, out_size);    return;
    case KBO_HUB_VIEW_ASIAN_GAMES: kbo_build_asian_games_hub_text(out, out_size);       return;
    case KBO_HUB_VIEW_MOD_INFO: kbo_build_mod_info_hub_text(out, out_size);            return;
    case KBO_HUB_VIEW_SETTINGS: kbo_build_settings_hub_text(out, out_size);            return;
    default:                    kbo_build_mod_info_hub_text(out, out_size);             return;
    }
}
