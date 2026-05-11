#include "../info/ui_mod_info_views_internal.h"

void kbo_webview_append_settings_view(KboWindowTextBuffer* buffer)
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

    const char* baseline_labels_ko[9] = {
        "\xec\xb5\x9c\xec\xa0\x80 \xec\x97\xb0\xeb\xb4\x89",
        "\xed\x95\x98\xea\xb8\x89",
        "\xeb\xb3\xb4\xed\x86\xb5 \xec\x9d\xb4\xed\x95\x98",
        "\xed\x8f\x89\xea\xb7\xa0 \xec\x9d\xb4\xed\x95\x98",
        "\xed\x8f\x89\xea\xb7\xa0",
        "\xed\x8f\x89\xea\xb7\xa0 \xec\x9d\xb4\xec\x83\x81",
        "\xec\xa4\x80\xec\xb2\x99\xea\xb8\x89",
        "\xec\x8a\xa4\xed\x83\x80\xea\xb8\x89",
        "\xec\x8a\x88\xed\x8d\xbc\xec\x8a\xa4\xed\x83\x80\xea\xb8\x89"
    };
    const char* baseline_labels_en[9] = {
        "Minimum",
        "Poor",
        "Fair",
        "Below Average",
        "Average",
        "Above Average",
        "Good",
        "Star",
        "Superstar"
    };
    const char* non_asian_quality_cap_labels_ko[KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT] = {
        "\xec\x84\xa0\xeb\xb0\x9c \xed\x88\xac\xec\x88\x98",
        "\xeb\xb6\x88\xed\x8e\x9c \xed\x88\xac\xec\x88\x98",
        "\xed\x88\xac\xec\x88\x98(\xec\x97\xad\xed\x95\xa0 \xeb\xaf\xb8\xec\x83\x81)",
        "\xec\x95\xbc\xec\x88\x98",
        "\xed\x8f\xac\xec\x88\x98"
    };
    const char* non_asian_quality_cap_labels_en[KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT] = {
        "Starting Pitcher",
        "Bullpen Pitcher",
        "Unknown Pitcher",
        "Hitter",
        "Catcher"
    };
    kbo_window_text_appendf(
        buffer,
        "<div class='rights settingsGrid'>"
        "<section class='card modCard settingsCard'><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95", "LEAGUE SETTINGS"));

    kbo_window_text_appendf(
        buffer,
        "</h2><div class='settingRow'><label class='settingLabel' for='intlFaMultiplierSelect'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 FA \xec\x83\x9d\xec\x84\xb1", "International FA pool"));
    char current_choice_label[32] = {0};
    snprintf(current_choice_label, sizeof(current_choice_label), "%dx", multiplier);
    kbo_window_text_appendf(buffer, "</label>");
    kbo_webview_begin_ootp_choice(buffer, "intlFaMultiplierSelect", current_choice_label);
    if (!preset_has_current) {
        char href[96] = {0};
        snprintf(href, sizeof(href), "kbo://settings/intl-fa-multiplier/%d", multiplier);
        kbo_webview_append_ootp_choice_option(buffer, href, current_choice_label, 1);
    }

    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
        int value = presets[i];
        char href[96] = {0};
        char label[32] = {0};
        snprintf(href, sizeof(href), "kbo://settings/intl-fa-multiplier/%d", value);
        snprintf(label, sizeof(label), "%dx", value);
        kbo_webview_append_ootp_choice_option(buffer, href, label, value == multiplier);
    }

    int quality_cap_enabled = kbo_get_foreign_fa_quality_cap_enabled_setting();
    kbo_window_text_appendf(
        buffer,
        "</div></details></div><div class='settingRow'><label class='settingLabel' for='foreignFaQualityCapSelect'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x83\x9d\xec\x84\xb1 \xed\x92\x88\xec\xa7\x88 \xec\xba\xa1", "Foreign FA quality cap"));
    kbo_window_text_appendf(buffer, "</label>");
    kbo_webview_begin_ootp_choice(buffer, "foreignFaQualityCapSelect", quality_cap_enabled ? "ON" : "OFF");
    kbo_webview_append_ootp_choice_option(buffer, "kbo://settings/foreign-fa-quality-cap/on", "ON", quality_cap_enabled);
    kbo_webview_append_ootp_choice_option(buffer, "kbo://settings/foreign-fa-quality-cap/off", "OFF", !quality_cap_enabled);
    kbo_webview_end_ootp_choice(buffer);
    kbo_window_text_appendf(buffer, "</div>");

    kbo_window_text_appendf(
        buffer,
        "<div class='settingsDivider'></div><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xeb\xb9\x84\xec\x95\x84\xec\x8b\x9c\xec\x95\x84 \xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xed\x92\x88\xec\xa7\x88 \xec\xba\xa1", "NON-ASIAN QUALITY CAPS"));
    kbo_window_text_appendf(buffer, "</h2>");

    for (int i = 0; i < KBO_FOREIGN_FA_NON_ASIAN_QUALITY_CAP_COUNT; i++) {
        int current = kbo_get_foreign_fa_non_asian_quality_cap_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='nonAsianQualityCap%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(non_asian_quality_cap_labels_ko[i], non_asian_quality_cap_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='nonAsianQualityCap%d' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='0' data-max='%d' data-step='500' value='%d' "
            "onchange=\"location.href='kbo://settings/foreign-fa-non-asian-quality-cap/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            KBO_FOREIGN_FA_QUALITY_CAP_MAX,
            current,
            i);
    }

    kbo_window_text_appendf(
        buffer,
        "<div class='settingsDivider'></div><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x97\xb0\xeb\xb4\x89 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4\xeb\x9d\xbc\xec\x9d\xb8", "FOREIGN PLAYER SALARY BASELINE"));
    kbo_window_text_appendf(buffer, "</h2>");

    for (int i = 0; i < 9; i++) {
        int current = kbo_get_foreign_fa_demand_baseline_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='foreignBaseline%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(baseline_labels_ko[i], baseline_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='foreignBaseline%d' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='0' data-max='20000000' data-step='1000' value='%d' "
            "onchange=\"location.href='kbo://settings/foreign-fa-baseline/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            current,
            i);
    }

    kbo_window_text_appendf(buffer, "<div class='settingsDivider'></div><h2 class='cardTitle'>");
    kbo_html_append_escaped(buffer, kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x84\xec\xbf\xbc\xed\x84\xb0 \xec\x97\xb0\xeb\xb4\x89 \xeb\xb2\xa0\xec\x9d\xb4\xec\x8a\xa4\xeb\x9d\xbc\xec\x9d\xb8", "ASIAN QUOTA SALARY BASELINE"));
    kbo_window_text_appendf(buffer, "</h2>");

    for (int i = 0; i < 9; i++) {
        int current = kbo_get_asian_quota_fa_demand_baseline_value(i);
        kbo_window_text_appendf(
            buffer,
            "<div class='settingRow'><label class='settingLabel' for='asianQuotaBaseline%d'>",
            i);
        kbo_html_append_escaped(buffer, kbo_hub_text(baseline_labels_ko[i], baseline_labels_en[i]));
        kbo_window_text_appendf(
            buffer,
            "</label><input id='asianQuotaBaseline%d' class='ootpSelect salaryInput' type='text' inputmode='numeric' pattern='[0-9]*' data-min='0' data-max='20000000' data-step='1000' value='%d' "
            "onchange=\"location.href='kbo://settings/asian-quota-fa-baseline/%d/'+this.value\" "
            "onkeydown=\"if(event.key==='Enter'){this.blur();}\"></div>",
            i,
            current,
            i);
    }

    kbo_window_text_appendf(buffer, "</section></div>");
}
