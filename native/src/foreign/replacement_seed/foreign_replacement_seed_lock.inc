/* Foreign replacement-player seed lock helpers. Included from native/KBOFix.c. */

static void kbo_lock_foreign_replacement_player_seeds(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_replacement_player_seed_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_foreign_replacement_player_seeds(void)
{
    InterlockedExchange(&g_kbo_foreign_replacement_player_seed_lock, 0);
}

