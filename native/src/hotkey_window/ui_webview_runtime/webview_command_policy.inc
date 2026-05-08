static int kbo_webview_team_action_allowed(uint32_t team_id, const char* source)
{
    if (kbo_get_allow_all_ui_team_actions_setting()) {
        return 1;
    }

    if (team_id != 0 && kbo_team_is_human_controlled(team_id, source)) {
        return 1;
    }

    append_logf(
        "webview team action blocked reason=team_not_human_controlled source=%s team=%u",
        source != NULL ? source : "",
        team_id);
    return 0;
}

