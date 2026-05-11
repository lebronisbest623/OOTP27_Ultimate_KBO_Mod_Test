#include "ui_mod_info_views_internal.h"

void kbo_webview_begin_ootp_choice(KboWindowTextBuffer* buffer, const char* id, const char* current_label)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<details class='ootpChoice'");
    if (id != NULL && id[0] != '\0') {
        kbo_window_text_appendf(buffer, " id='");
        kbo_html_append_escaped(buffer, id);
        kbo_window_text_appendf(buffer, "'");
    }
    kbo_window_text_appendf(buffer, "><summary><span>");
    kbo_html_append_escaped(buffer, current_label != NULL ? current_label : "");
    kbo_window_text_appendf(buffer, "</span></summary><div class='ootpChoiceMenu'>");
}

void kbo_webview_append_ootp_choice_option(
    KboWindowTextBuffer* buffer,
    const char* href,
    const char* label,
    int selected)
{
    if (buffer == NULL || href == NULL || label == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "<a class='ootpChoiceOption%s' href='", selected ? " selected" : "");
    kbo_html_append_escaped(buffer, href);
    kbo_window_text_appendf(buffer, "'>");
    kbo_html_append_escaped(buffer, label);
    kbo_window_text_appendf(buffer, "</a>");
}

void kbo_webview_end_ootp_choice(KboWindowTextBuffer* buffer)
{
    if (buffer == NULL) {
        return;
    }
    kbo_window_text_appendf(buffer, "</div></details>");
}
