/* All-Star shared typedefs, state, and layout selection. */

typedef void* (__fastcall *OotpStringAssignFn)(void* string_object, const char* text);
typedef void (__fastcall *OotpVectorPushBack)(void* vector, void* value);

typedef struct KboAllstarTeamRow {
    uint16_t year;
    char team_id[16];
    char team_name[96];
    char current_city[64];
    uint8_t side;
} KboAllstarTeamRow;

typedef struct KboAllstarLayout {
    uint32_t rules_flag_offset;
    uint32_t game_flag_offset;
    uint32_t auto_schedule_offset;
    uint32_t team_a_offset;
    uint32_t team_b_offset;
    uint32_t subleague_array_offset;
    uint32_t subleague_count_offset;
    uint32_t league_id_primary_offset;
    uint32_t league_id_fallback_offset;
} KboAllstarLayout;

static KboAllstarTeamRow g_allstar_team_rows[OOTP27_KBO_MAX_ALLSTAR_TEAM_ROWS];
static int g_allstar_team_row_count = 0;
static LONG g_allstar_team_rules_loaded = 0;
static LONG g_allstar_candidate_seed_log_count = 0;
static LONG g_allstar_voting_prepare_log_count = 0;
static LONG g_allstar_team_name_log_count = 0;

static KboAllstarLayout kbo_get_allstar_layout(void)
{
    OotpBuildInfo info = read_ootp_build_info();
    if (info.ok && info.timestamp == 0x69F75E6Bu && info.size_of_image == 0x03919000u) {
        KboAllstarLayout may_layout = {
            0x242u, 0x45f0u, 0x45f1u, 0x45f8u, 0x45fcu,
            0x4a38u, 0x4a44u, 0x4cc8u, 0x4cc0u
        };
        return may_layout;
    }

    KboAllstarLayout april_layout = {
        0x242u, 0x45e8u, 0x45e9u, 0x45f0u, 0x45f4u,
        0x4a30u, 0x4a3cu, 0x4cc0u, 0x4cc8u
    };
    return april_layout;
}
