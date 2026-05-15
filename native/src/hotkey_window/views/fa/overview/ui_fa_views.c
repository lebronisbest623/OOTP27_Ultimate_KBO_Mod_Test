#include "../ui_fa_views_internal.h"

void kbo_webview_append_fa_view(
    KboWindowTextBuffer* buffer,
    int selected_fa_subview,
    int selected_fa_compensation_subview,
    uint32_t selected_compensation_player_id,
    uint32_t selected_league_id)
{
    KBO_PROFILE_BEGIN(profile_fa_view);
    if (selected_fa_subview == KBO_HUB_FA_SUBVIEW_COMPENSATION) {
        kbo_webview_append_fa_compensation_view(
            buffer,
            selected_fa_compensation_subview,
            selected_compensation_player_id);
        KBO_PROFILE_END(profile_fa_view, "webview.fa_view.compensation");
    } else if (selected_fa_subview == KBO_HUB_FA_SUBVIEW_RIGHTS_EXERCISE) {
        kbo_webview_append_fa_rights_exercise_view(buffer, selected_league_id);
        KBO_PROFILE_END(profile_fa_view, "webview.fa_view.rights_exercise");
    } else {
        kbo_webview_append_fa_cases_view(buffer, selected_league_id);
        KBO_PROFILE_END(profile_fa_view, "webview.fa_view.market");
    }
}
