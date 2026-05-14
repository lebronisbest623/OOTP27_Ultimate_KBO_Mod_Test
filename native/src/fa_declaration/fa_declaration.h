#ifndef KBOFIX_SRC_FA_DECLARATION_FA_DECLARATION_H_
#define KBOFIX_SRC_FA_DECLARATION_FA_DECLARATION_H_

#include <stdint.h>

typedef struct KboFaDeclarationDecision {
    uint32_t player_id;
    uint32_t declaration_date;
    uint32_t season;
    uint32_t declared;
    uint32_t team_id;
    uint32_t league_id;
} KboFaDeclarationDecision;

int kbo_handle_fa_declaration_event(uint32_t event_yyyymmdd, const char* source);
int kbo_fa_declaration_find_latest_decision(
    uint32_t player_id,
    uint32_t season,
    KboFaDeclarationDecision* out_decision);

#endif
