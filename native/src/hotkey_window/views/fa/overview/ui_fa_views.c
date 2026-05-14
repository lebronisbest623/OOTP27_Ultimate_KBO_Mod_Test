#include "../ui_fa_views_internal.h"

void kbo_webview_append_fa_view(
    KboWindowTextBuffer* buffer,
    int selected_fa_subview,
    uint32_t selected_compensation_player_id,
    uint32_t selected_league_id)
{
    KBO_PROFILE_BEGIN(profile_fa_view);
    if (selected_fa_subview == KBO_HUB_FA_SUBVIEW_COMPENSATION) {
        kbo_webview_append_fa_compensation_view(buffer, selected_compensation_player_id);
        KBO_PROFILE_END(profile_fa_view, "webview.fa_view.compensation");
    } else {
        kbo_webview_append_fa_cases_view(buffer, selected_league_id);
        KBO_PROFILE_END(profile_fa_view, "webview.fa_view.market");
    }
}
