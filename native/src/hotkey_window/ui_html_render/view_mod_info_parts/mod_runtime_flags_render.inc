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

static void kbo_webview_append_mod_runtime_flag_group(
    KboWindowTextBuffer* buffer,
    int category,
    const char* title,
    const char* help_text)
{
    if (buffer == NULL || title == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<div class='flagGroup'><h3>");
    kbo_html_append_escaped(buffer, title);
    kbo_window_text_appendf(buffer, "</h3>");
    if (help_text != NULL && help_text[0] != '\0') {
        kbo_window_text_appendf(buffer, "<p>");
        kbo_html_append_escaped(buffer, help_text);
        kbo_window_text_appendf(buffer, "</p>");
    }
    for (size_t i = 0; i < sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS) / sizeof(KBO_MOD_RUNTIME_FLAG_SETTINGS[0]); i++) {
        if (KBO_MOD_RUNTIME_FLAG_SETTINGS[i].category == category) {
            kbo_webview_append_mod_runtime_flag_row(buffer, &KBO_MOD_RUNTIME_FLAG_SETTINGS[i]);
        }
    }
    kbo_window_text_appendf(buffer, "</div>");
}
