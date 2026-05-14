#ifndef KBOFIX_SRC_CORE_CORE_LEAGUE_CONTEXT_PARTS_RESOLVE_NAMED_SCAN_LEAGUE_NAMED_SCAN_H_
#define KBOFIX_SRC_CORE_CORE_LEAGUE_CONTEXT_PARTS_RESOLVE_NAMED_SCAN_LEAGUE_NAMED_SCAN_H_

#include <stdint.h>
#include <stddef.h>

#define KBO_CORE_NAMED_LEAGUE_SCAN_MIN_SCORE 90

int kbo_core_named_league_candidate_score(
    uintptr_t candidate,
    uint32_t league_id,
    char* out_name,
    size_t out_name_size);
uintptr_t kbo_find_named_league_ptr_by_memory_scan_all(uint32_t league_id);

#endif
