#ifndef KBOFIX_MILITARY_SELECTION_RULE_AUDIT_H_
#define KBOFIX_MILITARY_SELECTION_RULE_AUDIT_H_

#include <stdint.h>

void kbo_military_selection_audit_flow(
    const char* rule,
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t entry_year,
    uint32_t sang_id,
    int active_before,
    int slots,
    int queued,
    int considered,
    int routed,
    int refreshed,
    int seeded,
    int returned,
    int news_created,
    uint32_t vector_offset);

void kbo_military_selection_audit_player(
    const char* decision,
    const char* reason,
    const char* source,
    uint32_t entry_year,
    uint32_t player_id,
    uint32_t original_team_id,
    uint32_t service_team_id,
    int score,
    int called_pre_change,
    int called_register,
    int called_attach);

#endif
