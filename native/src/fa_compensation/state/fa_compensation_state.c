#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../bootstrap/profiling/profiler.h"
#include "../../core/logging/core_log.h"
#include "../../core/sync/lock.h"
#include "fa_compensation_state.h"

static KboLock g_kbo_fa_compensation_ledger_lock = KBO_LOCK_INIT;

void kbo_fa_compensation_lock_ledger(const char* source)
{
    KBO_PROFILE_BEGIN(profile_fa_comp_lock_wait);
    DWORD start = GetTickCount();
    while (!kbo_lock_try_enter(&g_kbo_fa_compensation_ledger_lock)) {
        if ((GetTickCount() - start) > 5000u) {
            kbo_log_runtimef(
                "KBO FA compensation ledger lock waiting source=%s wait_ms=%lu",
                source != NULL ? source : "",
                (unsigned long)(GetTickCount() - start));
            start = GetTickCount();
        }
        Sleep(1);
    }
    KBO_PROFILE_END(profile_fa_comp_lock_wait, "fa_comp.ledger.lock_wait");
}

void kbo_fa_compensation_unlock_ledger(void)
{
    kbo_lock_leave(&g_kbo_fa_compensation_ledger_lock);
}
