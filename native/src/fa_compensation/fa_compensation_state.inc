/* KBO FA compensation obligation ledger. */

#define KBO_FA_COMPENSATION_MAX 1024
#define KBO_FA_COMPENSATION_STATUS_PENDING 0u
#define KBO_FA_COMPENSATION_STATUS_RECORDED 1u
#define KBO_FA_COMPENSATION_STATUS_PLAYER_SELECTED 2u
#define KBO_FA_COMPENSATION_STATUS_PLAYER_TRANSFERRED 3u
#define KBO_FA_COMPENSATION_STATUS_CASH_ONLY_SELECTED 4u
#define KBO_FA_COMPENSATION_STATUS_CASH_ONLY_RECORDED 5u
#define KBO_FA_COMPENSATION_PROTECTED_LIST_MAX 128

static volatile LONG g_kbo_fa_compensation_ledger_lock = 0;

static void kbo_fa_compensation_lock_ledger(const char* source)
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

static void kbo_fa_compensation_unlock_ledger(void)
{
    InterlockedExchange(&g_kbo_fa_compensation_ledger_lock, 0);
}

typedef struct KboFaCompensationRecord {
    uint32_t player_id;
    uint32_t signed_on_yyyymmdd;
    uint32_t season;
    uint32_t league_id;
    uint32_t original_team_id;
    uint32_t signing_team_id;
    char grade[12];
    int32_t previous_salary;
    uint32_t cash_with_player;
    uint32_t cash_only;
    uint32_t protect_count;
    uint8_t requires_player_compensation;
    uint8_t status;
    char case_label[48];
    char player_name[96];
    char source[48];
} KboFaCompensationRecord;

typedef struct KboFaProtectedCandidate {
    uint32_t player_id;
    uint32_t team_id;
    uint16_t age;
    uint8_t role;
    int32_t score;
    char reason[96];
    char player_name[96];
} KboFaProtectedCandidate;

typedef struct KboFaCompensationProtectionDebugRow {
    uint32_t fa_player_id;
    uint32_t season;
    char grade[12];
    uint32_t original_team_id;
    uint32_t signing_team_id;
    uint32_t due_on_yyyymmdd;
    uint32_t generated_on_yyyymmdd;
    char list_type[24];
    uint32_t rank;
    uint32_t player_id;
    char player_name[96];
    int32_t score;
    uint16_t age;
    uint8_t role;
    char reason[96];
    char source[48];
} KboFaCompensationProtectionDebugRow;

typedef struct KboFaCompensationDecisionRow {
    uint32_t fa_player_id;
    uint32_t season;
    char grade[12];
    uint32_t original_team_id;
    uint32_t signing_team_id;
    uint32_t signed_on_yyyymmdd;
    uint32_t due_on_yyyymmdd;
    uint32_t decided_on_yyyymmdd;
    char action[24];
    uint32_t selected_player_id;
    char selected_player_name[96];
    int32_t selected_player_score;
    int unprotected_candidate_count;
    uint32_t cash_with_player;
    uint32_t cash_only;
    char source[48];
} KboFaCompensationDecisionRow;

typedef struct KboFaCompensationDueTask {
    KboFaCompensationRecord rec;
    uint8_t action;
} KboFaCompensationDueTask;

#define KBO_FA_COMPENSATION_DUE_TASK_SUBMIT_LIST 1u
#define KBO_FA_COMPENSATION_DUE_TASK_RECORD_CASH 2u
#define KBO_FA_COMPENSATION_DUE_TASK_TRANSFER_PLAYER 3u
