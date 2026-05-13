#ifndef KBOFIX_SRC_MILITARY_SERVICE_RUNTIME_ASSIGNMENT_H_
#define KBOFIX_SRC_MILITARY_SERVICE_RUNTIME_ASSIGNMENT_H_

#include <stdint.h>

int kbo_military_daily_roster_mutation_window_ready(
    uint32_t today_serial,
    int32_t player_count);
int kbo_apply_military_service_seed_assignments(uint8_t* sang, uint8_t* kpb, const char* source);
int kbo_release_invalid_military_service_team_assignment(
    uint8_t* player,
    uint8_t* service_team,
    uint32_t service_team_id,
    const char* source,
    uint32_t vector_offset);

#endif
