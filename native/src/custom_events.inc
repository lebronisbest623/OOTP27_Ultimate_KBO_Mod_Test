#include <stdint.h>

static LONG g_kbo_custom_event_monitor_started = 0;
static uint32_t g_kbo_foreign_priority_last_scheduled_date = 0;
static uint32_t g_kbo_military_selection_last_scheduled_date = 0;
static uint32_t g_kbo_foreign_priority_last_open_event_fired_date = 0;
static uint32_t g_kbo_foreign_priority_last_close_event_fired_date = 0;
static uintptr_t g_kbo_processed_event_ptrs[256] = {0};
static LONG g_kbo_processed_event_count = 0;

static const char g_kbo_foreign_priority_open_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Begins";
static const char g_kbo_foreign_priority_close_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Ends";
static const char g_kbo_military_selection_event_title[] =
    "[KBO] Military Service Selection";

static int kbo_announce_foreign_waiver_results(uint32_t event_yyyymmdd, const char* source);

static int kbo_custom_event_name_is_open(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_open_event_title);
}

static int kbo_custom_event_name_is_close(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_close_event_title);
}

static uint32_t kbo_get_latest_offseason_starts_event(uint32_t today_yyyymmdd)
{
    if (today_yyyymmdd == 0u) {
        return 0u;
    }
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0u;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0u;
    }

    uint32_t latest_offseason_start = 0u;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        if (event_year < 1980u || event_year > 2200u || event_month < 1u || event_month > 12u || event_day < 1u || event_day > 31u) {
            continue;
        }

        uint32_t event_yyyymmdd = event_year * 10000u + event_month * 100u + event_day;
        if (event_yyyymmdd > today_yyyymmdd) {
            continue;
        }

        char name[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }

        if (_stricmp(name, "Offseason starts") == 0 && event_yyyymmdd > latest_offseason_start) {
            latest_offseason_start = event_yyyymmdd;
        }
    }

    return latest_offseason_start;
}

static int kbo_custom_event_exists_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    int expect_open)
{
    if (event_yyyymmdd == 0u) {
        return 0;
    }

    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0
            || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)(event_yyyymmdd / 10000u)) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)((event_yyyymmdd / 100u) % 100u)) {
            continue;
        }
        if (event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)(event_yyyymmdd % 100u)) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }

        char name[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }
        if (expect_open ? kbo_custom_event_name_is_open(name) : kbo_custom_event_name_is_close(name)) {
            return 1;
        }
    }

    return 0;
}

static int kbo_custom_event_name_matches_local(const char* name)
{
    return kbo_custom_event_name_is_open(name) || kbo_custom_event_name_is_close(name);
}

static int kbo_custom_event_already_processed(uintptr_t event_ptr)
{
    LONG count = g_kbo_processed_event_count;
    if (count < 0) {
        count = 0;
    }
    if (count > (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]))) {
        count = (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]));
    }

    for (LONG i = 0; i < count; i++) {
        if (g_kbo_processed_event_ptrs[i] == event_ptr) {
            return 1;
        }
    }
    return 0;
}

static void kbo_mark_custom_event_processed(uintptr_t event_ptr)
{
    if (event_ptr == 0 || kbo_custom_event_already_processed(event_ptr)) {
        return;
    }

    LONG slot = InterlockedIncrement(&g_kbo_processed_event_count) - 1;
    if (slot < 0 || slot >= (LONG)(sizeof(g_kbo_processed_event_ptrs) / sizeof(g_kbo_processed_event_ptrs[0]))) {
        InterlockedDecrement(&g_kbo_processed_event_count);
        return;
    }
    g_kbo_processed_event_ptrs[slot] = event_ptr;
}

