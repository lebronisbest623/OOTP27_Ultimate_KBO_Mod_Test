#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_PENALTY_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_PENALTY_H_

#include <stdint.h>

/* Returns the draft penalty stages for a team based on CBT records.
   Returns 0 if the team has no pending draft penalty, or N (e.g. 9)
   if their most recent CBT season had a violation with enough consecutive
   violations to trigger the draft penalty. */
int kbo_cbt_draft_penalty_stages_for_team(uint32_t team_id);

/* Clears the in-memory cache (call when changing saves). */
void kbo_cbt_draft_penalty_cache_clear(void);

#endif
