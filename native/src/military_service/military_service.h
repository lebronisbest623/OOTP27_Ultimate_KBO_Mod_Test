#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_H_

#include <stdint.h>
#include <windows.h>

#include "players/loans/military_active_loan.h"
#include "selection/draft/military_draft_queue.h"
#include "selection/fa_policy/military_fa_policy.h"
#include "selection/events/military_selection_policy.h"

#define KBO_MILITARY_SERVICE_DAYS kbo_military_service_days()

void start_kbo_military_seed_bootstrap_thread(void);
void start_kbo_military_days_tick_thread(void);

#endif
