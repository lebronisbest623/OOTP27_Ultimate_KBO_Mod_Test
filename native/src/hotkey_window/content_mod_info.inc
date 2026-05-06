static void kbo_build_mod_info_hub_text(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    KboWindowTextBuffer buffer;
    buffer.data     = out;
    buffer.capacity = out_size;
    buffer.length   = 0;

    kbo_window_text_appendf(&buffer, "%s\r\n\r\n", kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO"));
    kbo_window_text_appendf(&buffer, "%s\r\n", kbo_hub_text("\xeb\x9d\xbc\xec\x9d\xb4\xec\x84\xa0\xec\x8a\xa4", "LICENSE"));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "  \xec\xa0\x80\xec\x9e\xa5\xec\x86\x8c\xec\x9d\x98 LICENSE, README, CREDITS \xeb\xac\xb8\xec\x84\x9c\xeb\xa5\xbc \xed\x99\x95\xec\x9d\xb8\xed\x95\x98\xec\x84\xb8\xec\x9a\x94.",
            "  See LICENSE, README, and CREDITS in the repository."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "  \xec\x9d\xb4 \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 OOTP Developments\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
            "  This mod is not an official OOTP Developments product."));
}
