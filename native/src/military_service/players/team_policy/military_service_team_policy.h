#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_TEAM_POLICY_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_TEAM_POLICY_H_

#include <stdint.h>

int kbo_team_id_is_military_service_team(uint32_t team_id);
int kbo_team_ptr_is_military_service_team(uint8_t* team);
void kbo_load_military_service_team_policy_override_once(void);

#endif