static int kbo_schedule_foreign_priority_custom_events(const char* source)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return 0;
    }

    uint32_t today = year * 10000u + month * 100u + day;
    uint32_t offseason_starts_yyyymmdd = kbo_get_latest_offseason_starts_event(today);
    if (offseason_starts_yyyymmdd == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=no_offseason_starts_event today=%u",
            source != NULL ? source : "",
            today);
        return 0;
    }
    if (offseason_starts_yyyymmdd > today) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=offseason_starts_in_future season_end=%u today=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            today);
        return 0;
    }
    if (g_kbo_foreign_priority_last_scheduled_date == offseason_starts_yyyymmdd) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=already_scheduled_for_season_end today=%u season_end=%u",
            source != NULL ? source : "",
            today,
            offseason_starts_yyyymmdd);
        return 0;
    }

    uint32_t anchor_date = offseason_starts_yyyymmdd;
    uint32_t close_date = kbo_add_days_yyyymmdd(anchor_date, 20u);
    uint32_t military_selection_date = kbo_add_one_month_yyyymmdd(anchor_date);
    if (close_date == 0u) {
        append_logf(
            "KBO custom event schedule skipped source=%s reason=close_date_invalid season_end=%u anchor=%u",
            source != NULL ? source : "",
            offseason_starts_yyyymmdd,
            anchor_date);
        return 0;
    }

    int created_close = 0;
    if (!kbo_custom_event_exists_for_date(
            league_id,
            close_date,
            0)) {
        created_close = create_kbo_league_event(
            close_date / 10000u,
            (close_date / 100u) % 100u,
            close_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_foreign_priority_close_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    int created_military = 0;
    if (military_selection_date != 0u
            && g_kbo_military_selection_last_scheduled_date != offseason_starts_yyyymmdd) {
        created_military = create_kbo_league_event(
            military_selection_date / 10000u,
            (military_selection_date / 100u) % 100u,
            military_selection_date % 100u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_military_selection_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
        if (created_military) {
            g_kbo_military_selection_last_scheduled_date = offseason_starts_yyyymmdd;
        }
    }

    append_logf(
        "KBO custom event schedule source=%s season_end=%u anchor=%u close=%u military=%u created_close=%d created_military=%d",
        source != NULL ? source : "",
        offseason_starts_yyyymmdd,
        anchor_date,
        close_date,
        military_selection_date,
        created_close,
        created_military);

    g_kbo_foreign_priority_last_scheduled_date = offseason_starts_yyyymmdd;
    return created_close || created_military;
}

static int scan_kbo_custom_events_once(const char* source)
{
    uintptr_t event_manager = get_kbo_league_event_manager();
    if (event_manager == 0 || !memory_range_readable((void*)event_manager, OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET + sizeof(int32_t))) {
        return 0;
    }

    uint32_t current_year = 0;
    uint32_t current_month = 0;
    uint32_t current_day = 0;
    if (!kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
        return 0;
    }

    uintptr_t event_vector = *(uintptr_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_VECTOR_OFFSET);
    int32_t event_count = *(int32_t*)(event_manager + OOTP27_EVENT_MANAGER_EVENT_COUNT_OFFSET);
    if (event_vector == 0 || event_count <= 0 || event_count > 20000
            || !memory_range_readable((void*)event_vector, (SIZE_T)event_count * sizeof(uintptr_t))) {
        return 0;
    }

    int triggered = 0;
    for (int32_t i = 0; i < event_count; i++) {
        uintptr_t event_ptr = *(uintptr_t*)(event_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (event_ptr == 0 || !memory_range_readable((void*)event_ptr, 0x48)) {
            continue;
        }

        uint8_t* event = (uint8_t*)event_ptr;
        if (event[OOTP27_LEAGUE_EVENT_DELETED_OFFSET] != 0) {
            continue;
        }
        uint32_t configured_league_id = kbo_get_foreign_waiver_league_id();
        if (configured_league_id == 0u) {
            configured_league_id = kbo_resolve_kbo_league_id();
        }
        if (*(uint32_t*)(event + OOTP27_LEAGUE_EVENT_LEAGUE_ID_OFFSET) != configured_league_id) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }

        char name[128] = {0};
        if (!copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))) {
            continue;
        }
        if (!kbo_custom_event_name_matches_local(name)) {
            continue;
        }

        uint32_t event_year = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET);
        uint32_t event_month = event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET];
        uint32_t event_day = event[OOTP27_LEAGUE_EVENT_DAY_OFFSET];
        uint32_t event_over = *(uint16_t*)(event + OOTP27_LEAGUE_EVENT_EVENT_OVER_OFFSET);
        int due = 0;
        if (event_year < current_year) {
            due = 1;
        } else if (event_year == current_year) {
            if (event_month < current_month) {
                due = 1;
            } else if (event_month == current_month && event_day <= current_day) {
                due = 1;
            }
        }

        if (!due || event_over != 0 || kbo_custom_event_already_processed(event_ptr)) {
            continue;
        }

        uint32_t event_yyyymmdd = event_year * 10000u + event_month * 100u + event_day;
        int action_result = 0;
        int is_open_event = kbo_custom_event_name_is_open(name);
        int is_close_event = kbo_custom_event_name_is_close(name);

        if (is_open_event) {
            if (g_kbo_foreign_priority_last_open_event_fired_date == event_yyyymmdd) {
                append_logf(
                    "KBO custom event skipped duplicate by date source=%s name=%s event_date=%04u-%02u-%02u",
                    source != NULL ? source : "",
                    name,
                    event_year,
                    event_month,
                    event_day);
                continue;
            }
            uint32_t serial = kbo_date_serial(event_year, event_month, event_day);
            action_result = kbo_open_foreign_waiver_window(
                event_yyyymmdd,
                serial,
                "custom_event_monitor");
            if (action_result) {
                g_kbo_foreign_priority_last_open_event_fired_date = event_yyyymmdd;
            }
        } else if (is_close_event) {
            if (g_kbo_foreign_priority_last_close_event_fired_date == event_yyyymmdd) {
                append_logf(
                    "KBO custom event skipped duplicate by date source=%s name=%s event_date=%04u-%02u-%02u",
                    source != NULL ? source : "",
                    name,
                    event_year,
                    event_month,
                    event_day);
                continue;
            }
            append_logf(
                "KBO custom event close marker reached source=%s date=%04u-%02u-%02u",
                source != NULL ? source : "",
                event_year,
                event_month,
                event_day);
            action_result = kbo_announce_foreign_waiver_results(event_yyyymmdd, source);
            if (!action_result) {
                action_result = 1;
            }
            if (action_result) {
                g_kbo_foreign_priority_last_close_event_fired_date = event_yyyymmdd;
            }
        }

        if (action_result) {
            kbo_mark_custom_event_processed(event_ptr);
            triggered++;
        }

        append_logf(
            "KBO custom event triggered source=%s name=%s event_date=%04u-%02u-%02u current=%04u-%02u-%02u over=%u event=%p action_result=%d",
            source != NULL ? source : "",
            name,
            event_year,
            event_month,
            event_day,
            current_year,
            current_month,
            current_day,
            event_over,
            (void*)event_ptr,
            action_result);
    }

    if (triggered > 0) {
        append_logf(
            "KBO custom event scan source=%s current=%04u-%02u-%02u triggered=%d count=%d manager=%p vector=%p",
            source != NULL ? source : "",
            current_year,
            current_month,
            current_day,
            triggered,
            event_count,
            (void*)event_manager,
            (void*)event_vector);
    }
    return triggered;
}

