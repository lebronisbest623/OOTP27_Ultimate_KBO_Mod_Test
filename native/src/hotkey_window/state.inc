/* F2 hub state assembler. */

#include "state_types.inc"
#include "state_window.inc"
#include "state_gdi.inc"
#include "state_view.inc"
#include "state_skin.inc"
#include "state_constants.inc"
static const char* kbo_hub_text(const char* ko, const char* en)
{
    return g_kbo_hub_language == KBO_HUB_LANG_EN ? en : ko;
}
#include "state_team_vector.h"
#include "state_language.inc"
#include "state_nav.inc"
#include "state_text_utils.h"
#include "state_league_lookup/league_lookup_state.inc"
#include "state_league_lookup/league_display_cache.inc"
#include "state_league_lookup/league_ptr_cache.inc"
#include "state_league_lookup/league_name_score.inc"
#include "state_league_lookup/league_memory_scan.inc"
#include "state_league_lookup/league_visible_ids.inc"
#include "state_league_lookup/league_multi_scan.inc"
#include "state_league_lookup/league_prewarm.inc"
#include "state_league_lookup/league_find_ptr.inc"
#include "state_league_name.inc"
static void kbo_hub_ensure_valid_selection(void);
static void kbo_refresh_hotkey_window(void);
static int kbo_resolve_foreign_waiver_top_candidate_for_team(uint32_t team_id, uint32_t* out_player_id, uint32_t* out_current_team_id);
static int kbo_append_foreign_waiver_user_decision(uint32_t team_id, uint32_t player_id, int retain);
static int kbo_is_foreign_waiver_negotiation_window_open(void);
