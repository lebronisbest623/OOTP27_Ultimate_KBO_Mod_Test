#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_H_

#include <stdint.h>
#include <windows.h>

#include "military_active_loan.h"
#include "military_draft_queue.h"
#include "military_fa_policy.h"

#define KBO_MILITARY_SERVICE_DAYS 545

void start_kbo_military_seed_bootstrap_thread(void);
void start_kbo_military_days_tick_thread(void);

#endif
