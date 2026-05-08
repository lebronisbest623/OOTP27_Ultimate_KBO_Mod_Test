/* Submit-offer probe hook. Included from native/KBOFix.c. */

typedef void (__fastcall *OotpFaSubmitOfferProbeFn)(void* screen);
typedef int (__fastcall *OotpFaOfferScreenCallbackProbeFn)(void* screen, void* sender, uintptr_t callback_id, uintptr_t value);
typedef int (__fastcall *OotpFaContractOfferCallbackProbeFn)(void* offer, void* sender, uintptr_t callback_id, uintptr_t value);
typedef uint8_t (__fastcall *OotpPlayerActionEligibilityFn)(void* action_context, int32_t action_id, uint8_t strict_check);
typedef uint8_t* (__fastcall *OotpLeagueFinancialsLookupFn)(void* global_db, int32_t league_id);

static LONG g_kbo_no_minor_contract_demand_floor_enabled = 0;

typedef struct KboFinancialSalaryLadderSnapshot {
    uint8_t* financials;
    int32_t values[9];
    LONG active;
} KboFinancialSalaryLadderSnapshot;

typedef struct KboForeignFaDemandRemapRecord {
    uint32_t player_id;
    int32_t original_demand;
    int32_t mapped_demand;
} KboForeignFaDemandRemapRecord;

static KboFinancialSalaryLadderSnapshot g_kbo_foreign_fa_demand_ladder_snapshot = {0};
static KboForeignFaDemandRemapRecord g_kbo_foreign_fa_demand_remap_records[512] = {0};
static volatile LONG g_kbo_foreign_fa_demand_remap_record_cursor = 0;

static const uint32_t KBO_FINANCIALS_SALARY_LADDER_OFFSETS[9] = {
    OOTP27_FINANCIALS_SALARY_LADDER_MINIMUM_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_POOR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_FAIR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_BELOW_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_ABOVE_AVERAGE_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_GOOD_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_STAR_OFFSET,
    OOTP27_FINANCIALS_SALARY_LADDER_SUPERSTAR_OFFSET
};

static int32_t kbo_no_minor_resolve_current_league_id(void);
static int kbo_no_minor_player_is_teamless_demand_floor_candidate(uint8_t* player, uint32_t league_id);
static void kbo_restore_foreign_fa_demand_salary_ladder(const char* source);
static uint8_t* kbo_resolve_current_league_financials(uint32_t* out_league_id);
