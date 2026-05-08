#ifndef KBO_CORE_SQL_HISTORY_TRANSACTIONS_H
#define KBO_CORE_SQL_HISTORY_TRANSACTIONS_H

#include <stdint.h>

int insert_kbo_player_history_sql(
    uint32_t player_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    const char* text,
    const char* source);

int insert_kbo_roster_transaction_sql(
    uint32_t league_id,
    uint32_t team_id,
    uint32_t year,
    uint32_t month,
    uint32_t day,
    uint32_t transaction_type,
    const char* league_text,
    const char* team_text,
    const char* source);

#endif
