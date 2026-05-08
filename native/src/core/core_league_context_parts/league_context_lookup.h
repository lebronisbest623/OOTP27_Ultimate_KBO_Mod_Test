#ifndef KBOFIX_SRC_CORE_CORE_LEAGUE_CONTEXT_PARTS_LEAGUE_CONTEXT_LOOKUP_H_
#define KBOFIX_SRC_CORE_CORE_LEAGUE_CONTEXT_PARTS_LEAGUE_CONTEXT_LOOKUP_H_

#include <stdint.h>

int kbo_league_candidate_matches_id(uintptr_t candidate, uint32_t league_id, uint32_t id_offset);
int kbo_league_scan_protect_allows_read(uint32_t protect);
uint32_t kbo_read_kbo_league_id_override(void);
uintptr_t kbo_find_league_ptr_by_memory_scan(uint32_t league_id);
uintptr_t kbo_find_league_ptr_from_id(uint32_t league_id);
uintptr_t kbo_find_league_ptr_from_global_vectors(uint32_t league_id);
uint32_t kbo_find_current_kbo_league_year(void);
uint32_t kbo_find_league_year_from_id(uint32_t league_id);
uint32_t kbo_find_league_year_from_id_no_scan(uint32_t league_id);
uint32_t kbo_resolve_kbo_league_id(void);

#endif
