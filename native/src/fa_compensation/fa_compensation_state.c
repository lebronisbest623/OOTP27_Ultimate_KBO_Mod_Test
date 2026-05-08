#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../bootstrap/profiler.h"
#include "../core/core_log.h"
#include "fa_compensation_state.h"

static volatile LONG g_kbo_fa_compensation_ledger_lock = 0;

void kbo_fa_compensation_lock_ledger(const char* source)
{
    KBO_PROFILE_BEGIN(profile_fa_comp_lock_wait);
    DWORD start = GetTickCount();
    while (InterlockedCompareExchange(&g_kbo_fa_compensation_ledger_lock, 1, 0) != 0) {
        if ((GetTickCount() - start) > 5000u) {
            append_logf(
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
    InterlockedExchange(&g_kbo_fa_compensation_ledger_lock, 0);
}
