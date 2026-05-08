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
    kbo_window_text_appendf(&buffer, "GitHub\r\n");
    kbo_window_text_appendf(&buffer, "  https://github.com/lebronisbest623/OOTP27_Ultimate_KBO\r\n\r\n");
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "Ultimate KBO \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 \xec\x98\xa4\xec\xa7\x81 OOTP\xec\x97\x90\xec\x84\x9c \xec\xb5\x9c\xea\xb3\xa0\xec\x9d\x98 KBO \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xec\x9c\x84\xed\x95\xb4 \xeb\xa7\x8c\xeb\x93\xa4\xec\x96\xb4\xec\xa7\x84 \xeb\xaa\xa8\xeb\x93\x9c\xec\x9e\x85\xeb\x8b\x88\xeb\x8b\xa4.",
            "Ultimate KBO is built only to provide the best KBO experience in OOTP."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "\xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 KBO \xeb\x9f\xb0\xec\xb2\x98\xec\x99\x80 \xed\x95\xa8\xea\xbb\x98\xed\x95\xa0 \xeb\x95\x8c \xec\xb5\x9c\xec\x83\x81\xec\x9d\x98 \xea\xb2\xbd\xed\x97\x98\xec\x9d\x84 \xed\x95\xa0 \xec\x88\x98 \xec\x9e\x88\xec\x8a\xb5\xeb\x8b\x88\xeb\x8b\xa4.",
            "The mod is best experienced together with the KBO Launcher."));
    kbo_window_text_appendf(
        &buffer, "%s\r\n",
        kbo_hub_text(
            "\xeb\xb3\xb8 \xeb\xaa\xa8\xeb\x93\x9c\xeb\x8a\x94 OOTPD\xec\x9d\x98 \xea\xb3\xb5\xec\x8b\x9d \xec\xa0\x9c\xed\x92\x88\xec\x9d\xb4 \xec\x95\x84\xeb\x8b\x99\xeb\x8b\x88\xeb\x8b\xa4.",
            "This mod is not an official OOTPD product."));
    for (size_t i = 0; i < kbo_supported_ootp_build_count(); i++) {
        const OotpSupportedBuild* build = kbo_supported_ootp_build_at(i);
        if (build == NULL) {
            continue;
        }
        kbo_window_text_appendf(
            &buffer,
            "Supported build: OOTP 27 %s / timestamp 0x%08X / image 0x%08X\r\n",
            build->label,
            build->timestamp,
            build->size_of_image);
    }
}
