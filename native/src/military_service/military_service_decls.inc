/* Military service forward declarations and hook typedefs. Included from native/src/military_service_loan.inc. */

typedef void (__fastcall *OotpMilitaryServiceEntryFn)(void* player);

static int32_t kbo_military_days_left(uint8_t* player);
static int32_t kbo_military_effective_days_left(uint8_t* player);
static uint32_t kbo_military_effective_return_yyyymmdd(uint8_t* player);
static void    kbo_set_military_days_left(uint8_t* player, int32_t days_left);
static int     find_active_kbo_military_loan_index(uint32_t player_id);
static void    register_active_kbo_military_loan(
    uint32_t player_id,
    uintptr_t player_ptr,
    uint32_t original_team_id,
    uint32_t original_league_id,
    uint32_t service_team_id,
    uint32_t service_league_id);
static void    clear_kbo_military_unavailable_flags(uint8_t* player);
static uint8_t* kbo_military_find_player_by_id(uint32_t player_id);

