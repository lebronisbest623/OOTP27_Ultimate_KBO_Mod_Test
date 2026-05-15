#ifndef KBOFIX_SRC_FOREIGN_INJURY_FOREIGN_INJURY_H_
#define KBOFIX_SRC_FOREIGN_INJURY_FOREIGN_INJURY_H_

#include <stdint.h>

#define KBO_FOREIGN_INJURY_REPLACEMENT_MAX      256
#ifndef KBO_FOREIGN_INJURY_SLOT_REGULAR
#define KBO_FOREIGN_INJURY_SLOT_REGULAR         1
#endif
#ifndef KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA     2
#endif
#define KBO_FOREIGN_INJURY_STATUS_OPEN          1
#define KBO_FOREIGN_INJURY_STATUS_ACTIVE        2
#define KBO_FOREIGN_INJURY_STATUS_PENDING       3
#define KBO_FOREIGN_INJURY_STATUS_CLOSED        4

typedef struct KboForeignInjuryReplacement {
    uint32_t team_id;
    uint32_t league_id;
    uint32_t injured_player_id;
    uint32_t replacement_player_id;
    uint32_t opened_on_yyyymmdd;
    uint32_t expected_end_yyyymmdd;
    uint8_t  slot_type;
    uint8_t  status;
    uint8_t  converted;
} KboForeignInjuryReplacement;

extern KboForeignInjuryReplacement g_kbo_foreign_injury_replacements[KBO_FOREIGN_INJURY_REPLACEMENT_MAX];
extern int g_kbo_foreign_injury_replacement_count;

const char* kbo_foreign_injury_slot_label(uint8_t slot_type);
const char* kbo_foreign_injury_status_label(uint8_t status);
int kbo_foreign_injury_status_uses_slot(uint8_t status);
uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player);
int kbo_foreign_injury_inactive_roster_has_long_term_injury_basis(
    uint8_t injury_active,
    int16_t days_left,
    int min_days,
    int inactive_roster_present);
int kbo_foreign_injury_duration_meets_minimum(int16_t days_left, int min_days);
int kbo_foreign_injury_player_excluded_from_foreign_count_locked(uint32_t team_id, uint32_t player_id);
int kbo_foreign_injury_player_excluded_from_foreign_count(uint32_t team_id, uint32_t player_id);
void kbo_lock_foreign_injury_replacements(void);
void kbo_unlock_foreign_injury_replacements(void);

int kbo_foreign_injury_replacement_enabled(void);
void kbo_ensure_foreign_injury_replacements_loaded(void);
int kbo_foreign_injury_replacements_loaded_for_current_save(void);
int kbo_team_has_foreign_injury_slot_for_candidate(
    uint32_t team_id,
    uint8_t slot_type,
    uint32_t candidate_player_id,
    uint32_t* out_injured_player_id,
    uint32_t* out_replacement_player_id);
int kbo_foreign_injury_replacement_signing_exception_available(
    uint32_t team_id,
    uint8_t* candidate,
    uint8_t* out_slot_type,
    uint32_t* out_injured_player_id,
    uint32_t* out_effective_count,
    uint32_t* out_effective_limit);
int kbo_attach_foreign_injury_replacement_after_signing(
    uint32_t team_id,
    uint8_t* replacement,
    uint8_t slot_type,
    uint32_t injured_player_id,
    const char* source);
int kbo_foreign_injury_replacement_callup_exception_available(
    uintptr_t team_ptr,
    uint8_t* candidate,
    int32_t ootp_limit,
    int check_type,
    uint32_t* out_effective_after,
    uint8_t* out_slot_type);
void kbo_count_foreign_injury_replacements_for_team(
    uint32_t team_id,
    int* out_open,
    int* out_pending,
    int* out_closed);
void kbo_foreign_injury_replacement_scan_once(const char* source);
void start_kbo_foreign_injury_replacement_thread(void);

#endif
