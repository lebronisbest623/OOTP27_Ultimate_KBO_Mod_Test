#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_ACTIVE_LOAN_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_ACTIVE_LOAN_H_

#include <stdint.h>
#include <windows.h>

typedef struct KboMilitaryActiveLoan {
    uint32_t player_id;
    uint32_t original_team_id;
    uint32_t original_league_id;
    uint32_t service_team_id;
    uint32_t service_league_id;
    uint32_t service_start_date_serial;
    uint32_t service_return_date_serial;
    int32_t  service_total_days;
    uintptr_t player_ptr;
} KboMilitaryActiveLoan;

extern LONG g_active_military_loan_count;

int find_active_kbo_military_loan_index(uint32_t player_id);
KboMilitaryActiveLoan* kbo_active_military_loan_at(int index);
void register_active_kbo_military_loan(
    uint32_t player_id,
    uintptr_t player_ptr,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t service_league_id);
void unregister_active_kbo_military_loan(uint32_t player_id);
int kbo_player_is_registered_active_military_loan(uintptr_t player_ptr);

#endif
