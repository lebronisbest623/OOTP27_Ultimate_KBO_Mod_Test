#ifndef KBOFIX_SRC_CORE_SYNC_LOCK_H_
#define KBOFIX_SRC_CORE_SYNC_LOCK_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef SRWLOCK_INIT
typedef struct _RTL_SRWLOCK {
    PVOID Ptr;
} RTL_SRWLOCK, *PRTL_SRWLOCK;
typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT {0}
WINBASEAPI VOID WINAPI AcquireSRWLockExclusive(PSRWLOCK lock);
WINBASEAPI VOID WINAPI ReleaseSRWLockExclusive(PSRWLOCK lock);
WINBASEAPI BOOLEAN WINAPI TryAcquireSRWLockExclusive(PSRWLOCK lock);
WINBASEAPI VOID WINAPI AcquireSRWLockShared(PSRWLOCK lock);
WINBASEAPI VOID WINAPI ReleaseSRWLockShared(PSRWLOCK lock);
#endif

typedef SRWLOCK KboLock;
typedef SRWLOCK KboRwLock;

#define KBO_LOCK_INIT SRWLOCK_INIT
#define KBO_RW_LOCK_INIT SRWLOCK_INIT

static inline void kbo_lock_enter(KboLock* lock)
{
    if (lock != NULL) {
        AcquireSRWLockExclusive(lock);
    }
}

static inline int kbo_lock_try_enter(KboLock* lock)
{
    return lock != NULL && TryAcquireSRWLockExclusive(lock) != FALSE;
}

static inline void kbo_lock_leave(KboLock* lock)
{
    if (lock != NULL) {
        ReleaseSRWLockExclusive(lock);
    }
}

static inline void kbo_rw_lock_enter_shared(KboRwLock* lock)
{
    if (lock != NULL) {
        AcquireSRWLockShared(lock);
    }
}

static inline void kbo_rw_lock_leave_shared(KboRwLock* lock)
{
    if (lock != NULL) {
        ReleaseSRWLockShared(lock);
    }
}

static inline void kbo_rw_lock_enter_exclusive(KboRwLock* lock)
{
    if (lock != NULL) {
        AcquireSRWLockExclusive(lock);
    }
}

static inline void kbo_rw_lock_leave_exclusive(KboRwLock* lock)
{
    if (lock != NULL) {
        ReleaseSRWLockExclusive(lock);
    }
}

#endif
