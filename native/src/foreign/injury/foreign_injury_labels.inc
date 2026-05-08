/* Foreign injury replacement labels, slot helpers, and lock helpers. Included from native/KBOFix.c. */

static const char* kbo_foreign_injury_slot_label(uint8_t slot_type)
{
    return slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA ? "Asian quota" : "Regular";
}

static const char* kbo_foreign_injury_status_label(uint8_t status)
{
    switch (status) {
    case KBO_FOREIGN_INJURY_STATUS_OPEN:    return "Open";
    case KBO_FOREIGN_INJURY_STATUS_ACTIVE:  return "Active";
    case KBO_FOREIGN_INJURY_STATUS_PENDING: return "Decision due";
    case KBO_FOREIGN_INJURY_STATUS_CLOSED:  return "Closed";
    default:                                return "Unknown";
    }
}

static int kbo_foreign_injury_status_uses_slot(uint8_t status)
{
    return status == KBO_FOREIGN_INJURY_STATUS_OPEN || status == KBO_FOREIGN_INJURY_STATUS_ACTIVE;
}

static uint8_t kbo_foreign_injury_slot_type_for_player(uint8_t* player)
{
    return kbo_player_is_asian_quota_candidate(player)
        ? KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
        : KBO_FOREIGN_INJURY_SLOT_REGULAR;
}

static void kbo_lock_foreign_injury_replacements(void)
{
    while (InterlockedCompareExchange(&g_kbo_foreign_injury_replacement_lock, 1, 0) != 0) {
        Sleep(0);
    }
}

static void kbo_unlock_foreign_injury_replacements(void)
{
    InterlockedExchange(&g_kbo_foreign_injury_replacement_lock, 0);
}
