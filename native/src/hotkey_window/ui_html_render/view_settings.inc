static void kbo_webview_append_settings_view(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }

    int multiplier = kbo_get_intl_established_fa_multiplier();
    const int presets[] = {1, 2, 5, 10, 20};
    int preset_has_current = 0;
    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        if (presets[i] == multiplier) {
            preset_has_current = 1;
            break;
        }
    }

    kbo_window_text_appendf(
        buffer,
        "<div class='rights settingsGrid'>"
        "<section class='card modCard settingsCard'><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95", "LEAGUE SETTINGS"));

    kbo_window_text_appendf(
        buffer,
        "</h2><div class='settingRow'><label class='settingLabel' for='intlFaMultiplierSelect'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 FA \xec\x83\x9d\xec\x84\xb1", "International FA pool"));
    kbo_window_text_appendf(
        buffer,
        "</label><select id='intlFaMultiplierSelect' class='ootpSelect' onchange=\"if(this.value){location.href='kbo://settings/intl-fa-multiplier/'+this.value}\">");
    if (!preset_has_current) {
        kbo_window_text_appendf(
            buffer,
            "<option value='%d' selected>%dx</option>",
            multiplier,
            multiplier);
    }

    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        int value = presets[i];
        kbo_window_text_appendf(
            buffer,
            "<option value='%d' %s>%dx</option>",
            value,
            value == multiplier ? "selected" : "",
            value);
    }

    kbo_window_text_appendf(
        buffer,
        "</select></div></section></div>");
}
