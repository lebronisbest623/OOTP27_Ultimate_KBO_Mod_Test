#include "../internal/ui_military_view_internal.h"

void kbo_webview_append_military_view(KboWindowTextBuffer* buffer, int selected_military_subview, uint32_t* selected_results_year)
{
    if (buffer == NULL) {
        return;
    }

    if (selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_RESULTS) {
        kbo_webview_append_military_results_view(buffer, selected_results_year);
        return;
    }

    if (selected_military_subview == KBO_HUB_MILITARY_SUBVIEW_APPLICANTS) {
        kbo_webview_append_military_applicants_view(buffer);
        return;
    }

    kbo_webview_append_military_roster_view(buffer);
}
