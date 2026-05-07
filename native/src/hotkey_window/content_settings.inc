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
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xeb\xa6\xac\xea\xb7\xb8 \xec\x84\xa4\xec\xa0\x95", "LEAGUE SETTINGS"));
    int multiplier = kbo_get_intl_established_fa_multiplier();
    kbo_window_text_appendf(&buffer, "%s\r\n", "INTERNATIONAL ESTABLISHED FA");
    kbo_window_text_appendf(
        &buffer,
        "  Multiplier: %dx (OOTP A Lot 12 -> about %d)\r\n",
        multiplier,
        12 * multiplier);
}
