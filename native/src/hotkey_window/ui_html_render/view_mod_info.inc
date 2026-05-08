typedef struct KboModRuntimeFlagSetting {
    const char* key;
    const char* label;
    int enabled_value;
    int default_enabled;
    const char* companion_enable_key;
} KboModRuntimeFlagSetting;

static const KboModRuntimeFlagSetting KBO_MOD_RUNTIME_FLAG_SETTINGS[] = {
    { "enable_experimental_runtime_hooks", "Experimental runtime hooks", 1, 1, NULL },
    { "enable_foreign_waiver_ai", "Foreign waiver AI", 1, 1, NULL },
    { "enable_foreign_waiver_background_scanner", "Foreign waiver background scanner", 1, 1, NULL },
    { "disable_foreign_waiver_legacy_auto_detector", "Foreign waiver legacy detector", 0, 1, NULL },
    { "disable_kbo_foreign_trade_check_patch", "Foreign trade quota guard", 0, 1, "enable_kbo_foreign_trade_check_patch" },
    { "disable_kbo_custom_foreign_policy", "Custom foreign policy", 0, 1, NULL },
    { "disable_foreign_injury_replacement", "Foreign injury replacement", 0, 1, NULL },
    { "disable_intl_established_fa_generation_filter", "International FA generation filter", 0, 1, NULL },
    { "disable_kbo_fa_compensation", "FA compensation", 0, 1, NULL },
    { "disable_kbo_salary_arbitration_no_withdraw_patch", "Salary arbitration no-withdraw", 0, 1, NULL },
    { "disable_kbo_no_minor_contract_experimental_patch", "No minor-contract patch", 0, 1, NULL },
    { "disable_kbo_sangmu_fa_block_core", "Military team FA block", 0, 1, NULL },
    { "disable_kbo_military_team_add_guard_patch", "Military team add guard", 0, 1, NULL },
    { "disable_amateur_assignment_reroute", "Amateur reputation assignment", 0, 1, NULL },
    { "enable_single_division_allstar_runtime_patches", "Single-division All-Star runtime", 1, 1, NULL },
    { "enable_single_division_allstar_voting_hook", "Single-division All-Star voting", 1, 1, NULL },
    { "enable_single_division_allstar_events", "Single-division All-Star events", 1, 1, NULL },
    { "enable_single_division_allstar_settings_patch", "Single-division All-Star settings", 1, 1, NULL },
    { "disable_kbo_fa_salary_opening_day_snapshot", "FA salary opening-day snapshot", 0, 1, NULL },
    { "disable_kbo_fa_salary_opening_day_phase_hook", "FA salary phase hook", 0, 1, NULL },
    { "enable_fa_requalification", "FA requalification", 1, 0, NULL },
    { "disable_kbo_ai_fa_status_candidate_insert_hook", "AI FA candidate hook", 0, 1, NULL },
    { "enable_kbo_ai_fa_fallback_patch", "AI FA fallback patch", 1, 1, NULL },
    { "enable_kbo_fa_signability_hooks", "Legacy FA signability hooks", 1, 0, NULL },
    { "enable_kbo_player_team_signability_patch", "Player-team signability patch", 1, 1, NULL },
    { "enable_kbo_offer_eligibility_patch", "Offer eligibility patch", 1, 1, NULL },
    { "disable_kbo_submit_offer_probe_patch", "Submit-offer probe", 0, 1, "enable_kbo_submit_offer_probe_patch" },
    { "disable_kbo_foreign_signing_branch_patch", "Foreign signing branch hook", 0, 1, NULL },
    { "disable_kbo_runtime_roster_marker_guard", "Runtime roster marker guard", 0, 1, NULL },
    { "enable_kbo_callup_foreign_limit_patch", "Foreign call-up limit patch", 1, 1, NULL },
    { "enable_intl_established_fa_quality_probe_patch", "International FA quality probe", 1, 1, NULL },
    { "enable_kbo_season_phase_monitor", "Season phase monitor", 1, 1, NULL },
    { "enable_amateur_assignment_verbose_log", "Amateur assignment verbose log", 1, 0, NULL }
};