static void kbo_scan_nation_id_offset_once(void)
{
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, NULL)) {
        append_log_line("nation_id_scan: no player vector");
        return;
    }

    uint32_t kbo_league_id = kbo_resolve_kbo_league_id();
    if (kbo_league_id == 0) {
        append_log_line("nation_id_scan: kbo league id unknown");
        return;
    }

    /* Scan 0x00-0x400 for uint32 == 177 across KBO players */
    const uint32_t SCAN_END  = 0x400u;
    const uint32_t SCAN_STEP = 4u;
    const uint32_t SLOTS     = SCAN_END / SCAN_STEP;   /* 256 slots */
    uint32_t hits32[256] = {0};
    uint32_t hits8[256]  = {0};   /* also check uint8 at same positions */

    int kbo_players_checked = 0;
    for (int32_t i = 0; i < player_count && kbo_players_checked < 300; i++) {
        uintptr_t ptr = *(uintptr_t*)(player_vector + (uintptr_t)i * sizeof(uintptr_t));
        if (!kbo_player_pointer_plausible(ptr)) {
            continue;
        }
        uint8_t* p = (uint8_t*)ptr;
        if (!memory_range_readable(p, SCAN_END + 4)) {
            continue;
        }
        uint32_t league_id = *(uint32_t*)(p + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (league_id != kbo_league_id) {
            continue;
        }
        kbo_players_checked++;
        for (uint32_t off = 0; off < SCAN_END; off += SCAN_STEP) {
            uint32_t val32 = *(uint32_t*)(p + off);
            if (val32 == 177u) {
                hits32[off / SCAN_STEP]++;
            }
            if (p[off] == 177u) {
                hits8[off / SCAN_STEP]++;
            }
        }
    }

    append_logf("nation_id_scan: checked %d KBO players (range 0x000-0x%03x)",
        kbo_players_checked, SCAN_END);
    for (uint32_t s = 0; s < SLOTS; s++) {
        uint32_t off = s * SCAN_STEP;
        if (hits32[s] > 0) {
            append_logf("nation_id_scan: uint32 offset=0x%03x hits=%u/%d",
                off, hits32[s], kbo_players_checked);
        }
        if (hits8[s] > 0) {
            append_logf("nation_id_scan: uint8  offset=0x%03x hits=%u/%d",
                off, hits8[s], kbo_players_checked);
        }
    }
    if (kbo_players_checked > 0) {
        /* dump first KBO player's raw bytes at 0x00-0x100 for manual inspection */
        for (int32_t i = 0; i < player_count; i++) {
            uintptr_t ptr = *(uintptr_t*)(player_vector + (uintptr_t)i * sizeof(uintptr_t));
            if (!kbo_player_pointer_plausible(ptr)) continue;
            uint8_t* p = (uint8_t*)ptr;
            if (!memory_range_readable(p, 0x110)) continue;
            if (*(uint32_t*)(p + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) != kbo_league_id) continue;
            uint32_t pid = *(uint32_t*)(p + OOTP27_PLAYER_ID_OFFSET);
            append_logf("nation_id_scan: first KBO player ptr=%p pid=%u", (void*)ptr, pid);
            append_logf("nation_id_scan: +000 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7],p[8],p[9],p[10],p[11],p[12],p[13],p[14],p[15]);
            append_logf("nation_id_scan: +010 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[16],p[17],p[18],p[19],p[20],p[21],p[22],p[23],p[24],p[25],p[26],p[27],p[28],p[29],p[30],p[31]);
            append_logf("nation_id_scan: +020 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[32],p[33],p[34],p[35],p[36],p[37],p[38],p[39],p[40],p[41],p[42],p[43],p[44],p[45],p[46],p[47]);
            append_logf("nation_id_scan: +030 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[48],p[49],p[50],p[51],p[52],p[53],p[54],p[55],p[56],p[57],p[58],p[59],p[60],p[61],p[62],p[63]);
            append_logf("nation_id_scan: +040 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[64],p[65],p[66],p[67],p[68],p[69],p[70],p[71],p[72],p[73],p[74],p[75],p[76],p[77],p[78],p[79]);
            append_logf("nation_id_scan: +050 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[80],p[81],p[82],p[83],p[84],p[85],p[86],p[87],p[88],p[89],p[90],p[91],p[92],p[93],p[94],p[95]);
            append_logf("nation_id_scan: +060 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[96],p[97],p[98],p[99],p[100],p[101],p[102],p[103],p[104],p[105],p[106],p[107],p[108],p[109],p[110],p[111]);
            append_logf("nation_id_scan: +070 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[112],p[113],p[114],p[115],p[116],p[117],p[118],p[119],p[120],p[121],p[122],p[123],p[124],p[125],p[126],p[127]);
            append_logf("nation_id_scan: +080 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[128],p[129],p[130],p[131],p[132],p[133],p[134],p[135],p[136],p[137],p[138],p[139],p[140],p[141],p[142],p[143]);
            append_logf("nation_id_scan: +090 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[144],p[145],p[146],p[147],p[148],p[149],p[150],p[151],p[152],p[153],p[154],p[155],p[156],p[157],p[158],p[159]);
            append_logf("nation_id_scan: +0a0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[160],p[161],p[162],p[163],p[164],p[165],p[166],p[167],p[168],p[169],p[170],p[171],p[172],p[173],p[174],p[175]);
            append_logf("nation_id_scan: +0b0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[176],p[177],p[178],p[179],p[180],p[181],p[182],p[183],p[184],p[185],p[186],p[187],p[188],p[189],p[190],p[191]);
            append_logf("nation_id_scan: +0c0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[192],p[193],p[194],p[195],p[196],p[197],p[198],p[199],p[200],p[201],p[202],p[203],p[204],p[205],p[206],p[207]);
            append_logf("nation_id_scan: +0d0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[208],p[209],p[210],p[211],p[212],p[213],p[214],p[215],p[216],p[217],p[218],p[219],p[220],p[221],p[222],p[223]);
            append_logf("nation_id_scan: +0e0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[224],p[225],p[226],p[227],p[228],p[229],p[230],p[231],p[232],p[233],p[234],p[235],p[236],p[237],p[238],p[239]);
            append_logf("nation_id_scan: +0f0 %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
                p[240],p[241],p[242],p[243],p[244],p[245],p[246],p[247],p[248],p[249],p[250],p[251],p[252],p[253],p[254],p[255]);
            break;
        }
    }
}

static DWORD WINAPI kbo_custom_event_monitor_thread(LPVOID parameter)
{
    (void)parameter;
    append_log_line("KBO custom event monitor started");

    for (;;) {
        Sleep(2000);
        kbo_schedule_foreign_priority_custom_events(g_kbo_default_event_source);
        scan_kbo_custom_events_once("custom_event_monitor");
    }

    return 0;
}

static int start_kbo_custom_event_monitor(void)
{
    if (!kbo_fix_enabled()) {
        append_log_line("KBO custom event monitor skipped reason=fix_disabled");
        return 0;
    }
    if (InterlockedCompareExchange(&g_kbo_custom_event_monitor_started, 1, 0) != 0) {
        return 1;
    }

    HANDLE thread = CreateThread(NULL, 0, kbo_custom_event_monitor_thread, NULL, 0, NULL);
    if (thread == NULL) {
        InterlockedExchange(&g_kbo_custom_event_monitor_started, 0);
        append_logf("KBO custom event monitor skipped reason=create_thread_failed error=%lu", GetLastError());
        return 0;
    }
    CloseHandle(thread);
    return 1;
}
