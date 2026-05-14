#ifndef KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_ORDER_CBT_DRAFT_ORDER_LEDGER_H_
#define KBOFIX_SRC_COMPETITIVE_BALANCE_TAX_DRAFT_ORDER_CBT_DRAFT_ORDER_LEDGER_H_

#include <stdint.h>

#define KBO_CBT_DRAFT_ORDER_TARGET_ROUND 1u

typedef struct KboCbtDraftOrderMove {
    uint32_t season;
    uint32_t team_id;
    uint32_t stages;
    uint16_t from_pick;
    uint16_t to_pick;
} KboCbtDraftOrderMove;

int kbo_cbt_draft_order_append_ledger(const KboCbtDraftOrderMove* move, const char* source);

#endif
