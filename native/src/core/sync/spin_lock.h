#ifndef KBOFIX_SRC_CORE_SYNC_SPIN_LOCK_H_
#define KBOFIX_SRC_CORE_SYNC_SPIN_LOCK_H_

#include "lock.h"

typedef KboLock KboSpinLock;

#define KBO_SPIN_LOCK_INIT KBO_LOCK_INIT

static inline void kbo_spin_lock(KboSpinLock* lock)
{
    kbo_lock_enter(lock);
}

static inline void kbo_spin_unlock(KboSpinLock* lock)
{
    kbo_lock_leave(lock);
}

#endif
