#include "../internal/captain_selection_internal.h"
#include "../../core/sync/spin_lock.h"

void kbo_lock_captain_seeds(void)
{
    kbo_spin_lock(&g_kbo_captain_seed_lock);
}

void kbo_unlock_captain_seeds(void)
{
    kbo_spin_unlock(&g_kbo_captain_seed_lock);
}

