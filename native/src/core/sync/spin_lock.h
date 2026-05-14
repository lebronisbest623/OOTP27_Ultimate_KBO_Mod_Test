#ifndef KBOFIX_SRC_CORE_SYNC_SPIN_LOCK_H_
#define KBOFIX_SRC_CORE_SYNC_SPIN_LOCK_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static inline void kbo_spin_lock(volatile LONG* lock)
{
    if (lock == NULL) {
        return;
    }
    while (InterlockedCompareExchange(lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static inline void kbo_spin_unlock(volatile LONG* lock)
{
    if (lock != NULL) {
        InterlockedExchange(lock, 0);
    }
}

#endif
