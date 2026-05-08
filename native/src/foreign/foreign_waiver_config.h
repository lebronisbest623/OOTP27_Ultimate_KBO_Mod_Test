#ifndef KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_CONFIG_H_
#define KBOFIX_SRC_FOREIGN_FOREIGN_WAIVER_CONFIG_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint32_t read_u32_leading_number_from_file(const char* filename);
uint32_t kbo_get_foreign_waiver_auto_target_team_id(void);
int kbo_is_forced_foreign_candidate_id(uint32_t player_id);

#endif
