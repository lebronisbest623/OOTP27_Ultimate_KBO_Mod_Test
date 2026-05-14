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
    uint8_t contract_level;
    int32_t salary;
    int32_t fa_demand;
    int32_t score;
} KboFaDeclarationDecision;

int kbo_handle_fa_declaration_event(uint32_t event_yyyymmdd, const char* source);
int kbo_fa_declaration_find_latest_decision(
    uint32_t player_id,
    uint32_t season,
    KboFaDeclarationDecision* out_decision);
int kbo_fa_declaration_repair_retained_contract_salary(
    uint8_t* player,
    uint32_t season,
    const KboFaDeclarationDecision* decision,
    int32_t minimum_salary,
    const char* source);
int kbo_fa_declaration_repair_retained_contracts_for_season(
    uint32_t season,
    const char* source);

#endif
