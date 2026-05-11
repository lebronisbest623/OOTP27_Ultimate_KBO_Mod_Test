#include "../../hotkey_window_runtime_internal.h"
#include "../../hotkey_window_domain_contract.h"

const char* kbo_hub_foreign_slot_code_for_player(uint8_t* player)
{
    if (player != NULL && memory_range_readable(player, OOTP27_PLAYER_SCAN_BYTES)) {
        uint8_t replacement_slot_type = 0u;
        if (kbo_foreign_replacement_player_seed_matches_loaded(player, &replacement_slot_type)) {
            return "REPL";
        }

        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id != 0u) {
            int is_replacement = 0;
            kbo_ensure_foreign_injury_replacements_loaded();
            kbo_lock_foreign_injury_replacements();
            for (int i = 0; i < g_kbo_foreign_injury_replacement_count; i++) {
                KboForeignInjuryReplacement* rec = &g_kbo_foreign_injury_replacements[i];
                if (rec->replacement_player_id == player_id
                        && kbo_foreign_injury_status_uses_slot(rec->status)) {
                    is_replacement = 1;
                    break;
                }
            }
            kbo_unlock_foreign_injury_replacements();
            if (is_replacement) {
                return "REPL";
            }
        }
    }
    return kbo_player_is_asian_quota_candidate(player) ? "AQ" : "REG";
}

