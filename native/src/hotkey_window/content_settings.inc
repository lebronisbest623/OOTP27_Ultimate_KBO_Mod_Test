static void kbo_build_settings_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x84\xa4\xec\xa0\x95", "SETTINGS"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xec\x96\xb8\xec\x96\xb4", "LANGUAGE"));
    kbo_window_text_appendf(
        &buffer,
        kbo_hub_text("  \xed\x98\x84\xec\x9e\xac \xec\x96\xb8\xec\x96\xb4: %s\r\n", "  Current language: %s\r\n"),
        g_kbo_hub_language == KBO_HUB_LANG_EN ? "English" : "\xED\x95\x9c\xEA\xB5\xAD\xEC\x96\xB4");
    int multiplier = kbo_get_intl_established_fa_multiplier();
    kbo_window_text_appendf(&buffer, "\r\n");
    kbo_window_text_appendf(&buffer, "%s\r\n", "INTERNATIONAL ESTABLISHED FA");
    kbo_window_text_appendf(
        &buffer,
        "  Multiplier: %dx (OOTP A Lot 12 -> about %d)\r\n",
        multiplier,
        12 * multiplier);
    kbo_window_text_appendf(&buffer, "\r\n");
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xeb\x8b\xa8\xec\xb6\x95\xed\x82\xa4", "HOTKEYS"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("  F2  \xec\xb0\xbd \xec\x97\xb4\xea\xb8\xb0 / \xeb\x8b\xab\xea\xb8\xb0", "  F2  Open / close window"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("  F5  \xed\x98\x84\xec\x9e\xac \xed\x8c\xa8\xeb\x84\x90 \xec\x83\x88\xeb\xa1\x9c\xea\xb3\xa0\xec\xb9\xa8", "  F5  Refresh current panel"));
}