static const KboModRuntimeFlagSetting* kbo_find_mod_runtime_flag_setting(const char* key)
{
    if (key == NULL || key[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS) / sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS[0]); i++) {
        if (strcmp(KBO_MOD_RUNTIME_FLAG_SETTINGS[i].key, key) == 0) {
            return &KBO_MOD_RUNTIME_FLAG_SETTINGS[i];
        }
    }
    return NULL;
}

static int kbo_get_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = setting->enabled_value ? 0 : 1;
    if (!kbo_read_localappdata_json_flag_value(setting->key, setting->key, &raw_value)) {
        return setting->default_enabled ? 1 : 0;
    }
    return raw_value == setting->enabled_value ? 1 : 0;
}

static int kbo_set_mod_runtime_flag_enabled(const KboModRuntimeFlagSetting* setting, int enabled)
{
    if (setting == NULL || setting->key == NULL || setting->key[0] == '\0') {
        return 0;
    }
    int raw_value = enabled ? setting->enabled_value : (setting->enabled_value ? 0 : 1);
    int ok = kbo_write_localappdata_json_int_value(setting->key, raw_value);
    if (ok && enabled && setting->companion_enable_key != NULL && setting->companion_enable_key[0] != '\0') {
        ok = kbo_write_localappdata_json_int_value(setting->companion_enable_key, 1);
    }
    return ok;
}

static void kbo_webview_append_mod_runtime_flag_row(KboWindowTextBuffer* buffer, const KboModRuntimeFlagSetting* setting)
{
    if (buffer == NULL || setting == NULL || setting->key == NULL || setting->label == NULL) {
        return;
    }
    int enabled = kbo_get_mod_runtime_flag_enabled(setting);
    kbo_window_text_appendf(
        buffer,
        "<div class='settingRow flagSettingRow'><label class='settingLabel' for='flag_%s'>",
        setting->key);
    kbo_html_append_escaped(buffer, setting->label);
    kbo_window_text_appendf(
        buffer,
        "</label><select id='flag_%s' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/flag/%s/'+this.value}\">"
        "<option value='on' %s>On</option>"
        "<option value='off' %s>Off</option>"
        "</select></div>",
        setting->key,
        setting->key,
        enabled ? "selected" : "",
        enabled ? "" : "selected");
}

