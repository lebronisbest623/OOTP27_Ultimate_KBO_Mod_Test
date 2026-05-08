#ifndef KBOFIX_SRC_CORE_CORE_TEAM_COLLECT_H_
#define KBOFIX_SRC_CORE_CORE_TEAM_COLLECT_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int collect_kbo_league_team_ids(uint32_t league_id, uint32_t* team_ids, int max_team_ids, int* out_scanned, int* out_unreadable);

#endif