static void kbo_webview_append_mod_info_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_SETTINGS) {
        int profiler_enabled = kbo_get_profiler_enabled_setting();
        int allow_all_team_actions = kbo_get_allow_all_ui_team_actions_setting();
        kbo_window_text_appendf(
            buffer,
            "<div class='rights settingsGrid'>"
            "<section class='card modCard settingsCard'><h2 class='cardTitle'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\x84\xa4\xec\xa0\x95", "MOD SETTINGS"));
        kbo_window_text_appendf(
            buffer,
            "</h2><div class='settingRow'><label class='settingLabel' for='languageSelect'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("\xed\x91\x9c\xec\x8b\x9c \xec\x96\xb8\xec\x96\xb4", "Display language"));
        kbo_window_text_appendf(
            buffer,
            "</label><select id='languageSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/lang/'+this.value}\">"
            "<option value='ko' %s>%s</option>"
            "<option value='en' %s>English</option>"
            "</select></div>",
            g_kbo_hub_language == KBO_HUB_LANG_KO ? "selected" : "",
            "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4",
            g_kbo_hub_language == KBO_HUB_LANG_EN ? "selected" : "");
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='profilerSelect'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("\xed\x94\x84\xeb\xa1\x9c\xed\x8c\x8c\xec\x9d\xbc\xeb\x9f\xac", "Profiler"));
        kbo_window_text_appendf(
            buffer,
            "</label><select id='profilerSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/profiler/'+this.value}\">"
            "<option value='off' %s>Off</option>"
            "<option value='on' %s>On</option>"
            "</select></div>",
            profiler_enabled ? "" : "selected",
            profiler_enabled ? "selected" : "");
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='uiTeamActionsSelect'>");
        kbo_html_append_escaped(buffer, kbo_hub_text("UI \xed\x8c\x80 \xec\x95\xa1\xec\x85\x98", "UI team actions"));
        kbo_window_text_appendf(
            buffer,
            "</label><select id='uiTeamActionsSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://mod/settings/ui-team-actions/'+this.value}\">"
            "<option value='all' %s>%s</option>"
            "<option value='controlled' %s>%s</option>"
            "</select></div>",
            allow_all_team_actions ? "selected" : "",
            kbo_hub_text("\xeb\xaa\xa8\xeb\x93\xa0 \xed\x8c\x80 \xed\x97\x88\xec\x9a\xa9 (\xea\xb0\x9c\xeb\xb0\x9c)", "All teams (development)"),
            allow_all_team_actions ? "" : "selected",
            kbo_hub_text("\xeb\x82\xb4 \xed\x8c\x80\xeb\xa7\x8c \xed\x97\x88\xec\x9a\xa9", "Controlled team only"));
        kbo_window_text_appendf(buffer, "<div class='settingsDivider'></div>");
        for (size_t i = 0; i < sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS) / sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS[0]); i++) {
            kbo_webview_append_mod_runtime_flag_row(buffer, &KBO_MOD_RUNTIME_FLAG_SETTINGS[i]);
        }
        kbo_window_text_appendf(buffer, "</section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_README) {
        char github_icon_path[MAX_PATH] = {0};
        kbo_hub_local_asset_path("github-mark.png", github_icon_path, sizeof(github_icon_path));

        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>MOD INFO</h2>"
            "<p class='modLead'>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 Out of the Park Baseball 27\xec\x97\x90\xec\x84\x9c KBO\xeb\xa5\xbc \xea\xb0\x80\xec\x9e\xa5 KBO\xeb\x8b\xb5\xea\xb2\x8c \xec\xa6\x90\xea\xb8\xb0\xea\xb8\xb0 \xec\x9c\x84\xed\x95\x9c \xed\x8c\xac \xec\xa0\x9c\xec\x9e\x91 \xeb\xaa\xa8\xeb\x93\x9c\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is a fan-made mod for Out of the Park Baseball 27, built to make the KBO experience feel richer, sharper, and more authentic."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x8b\xa4\xec\xa0\x9c \xeb\xa6\xac\xea\xb7\xb8\xec\x9d\x98 \xed\x9d\x90\xeb\xa6\x84, \xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0 \xeb\xa7\xa5\xeb\x9d\xbd, \xed\x95\x9c\xea\xb5\xad \xec\x95\xbc\xea\xb5\xac \xed\x8a\xb9\xec\x9c\xa0\xec\x9d\x98 \xec\xa0\x9c\xeb\x8f\x84\xec\x99\x80 \xeb\xa6\xac\xeb\x93\xac\xec\x9d\x84 \xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xed\x95\xa8\xea\xbb\x98 \xea\xb2\x8c\xec\x9e\x84 \xec\x95\x88\xec\x9c\xbc\xeb\xa1\x9c \xec\x9e\x90\xec\x97\xb0\xec\x8a\xa4\xeb\x9f\xbd\xea\xb2\x8c \xeb\x81\x8c\xec\x96\xb4\xec\x98\xa4\xeb\x8a\x94 \xea\xb2\x83\xec\x9d\x84 \xeb\xaa\xa9\xed\x91\x9c\xeb\xa1\x9c \xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Used with the KBO Launcher, it brings launcher-assisted rules, roster context, and Korean baseball rhythms into the game."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "KBO \xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xed\x95\xa8\xea\xbb\x98 \xec\x82\xac\xec\x9a\xa9\xed\x95\xa0 \xeb\x95\x8c \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98, \xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80, \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84 \xea\xb0\x99\xec\x9d\x80 \xeb\xb3\xb4\xea\xb0\x95 \xea\xb8\xb0\xeb\x8a\xa5\xec\x9d\xb4 \xeb\xa7\x9e\xeb\xac\xbc\xeb\xa0\xa4 \xea\xb0\x80\xec\x9e\xa5 \xec\x99\x84\xec\x84\xb1\xeb\x90\x9c \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xec\xa0\x9c\xea\xb3\xb5\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "The best experience comes when the mod and launcher work together across foreign-player, military-team, and Asian Games workflows."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 Out of the Park Developments\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88 \xeb\x98\x90\xeb\x8a\x94 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x9c\xb4 \xec\xbd\x98\xed\x85\x90\xec\xb8\xa0\xea\xb0\x80 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is not an official product of, nor affiliated with, Out of the Park Developments."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>GITHUB</h2>"
            "<div class='githubHero'>");
        if (github_icon_path[0] != '\0') {
            kbo_window_text_appendf(buffer, "<img class='githubLogo' src='");
            kbo_webview_append_image_src(buffer, github_icon_path);
            kbo_window_text_appendf(buffer, "'>");
        }
        kbo_window_text_appendf(
            buffer,
            "<div class='githubRepo'><strong>OOTP27_Ultimate_KBO</strong>"
            "<span>github.com/lebronisbest623</span></div></div>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xea\xb0\x9c\xeb\xb0\x9c \xea\xb8\xb0\xeb\xa1\x9d\xea\xb3\xbc \xeb\xb0\xb0\xed\x8f\xac \xed\x9d\x90\xeb\xa6\x84\xec\x9d\x80 GitHub \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xec\x97\x90\xec\x84\x9c \xed\x99\x95\xec\x9d\xb8\xed\x95\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Development history, releases, and issues are available in the GitHub repository."));
        kbo_window_text_appendf(
            buffer,
            "</p><a class='githubLink' href='kbo://github'>GitHub</a>"
            "</section><section class='card modCard'><h2 class='cardTitle'>SUPPORTED BUILD</h2>"
            "<div class='buildList'>");
        for (size_t i = 0; i < sizeof(KBO_SUPPORTED_OOTP_BUILDS) / sizeof(KBO_SUPPORTED_OOTP_BUILDS[0]); i++) {
            const OotpSupportedBuild* build = &KBO_SUPPORTED_OOTP_BUILDS[i];
            kbo_window_text_appendf(
                buffer,
                "<div class='buildRow'><span class='buildLabel'>%s</span><span class='buildValue'>OOTP 27 %s</span></div>"
                "<div class='buildRow'><span class='buildLabel'>%s</span><span class='buildValue'>0x%08X</span></div>"
                "<div class='buildRow'><span class='buildLabel'>%s</span><span class='buildValue'>0x%08X</span></div>",
                kbo_hub_text("\xeb\xb2\x84\xec\xa0\x84", "Build"),
                build->label,
                kbo_hub_text("\xed\x83\x80\xec\x9e\x84\xec\x8a\xa4\xed\x83\xac\xed\x94\x84", "Timestamp"),
                build->timestamp,
                kbo_hub_text("\xec\x9d\xb4\xeb\xaf\xb8\xec\xa7\x80", "Image"),
                build->size_of_image);
        }
        kbo_window_text_appendf(buffer, "</div></section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_LICENSE) {
        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>LICENSE</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xec\x99\x80 KBO \xeb\x9f\xb0\xec\xb2\x98\xeb\x8a\x94 \xed\x8c\xac \xec\xa0\x9c\xec\x9e\x91 \xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xeb\xa1\x9c, \xea\xb0\x9c\xec\x9d\xb8\xec\xa0\x81\xec\x9d\xb8 OOTP \xed\x94\x8c\xeb\xa0\x88\xec\x9d\xb4\xec\x99\x80 \xec\xbb\xa4\xeb\xae\xa4\xeb\x8b\x88\xed\x8b\xb0 \xed\x99\x9c\xec\x9a\xa9\xec\x9d\x84 \xec\x9c\x84\xed\x95\xb4 \xec\x9e\x88\xeb\x8a\x94 \xea\xb7\xb8\xeb\x8c\x80\xeb\xa1\x9c \xec\xa0\x9c\xea\xb3\xb5\xeb\x90\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO and the KBO Launcher are fan-made project materials provided as-is for personal OOTP play and community use."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xeb\xaa\xa8\xeb\x93\x9c \xed\x8c\x8c\xec\x9d\xbc\xea\xb3\xbc \xeb\x9f\xb0\xec\xb2\x98 \xea\xb5\xac\xec\x84\xb1\xec\x9d\x80 \xec\xb6\x9c\xec\xb2\x98\xeb\xa5\xbc \xeb\x82\xa8\xea\xb8\xb0\xeb\x8a\x94 \xed\x95\x9c \xec\x9e\x90\xec\x9c\xa0\xeb\xa1\xad\xea\xb2\x8c \xec\xb0\xb8\xea\xb3\xa0\xed\x95\x98\xea\xb3\xa0 \xec\x88\x98\xec\xa0\x95\xed\x95\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4. \xeb\x8b\xa8, \xeb\xb3\xb8 \xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xeb\xa5\xbc \xec\x9c\xa0\xeb\xa3\x8c \xec\x83\x81\xed\x92\x88\xec\x9d\xb4\xeb\x82\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xbd\x98\xed\x85\x90\xec\xb8\xa0\xec\xb2\x98\xeb\x9f\xbc \xeb\xb0\xb0\xed\x8f\xac\xed\x95\x98\xeb\x8a\x94 \xea\xb2\x83\xec\x9d\x80 \xea\xb6\x8c\xec\x9e\xa5\xed\x95\x98\xec\xa7\x80 \xec\x95\x8a\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "You may reference and modify the mod files and launcher configuration as long as attribution remains. Do not sell or present this project as official content."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xeb\xa5\xbc \xec\x9e\xac\xeb\xb0\xb0\xed\x8f\xac\xed\x95\x98\xea\xb1\xb0\xeb\x82\x98 \xec\x88\x98\xec\xa0\x95\xeb\xb3\xb8\xec\x9d\x84 \xea\xb3\xb5\xec\x9c\xa0\xed\x95\xa0 \xeb\x95\x8c\xeb\x8a\x94 Ultimate KBO\xec\x99\x80 \xec\x9b\x90 \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xeb\xa5\xbc \xed\x95\xa8\xea\xbb\x98 \xed\x91\x9c\xec\x8b\x9c\xed\x95\xb4 \xec\xa3\xbc\xec\x84\xb8\xec\x9a\x94.",
                "When redistributing the project or sharing modified versions, keep the Ultimate KBO name and original repository visible."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>NOT OFFICIAL</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 Out of the Park Developments\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88, \xea\xb3\xb5\xec\x8b\x9d \xed\x8c\xa8\xec\xb9\x98, \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x9c\xb4 \xec\xbd\x98\xed\x85\x90\xec\xb8\xa0\xea\xb0\x80 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is not an official product, patch, or affiliated content of Out of the Park Developments."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Out of the Park Baseball, OOTP \xeb\xb0\x8f \xea\xb4\x80\xeb\xa0\xa8 \xec\x83\x81\xed\x91\x9c\xeb\x8a\x94 \xea\xb0\x81 \xea\xb6\x8c\xeb\xa6\xac\xec\x9e\x90\xec\x97\x90\xea\xb2\x8c \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Out of the Park Baseball, OOTP, and related trademarks belong to their respective owners."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>THIRD-PARTY ASSETS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xea\xb5\xac\xeb\x8b\xa8\xeb\xaa\x85, \xeb\xa1\x9c\xea\xb3\xa0, \xeb\xa6\xac\xea\xb7\xb8 \xed\x91\x9c\xec\x8b\x9d \xeb\x93\xb1 \xed\x98\x84\xec\x8b\xa4 \xec\x95\xbc\xea\xb5\xac\xec\x99\x80 \xea\xb4\x80\xeb\xa0\xa8\xeb\x90\x9c \xec\x9e\x90\xec\x82\xb0\xec\x9d\x98 \xea\xb6\x8c\xeb\xa6\xac\xeb\x8a\x94 \xea\xb0\x81 \xea\xb6\x8c\xeb\xa6\xac\xec\x9e\x90\xec\x97\x90\xea\xb2\x8c \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Club names, logos, league marks, and other real-baseball assets belong to their respective rights holders."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x95\xa8\xea\xbb\x98 \xec\xa0\x9c\xea\xb3\xb5\xeb\x90\x98\xeb\x8a\x94 \xed\x8f\xb0\xed\x8a\xb8, \xec\x95\x84\xec\x9d\xb4\xec\xbd\x98, \xeb\x9d\xbc\xec\x9d\xb4\xeb\xb8\x8c\xeb\x9f\xac\xeb\xa6\xac \xeb\x93\xb1\xec\x9d\x80 \xea\xb0\x81\xea\xb0\x81\xec\x9d\x98 \xec\x9b\x90 \xeb\x9d\xbc\xec\x9d\xb4\xec\x84\xa0\xec\x8a\xa4 \xec\xa1\xb0\xea\xb1\xb4\xec\x9d\x84 \xeb\x94\xb0\xeb\xa6\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Bundled fonts, icons, libraries, and other third-party materials remain under their original license terms."));
        kbo_window_text_appendf(buffer, "</p></section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CREDITS) {
        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>CREATOR</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xec\x99\x80 KBO \xeb\x9f\xb0\xec\xb2\x98\xeb\x8a\x94 lebronisbest623\xec\x9d\x98 \xec\x9e\x91\xed\x92\x88\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO and the KBO Launcher are works by lebronisbest623."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xeb\xaa\xa8\xeb\x93\x9c \xea\xb5\xac\xec\x84\xb1, \xeb\x9f\xb0\xec\xb2\x98 \xea\xb0\x9c\xeb\xb0\x9c, KBO \xed\x99\x98\xea\xb2\xbd \xea\xb5\xac\xed\x98\x84\xec\x9d\x84 \xed\x95\xa8\xea\xbb\x98 \xeb\x8b\xb4\xec\x95\x98\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "This includes the mod structure, launcher work, and KBO-specific game integration."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>TESTERS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x95\x84\xec\xa7\x81 \xeb\x93\xb1\xeb\xa1\x9d\xeb\x90\x9c \xed\x85\x8c\xec\x8a\xa4\xed\x84\xb0\xea\xb0\x80 \xec\x97\x86\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "No testers are listed yet."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x85\x8c\xec\x8a\xa4\xed\x8a\xb8\xec\x97\x90 \xed\x95\xa8\xea\xbb\x98\xed\x95\xa0 \xec\x82\xac\xeb\x9e\x8c\xec\x9d\xb4 \xec\x83\x9d\xea\xb8\xb0\xeb\xa9\xb4 \xec\x9d\xb4\xea\xb3\xb3\xec\x97\x90 \xea\xb8\xb0\xeb\xa1\x9d\xeb\x90\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Future testers will be recorded here."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>SPECIAL THANKS</h2>"
            "<p>lazyquokka1218 (facegens &amp; logos)</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xed\x94\x8c\xeb\xa0\x88\xec\x9d\xb4\xec\x96\xb4 facegen\xea\xb3\xbc \xeb\xa1\x9c\xea\xb3\xa0 \xec\x9e\x90\xec\x82\xb0 \xec\x9e\x91\xec\x97\x85\xec\x97\x90 \xea\xb0\x90\xec\x82\xac\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Thank you for the player facegen and logo asset work."));
        kbo_window_text_appendf(buffer, "</p></section></div>");
        return;
    }

    if (g_kbo_hub_selected_mod_subview == KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS) {
        char github_icon_path[MAX_PATH] = {0};
        kbo_hub_local_asset_path("github-mark.png", github_icon_path, sizeof(github_icon_path));

        kbo_window_text_appendf(
            buffer,
            "<div class='rights modReadme modContrib'>"
            "<section class='card modCard modCardMain'><h2 class='cardTitle'>CONTRIBUTIONS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "Ultimate KBO\xeb\x8a\x94 \xed\x98\xbc\xec\x9e\x90 \xeb\x8b\xab\xec\x95\x84\xeb\x91\x90\xeb\x8a\x94 \xeb\xaa\xa8\xeb\x93\x9c\xea\xb0\x80 \xec\x95\x84\xeb\x8b\x88\xeb\x9d\xbc, KBO\xeb\xa5\xbc \xeb\x8d\x94 \xea\xb7\xb8\xeb\x9f\xb4\xeb\x93\xaf\xed\x95\x98\xea\xb2\x8c \xeb\xa7\x8c\xeb\x93\xa4\xea\xb3\xa0 \xec\x8b\xb6\xec\x9d\x80 \xec\x82\xac\xeb\x9e\x8c\xeb\x93\xa4\xec\x9d\xb4 \xec\xa1\xb0\xea\xb8\x88\xec\x94\xa9 \xeb\xb3\xb4\xed\x83\x9c\xeb\xa9\xb0 \xec\xa2\x8b\xec\x95\x84\xec\xa7\x88 \xec\x88\x98 \xec\x9e\x88\xeb\x8a\x94 \xed\x94\x84\xeb\xa1\x9c\xec\xa0\x9d\xed\x8a\xb8\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Ultimate KBO is a project that can grow through small contributions from people who want the KBO experience to feel more complete."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "GitHub \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xec\x97\x90\xec\x84\x9c\xeb\x8a\x94 \xeb\xaa\xa8\xeb\x93\x9c \xeb\x8d\xb0\xec\x9d\xb4\xed\x84\xb0, \xea\xb5\xac\xec\x9e\xa5 \xec\x9e\x90\xeb\xa3\x8c, \xeb\xa1\x9c\xea\xb3\xa0, \xed\x8e\x98\xec\x9d\xb4\xec\x8a\xa4\xec\xa0\xa0, \xeb\xb2\x88\xec\x97\xad, \xed\x85\x8c\xec\x8a\xa4\xed\x8a\xb8 \xeb\xa6\xac\xed\x8f\xac\xed\x8a\xb8 \xeb\x93\xb1 \xec\x97\xac\xeb\x9f\xac \xed\x98\x95\xed\x83\x9c\xec\x9d\x98 \xea\xb8\xb0\xec\x97\xac\xeb\xa5\xbc \xeb\xb0\x9b\xec\x9d\x84 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "The GitHub repository can accept many kinds of contributions, including mod data, stadium references, logos, facegens, translations, and testing reports."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x9e\x91\xec\x9d\x80 \xec\x88\x98\xec\xa0\x95 \xec\xa0\x9c\xec\x95\x88, \xeb\x88\x84\xeb\x9d\xbd\xeb\x90\x9c \xec\x9e\x90\xeb\xa3\x8c \xec\xa0\x9c\xeb\xb3\xb4, \xec\x8a\xa4\xed\x81\xac\xeb\xa6\xb0\xec\x83\xb7\xea\xb3\xbc \xec\x9e\xac\xed\x98\x84 \xeb\xa1\x9c\xea\xb7\xb8\xeb\x8f\x84 \xec\xb6\xa9\xeb\xb6\x84\xed\x9e\x88 \xeb\x8f\x84\xec\x9b\x80\xec\x9d\xb4 \xeb\x90\xa9\xeb\x8b\x88\xeb\x8b\xa4.",
                "Small fixes, missing references, screenshots, and reproduction notes are all useful."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>WHAT HELPS</h2>"
            "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0\xec\x99\x80 \xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95 \xeb\xb3\xb4\xea\xb0\x95, \xea\xb5\xac\xec\x9e\xa5 \xec\xa0\x95\xeb\xb3\xb4, \xed\x8c\x80/\xeb\xa6\xac\xea\xb7\xb8 \xeb\xa1\x9c\xea\xb3\xa0, \xec\x84\xa0\xec\x88\x98 facegen, \xed\x85\x8d\xec\x8a\xa4\xed\x8a\xb8\xec\x99\x80 \xeb\xb2\x88\xec\x97\xad, \xeb\xb2\x84\xea\xb7\xb8 \xeb\xa6\xac\xed\x8f\xac\xed\x8a\xb8\xea\xb0\x80 \xeb\xaa\xa8\xeb\x91\x90 \xea\xb8\xb0\xec\x97\xac \xeb\x8c\x80\xec\x83\x81\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
                "Roster and league tuning, stadium information, team and league logos, player facegens, text, translation, and bug reports are all welcome."));
        kbo_window_text_appendf(buffer, "</p><p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\x99\x84\xec\x84\xb1\xeb\x90\x9c \xed\x8c\x8c\xec\x9d\xbc\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x88\xec\x96\xb4\xeb\x8f\x84 \xea\xb4\x9c\xec\xb0\xae\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4. \xec\xb0\xb8\xea\xb3\xa0 \xec\x9e\x90\xeb\xa3\x8c\xeb\x82\x98 \xeb\xb0\xa9\xed\x96\xa5 \xec\xa0\x9c\xec\x95\x88\xeb\xa7\x8c\xec\x9c\xbc\xeb\xa1\x9c\xeb\x8f\x84 \xeb\x8b\xa4\xec\x9d\x8c \xec\x97\x85\xeb\x8d\xb0\xec\x9d\xb4\xed\x8a\xb8\xec\x9d\x98 \xec\x8b\xa4\xeb\xa7\x88\xeb\xa6\xac\xea\xb0\x80 \xeb\x90\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "A contribution does not have to be a finished file. References and direction can still shape the next update."));
        kbo_window_text_appendf(
            buffer,
            "</p></section><section class='card modCard'><h2 class='cardTitle'>GITHUB</h2>");
        if (github_icon_path[0] != '\0') {
            kbo_window_text_appendf(buffer, "<div class='githubHero'><img class='githubLogo' src='");
            kbo_webview_append_image_src(buffer, github_icon_path);
            kbo_window_text_appendf(buffer, "'><div class='githubRepo'><strong>OOTP27_Ultimate_KBO</strong><span>github.com/lebronisbest623</span></div></div>");
        }
        kbo_window_text_appendf(buffer, "<p>");
        kbo_html_append_escaped(
            buffer,
            kbo_hub_text(
                "\xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xeb\xa5\xbc \xeb\xb0\xa9\xeb\xac\xb8\xed\x95\xb4 \xec\x9d\xb4\xec\x8a\x88\xeb\xa5\xbc \xeb\x82\xa8\xea\xb8\xb0\xea\xb1\xb0\xeb\x82\x98, \xec\x9e\x90\xeb\xa3\x8c\xeb\xa5\xbc \xea\xb3\xb5\xec\x9c\xa0\xed\x95\x98\xea\xb1\xb0\xeb\x82\x98, \xec\xa7\x81\xec\xa0\x91 \xeb\xb3\x80\xea\xb2\xbd \xec\xa0\x9c\xec\x95\x88\xec\x9d\x84 \xeb\xb3\xb4\xeb\x82\xbc \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
                "Visit the repository to open issues, share material, or send proposed changes."));
        kbo_window_text_appendf(buffer, "</p><a class='githubLink' href='kbo://github'>GitHub</a></section></div>");
        return;
    }

    kbo_window_text_appendf(buffer, "<div class='rights'><section class='card'></section></div>");
}
