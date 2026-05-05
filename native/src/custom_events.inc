#include <stdint.h>

static LONG g_kbo_custom_event_monitor_started = 0;
static uint32_t g_kbo_foreign_priority_last_scheduled_date = 0;
static uint32_t g_kbo_military_selection_last_scheduled_date = 0;
static uint32_t g_kbo_asian_games_last_scheduled_year = 0;
static uint32_t g_kbo_foreign_priority_last_open_event_fired_date = 0;
static uint32_t g_kbo_foreign_priority_last_close_event_fired_date = 0;
static uint32_t g_kbo_asian_games_last_selection_fired_date = 0;
static uint32_t g_kbo_asian_games_last_departure_fired_date = 0;
static uint32_t g_kbo_asian_games_last_final_fired_date = 0;
static uintptr_t g_kbo_processed_event_ptrs[256] = {0};
static LONG g_kbo_processed_event_count = 0;

static const char g_kbo_foreign_priority_open_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Begins";
static const char g_kbo_foreign_priority_close_event_title[] =
    "[KBO] Foreign Player Priority Negotiation Ends";
static const char g_kbo_military_selection_event_title[] =
    "[KBO] Military Service Selection";
static const char g_kbo_asian_games_selection_event_title[] =
    "[KBO] Asian Games Roster Selection";
static const char g_kbo_asian_games_departure_event_title[] =
    "[KBO] Asian Games Player Departure";
static const char g_kbo_asian_games_final_event_title[] =
    "[KBO] Asian Games Final";

static int kbo_announce_foreign_waiver_results(uint32_t event_yyyymmdd, const char* source);
static void kbo_copy_asian_games_team_link(uint32_t team_id, char* out, size_t out_size);
static void kbo_copy_asian_games_player_link(KboAsianGamesRosterEntry* entry, char* out, size_t out_size);

static int kbo_custom_event_name_is_open(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_open_event_title);
}

static int kbo_custom_event_name_is_close(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_foreign_priority_close_event_title);
}

static int kbo_custom_event_name_is_asian_games_selection(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_selection_event_title);
}

static int kbo_custom_event_name_is_asian_games_departure(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_departure_event_title);
}

static int kbo_custom_event_name_is_asian_games_final(const char* name)
{
    return ascii_equals_ignore_case(name, g_kbo_asian_games_final_event_title);
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
    return kbo_custom_event_name_is_open(name)
        || kbo_custom_event_name_is_close(name)
        || kbo_custom_event_name_is_asian_games_selection(name)
        || kbo_custom_event_name_is_asian_games_departure(name)
        || kbo_custom_event_name_is_asian_games_final(name);
}

static int kbo_year_has_asian_games(uint32_t year)
{
    if (year < 1982u || year > 2200u) {
        return 0;
    }
    return ((year - 1982u) % 4u) == 0u;
}

static int kbo_custom_event_exists_by_title_for_date(
    uint32_t league_id,
    uint32_t event_yyyymmdd,
    const char* expected_title)
{
    if (event_yyyymmdd == 0u || expected_title == NULL || expected_title[0] == '\0') {
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

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
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
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_TYPE_OFFSET) != (uint16_t)OOTP27_EVENT_TYPE_CUSTOM_EVENT) {
            continue;
        }
        if (*(uint16_t*)(event + OOTP27_LEAGUE_EVENT_YEAR_OFFSET) != (uint16_t)year
                || event[OOTP27_LEAGUE_EVENT_MONTH_OFFSET] != (uint8_t)month
                || event[OOTP27_LEAGUE_EVENT_DAY_OFFSET] != (uint8_t)day) {
            continue;
        }

        char name[128] = {0};
        if (copy_ootp_string_object_text(event, OOTP27_LEAGUE_EVENT_NAME_STRING_OFFSET, name, sizeof(name))
                && ascii_equals_ignore_case(name, expected_title)) {
            return 1;
        }
    }

    return 0;
}

static int kbo_schedule_asian_games_custom_events(const char* source)
{
    uint32_t year = 0;
    uint32_t month = 0;
    uint32_t day = 0;
    if (!kbo_current_date_is_valid(&year, &month, &day)) {
        append_logf(
            "KBO Asian Games schedule skipped source=%s reason=current_date_unavailable",
            source != NULL ? source : "");
        return 0;
    }
    if (!kbo_year_has_asian_games(year)) {
        return 0;
    }
    if (g_kbo_asian_games_last_scheduled_year == year) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        append_logf(
            "KBO Asian Games schedule skipped source=%s reason=league_id_unavailable year=%u",
            source != NULL ? source : "",
            year);
        return 0;
    }

    uint32_t selection_date = year * 10000u + 805u;
    uint32_t departure_date = year * 10000u + 919u;
    uint32_t final_date = year * 10000u + 1004u;
    uint32_t today = year * 10000u + month * 100u + day;
    if (today > final_date) {
        g_kbo_asian_games_last_scheduled_year = year;
        append_logf(
            "KBO Asian Games schedule skipped source=%s reason=events_past year=%u today=%u final=%u",
            source != NULL ? source : "",
            year,
            today,
            final_date);
        return 0;
    }

    int created_selection = 0;
    int created_departure = 0;
    int created_final = 0;
    int skipped_past_selection = selection_date < today;
    int skipped_past_departure = departure_date < today;
    int skipped_past_final = final_date < today;
    if (!skipped_past_selection && !kbo_custom_event_exists_by_title_for_date(
            league_id,
            selection_date,
            g_kbo_asian_games_selection_event_title)) {
        created_selection = create_kbo_league_event(
            year,
            8u,
            5u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_asian_games_selection_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!skipped_past_departure && !kbo_custom_event_exists_by_title_for_date(
            league_id,
            departure_date,
            g_kbo_asian_games_departure_event_title)) {
        created_departure = create_kbo_league_event(
            year,
            9u,
            19u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_asian_games_departure_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }
    if (!skipped_past_final && !kbo_custom_event_exists_by_title_for_date(
            league_id,
            final_date,
            g_kbo_asian_games_final_event_title)) {
        created_final = create_kbo_league_event(
            year,
            10u,
            4u,
            league_id,
            OOTP27_EVENT_TYPE_CUSTOM_EVENT,
            g_kbo_asian_games_final_event_title,
            0,
            source != NULL ? source : g_kbo_default_event_source);
    }

    g_kbo_asian_games_last_scheduled_year = year;
    append_logf(
        "KBO Asian Games schedule source=%s year=%u today=%u selection=%u departure=%u final=%u created_selection=%d created_departure=%d created_final=%d skipped_past_selection=%d skipped_past_departure=%d skipped_past_final=%d",
        source != NULL ? source : "",
        year,
        today,
        selection_date,
        departure_date,
        final_date,
        created_selection,
        created_departure,
        created_final,
        skipped_past_selection,
        skipped_past_departure,
        skipped_past_final);
    return created_selection || created_departure || created_final;
}

static int kbo_asian_games_candidate_compare_desc(const void* left, const void* right)
{
    const KboAsianGamesCandidate* a = (const KboAsianGamesCandidate*)left;
    const KboAsianGamesCandidate* b = (const KboAsianGamesCandidate*)right;
    if (a->entry.score != b->entry.score) {
        return a->entry.score > b->entry.score ? -1 : 1;
    }
    if (a->entry.age != b->entry.age) {
        return a->entry.age < b->entry.age ? -1 : 1;
    }
    if (a->entry.player_id == b->entry.player_id) {
        return 0;
    }
    return a->entry.player_id < b->entry.player_id ? -1 : 1;
}

static int32_t kbo_asian_games_player_score(uint8_t* player)
{
    int32_t overall = kbo_read_player_i16(player, OOTP27_PLAYER_OVERALL_VALUE_OFFSET);
    int32_t talent = kbo_read_player_i16(player, OOTP27_PLAYER_TALENT_VALUE_OFFSET);
    int32_t ratings = kbo_read_player_i16(player, OOTP27_PLAYER_RATINGS_VALUE_OFFSET);
    int32_t career = kbo_read_player_i16(player, OOTP27_PLAYER_CAREER_VALUE_OFFSET);
    uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);

    int32_t score = talent * 45 + overall * 30 + ratings * 15 + career * 10;
    if (age <= 24u) {
        score += 1200;
    } else if (age <= 27u) {
        score += 200;
    } else {
        score -= (int32_t)(age - 27u) * 120;
    }
    if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u) {
        score += 900;
    }
    if (*(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) != 0u) {
        score += 150;
    }
    return score;
}

static int kbo_asian_games_role_is_pitcher(uint8_t role)
{
    return role == 1u;
}

static int kbo_asian_games_role_is_catcher(uint8_t role)
{
    return role == 2u;
}

static int kbo_asian_games_role_is_infielder(uint8_t role)
{
    return role >= 3u && role <= 6u;
}

static int kbo_asian_games_role_is_outfielder(uint8_t role)
{
    return role >= 7u && role <= 9u;
}

static const char* kbo_asian_games_role_bucket_label(uint8_t role)
{
    if (kbo_asian_games_role_is_pitcher(role)) {
        return "P";
    }
    if (kbo_asian_games_role_is_catcher(role)) {
        return "C";
    }
    if (kbo_asian_games_role_is_infielder(role)) {
        return "IF";
    }
    if (kbo_asian_games_role_is_outfielder(role)) {
        return "OF";
    }
    return "BAT";
}

static int kbo_asian_games_roles_same_bucket(uint8_t left, uint8_t right)
{
    if (kbo_asian_games_role_is_pitcher(left)) {
        return kbo_asian_games_role_is_pitcher(right);
    }
    if (kbo_asian_games_role_is_catcher(left)) {
        return kbo_asian_games_role_is_catcher(right);
    }
    if (kbo_asian_games_role_is_infielder(left)) {
        return kbo_asian_games_role_is_infielder(right);
    }
    if (kbo_asian_games_role_is_outfielder(left)) {
        return kbo_asian_games_role_is_outfielder(right);
    }
    return 0;
}

static int kbo_asian_games_league_allowed(uint32_t league_id, const uint32_t* allowed_leagues, int allowed_count)
{
    if (league_id == 0u || allowed_leagues == NULL || allowed_count <= 0) {
        return 0;
    }
    for (int i = 0; i < allowed_count; i++) {
        if (allowed_leagues[i] == league_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_ascii_contains_ignore_case(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }

    size_t needle_len = strlen(needle);
    for (const char* p = text; *p != '\0'; p++) {
        size_t i = 0;
        while (i < needle_len && p[i] != '\0') {
            char a = p[i];
            char b = needle[i];
            if (a >= 'A' && a <= 'Z') { a = (char)(a + 32); }
            if (b >= 'A' && b <= 'Z') { b = (char)(b + 32); }
            if (a != b) {
                break;
            }
            i++;
        }
        if (i == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int kbo_asian_games_team_is_service_or_ulsan(uint8_t* team)
{
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return 0;
    }

    static const uint32_t string_offsets[] = { 0x10u, 0x28u, 0x40u, 0x58u, 0x70u, 0x100u };
    for (size_t i = 0; i < sizeof(string_offsets) / sizeof(string_offsets[0]); i++) {
        char name[96] = {0};
        if (!copy_ootp_string_object_text(team, string_offsets[i], name, sizeof(name))) {
            continue;
        }
        if (kbo_ascii_contains_ignore_case(name, "Sangmu")
                || kbo_ascii_contains_ignore_case(name, "Police")
                || kbo_ascii_contains_ignore_case(name, "Ulsan")) {
            return 1;
        }
    }
    return 0;
}

static uint32_t kbo_asian_games_org_team_id_for_team(uint32_t team_id)
{
    if (team_id == 0u) {
        return 0u;
    }

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        uint32_t parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
        if (parent_team_id != 0u) {
            return parent_team_id;
        }
    }
    return team_id;
}

static int kbo_asian_games_find_org_index(const uint32_t* org_ids, int org_count, uint32_t org_id)
{
    if (org_ids == NULL || org_id == 0u || org_count <= 0) {
        return -1;
    }
    for (int i = 0; i < org_count; i++) {
        if (org_ids[i] == org_id) {
            return i;
        }
    }
    return -1;
}

static int kbo_asian_games_roster_org_count(uint32_t org_id, int selected_count)
{
    if (org_id == 0u || selected_count <= 0) {
        return 0;
    }

    int count = 0;
    if (selected_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        selected_count = KBO_ASIAN_GAMES_ROSTER_SIZE;
    }
    for (int i = 0; i < selected_count; i++) {
        uint32_t roster_org_id = kbo_asian_games_org_team_id_for_team(g_kbo_asian_games_roster[i].original_team_id);
        if (roster_org_id == org_id) {
            count++;
        }
    }
    return count;
}

static int kbo_asian_games_collect_required_orgs(uint32_t main_league_id, uint32_t* org_ids, int org_capacity)
{
    uintptr_t global = get_ootp_global_database();
    if (main_league_id == 0u || org_ids == NULL || org_capacity <= 0
            || global == 0 || !memory_range_readable((void*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET), 0x10)) {
        return 0;
    }

    uintptr_t vector = *(uintptr_t*)(global + OOTP27_KBO_TEAM_VECTOR_OFFSET);
    int32_t team_count = *(int32_t*)(global + OOTP27_KBO_TEAM_COUNT_OFFSET);
    if (vector == 0 || team_count <= 0 || team_count > 10000
            || !memory_range_readable((void*)vector, (SIZE_T)team_count * sizeof(uintptr_t))) {
        return 0;
    }

    int org_count = 0;
    for (int32_t i = 0; i < team_count && org_count < org_capacity; i++) {
        uintptr_t team_ptr = *(uintptr_t*)(vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (team_ptr == 0 || !memory_range_readable((void*)team_ptr, OOTP27_KBO_TEAM_READABLE_BYTES)) {
            continue;
        }
        uint8_t* team = (uint8_t*)team_ptr;
        if (*(uint32_t*)(team + OOTP27_KBO_TEAM_LEAGUE_ID_OFFSET) != main_league_id
                || *(uint32_t*)(team + OOTP27_KBO_TEAM_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID
                || team[OOTP27_KBO_TEAM_DELETED_OFFSET] != 0u) {
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(team)) {
            continue;
        }
        uint32_t team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_ID_OFFSET);
        uint32_t org_id = kbo_asian_games_org_team_id_for_team(team_id);
        if (org_id != 0u && kbo_asian_games_find_org_index(org_ids, org_count, org_id) < 0) {
            org_ids[org_count++] = org_id;
        }
    }
    return org_count;
}

static uint32_t kbo_asian_games_final_date_for_year(uint32_t year)
{
    return year * 10000u + 1004u;
}

static int32_t kbo_asian_games_days_until_return(uint32_t event_yyyymmdd)
{
    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;
    uint32_t final_date = kbo_asian_games_final_date_for_year(year);
    uint32_t start_serial = kbo_date_serial(year, month, day);
    uint32_t final_serial = kbo_date_serial(final_date / 10000u, (final_date / 100u) % 100u, final_date % 100u);
    if (start_serial == 0u || final_serial == 0u || final_serial < start_serial) {
        return 16;
    }
    return (int32_t)(final_serial - start_serial + 1u);
}

static int kbo_get_asian_games_roster_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    return kbo_get_save_scoped_data_file("asian_games_roster.csv", out, out_size);
}

static int kbo_get_legacy_asian_games_roster_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    char local_app_data[MAX_PATH] = {0};
    DWORD got = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
    if (got == 0 || got >= sizeof(local_app_data)) {
        return 0;
    }

    char dir[MAX_PATH] = {0};
    snprintf(dir, sizeof(dir), "%s\\OOTP-KBO", local_app_data);
    CreateDirectoryA(dir, NULL);
    snprintf(out, out_size, "%s\\asian_games_roster.csv", dir);
    return out[0] != '\0';
}

static int kbo_save_asian_games_roster_csv(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        append_logf("KBO Asian Games roster csv save skipped source=%s reason=path_unavailable", source != NULL ? source : "");
        return 0;
    }

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        append_logf("KBO Asian Games roster csv save skipped source=%s reason=create_failed gle=%lu path=%s", source != NULL ? source : "", GetLastError(), path);
        return 0;
    }

    const char* header = "year,index,player_id,original_team_id,original_league_id,departure_date,return_date,age,role,wildcard,old_restricted,old_secondary_restricted,old_injury_active,old_injury_days_left,departed,returned,exempted,score\r\n";
    DWORD written = 0;
    WriteFile(file, header, (DWORD)strlen(header), &written, NULL);

    int ok = 1;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        char line[512] = {0};
        int len = snprintf(
            line,
            sizeof(line),
            "%u,%ld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%d\r\n",
            g_kbo_asian_games_roster_year,
            i + 1,
            entry->player_id,
            entry->original_team_id,
            entry->original_league_id,
            entry->departure_date,
            entry->return_date,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            (uint32_t)entry->wildcard,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left,
            (uint32_t)entry->departed,
            (uint32_t)entry->returned,
            (uint32_t)entry->exempted,
            entry->score);
        if (len <= 0 || !WriteFile(file, line, (DWORD)len, &written, NULL) || written != (DWORD)len) {
            ok = 0;
            break;
        }
    }

    CloseHandle(file);
    append_logf("KBO Asian Games roster csv save source=%s ok=%d year=%u count=%ld path=%s", source != NULL ? source : "", ok, g_kbo_asian_games_roster_year, roster_count, path);
    return ok;
}

static int kbo_load_asian_games_roster_csv(const char* source)
{
    char path[MAX_PATH] = {0};
    if (!kbo_get_asian_games_roster_csv_path(path, sizeof(path))) {
        return 0;
    }

    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD scoped_error = GetLastError();
        char legacy_path[MAX_PATH] = {0};
        if (!kbo_get_legacy_asian_games_roster_csv_path(legacy_path, sizeof(legacy_path))) {
            append_logf("KBO Asian Games roster csv load skipped source=%s reason=open_failed gle=%lu path=%s", source != NULL ? source : "", scoped_error, path);
            return 0;
        }
        file = CreateFileA(legacy_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            append_logf("KBO Asian Games roster csv load skipped source=%s reason=open_failed gle=%lu path=%s legacy=%s", source != NULL ? source : "", scoped_error, path, legacy_path);
            return 0;
        }
        snprintf(path, sizeof(path), "%s", legacy_path);
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 65536u) {
        CloseHandle(file);
        return 0;
    }

    char* raw = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (raw == NULL) {
        CloseHandle(file);
        return 0;
    }

    DWORD read = 0;
    int ok = ReadFile(file, raw, size, &read, NULL) && read > 0u;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, raw);
        return 0;
    }
    raw[read] = '\0';

    KboAsianGamesRosterEntry loaded[KBO_ASIAN_GAMES_ROSTER_SIZE];
    memset(loaded, 0, sizeof(loaded));
    int loaded_count = 0;
    uint32_t loaded_year = 0;

    char* line = raw;
    while (line != NULL && *line != '\0') {
        char* next = strchr(line, '\n');
        if (next != NULL) { *next++ = '\0'; }
        char* cr = strchr(line, '\r');
        if (cr != NULL) { *cr = '\0'; }

        if (strncmp(line, "year,", 5) != 0 && line[0] != '\0') {
            unsigned int year = 0, index = 0, player_id = 0, team_id = 0, league_id = 0;
            unsigned int departure_date = 0, return_date = 0, age = 0, role = 0, wildcard = 0;
            unsigned int old_restricted = 0, old_secondary = 0, old_injury = 0;
            int old_days = 0, score = 0;
            unsigned int departed = 0, returned = 0, exempted = 0;
            int fields = sscanf(
                line,
                "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%u,%u,%u,%d",
                &year, &index, &player_id, &team_id, &league_id, &departure_date, &return_date,
                &age, &role, &wildcard, &old_restricted, &old_secondary, &old_injury, &old_days,
                &departed, &returned, &exempted, &score);
            if (fields == 18 && player_id != 0u && loaded_count < KBO_ASIAN_GAMES_ROSTER_SIZE) {
                KboAsianGamesRosterEntry* entry = &loaded[loaded_count++];
                loaded_year = year;
                entry->player_id = player_id;
                entry->original_team_id = team_id;
                entry->original_league_id = league_id;
                entry->departure_date = departure_date;
                entry->return_date = return_date;
                entry->age = (uint16_t)age;
                entry->role = (uint8_t)role;
                entry->wildcard = (uint8_t)wildcard;
                entry->old_restricted = (uint8_t)old_restricted;
                entry->old_secondary_restricted = (uint8_t)old_secondary;
                entry->old_injury_active = (uint8_t)old_injury;
                entry->old_injury_days_left = (int16_t)old_days;
                entry->departed = (uint8_t)departed;
                entry->returned = (uint8_t)returned;
                entry->exempted = (uint8_t)exempted;
                entry->score = score;
                entry->player_ptr = 0;
            }
        }
        line = next;
    }

    HeapFree(GetProcessHeap(), 0, raw);
    if (loaded_count <= 0) {
        return 0;
    }

    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    memcpy(g_kbo_asian_games_roster, loaded, sizeof(KboAsianGamesRosterEntry) * (size_t)loaded_count);
    g_kbo_asian_games_roster_count = loaded_count;
    g_kbo_asian_games_roster_year = loaded_year;
    append_logf("KBO Asian Games roster csv load source=%s year=%u count=%d path=%s", source != NULL ? source : "", loaded_year, loaded_count, path);
    return loaded_count;
}

static int kbo_asian_games_try_select_candidate(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int index,
    int* selected_count,
    int* pitcher_count,
    int* catcher_count,
    int* infielder_count,
    int* outfielder_count,
    int* wildcard_count,
    int enforce_team_max)
{
    if (candidates == NULL || index < 0 || index >= candidate_count
            || selected_count == NULL || pitcher_count == NULL
            || catcher_count == NULL || infielder_count == NULL
            || outfielder_count == NULL || wildcard_count == NULL) {
        return 0;
    }
    if (*selected_count >= KBO_ASIAN_GAMES_ROSTER_SIZE || candidates[index].selected) {
        return 0;
    }

    KboAsianGamesRosterEntry* entry = &candidates[index].entry;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        if (*pitcher_count >= KBO_ASIAN_GAMES_PITCHER_TARGET) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        if (*catcher_count >= KBO_ASIAN_GAMES_CATCHER_TARGET) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        if (*infielder_count >= KBO_ASIAN_GAMES_INFIELDER_TARGET) {
            return 0;
        }
    } else if (kbo_asian_games_role_is_outfielder(entry->role)) {
        if (*outfielder_count >= KBO_ASIAN_GAMES_OUTFIELDER_TARGET) {
            return 0;
        }
    } else {
        return 0;
    }

    int wildcard = entry->age > 24u;
    if (wildcard && *wildcard_count >= KBO_ASIAN_GAMES_MAX_WILDCARDS) {
        return 0;
    }
    if (enforce_team_max && candidates[index].org_team_id != 0u
            && kbo_asian_games_roster_org_count(candidates[index].org_team_id, *selected_count) >= KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS) {
        return 0;
    }

    entry->wildcard = wildcard ? 1u : 0u;
    g_kbo_asian_games_roster[*selected_count] = *entry;
    candidates[index].selected = 1u;
    (*selected_count)++;
    if (kbo_asian_games_role_is_pitcher(entry->role)) {
        (*pitcher_count)++;
    } else if (kbo_asian_games_role_is_catcher(entry->role)) {
        (*catcher_count)++;
    } else if (kbo_asian_games_role_is_infielder(entry->role)) {
        (*infielder_count)++;
    } else {
        (*outfielder_count)++;
    }
    if (wildcard) {
        (*wildcard_count)++;
    }
    return 1;
}

static int kbo_asian_games_apply_wildcard_replacements(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    int selected_count,
    int* wildcard_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    if (candidates == NULL || candidate_count <= 0 || selected_count <= 0 || wildcard_count == NULL) {
        return 0;
    }

    int replacements = 0;
    while (*wildcard_count < KBO_ASIAN_GAMES_MAX_WILDCARDS) {
        int best_candidate_index = -1;
        int best_roster_index = -1;
        int best_gain = -2147483647;
        int best_was_military_ready = 0;

        for (int i = 0; i < candidate_count; i++) {
            KboAsianGamesCandidate* candidate = &candidates[i];
            if (candidate->selected || candidate->entry.age <= 24u) {
                continue;
            }
            int candidate_military_ready = 1;
            uintptr_t player_ptr = candidate->entry.player_ptr;
            if (kbo_player_pointer_plausible(player_ptr)) {
                uint8_t* player = (uint8_t*)player_ptr;
                candidate_military_ready = player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u
                    && player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] == 0u;
            }

            for (int roster_index = 0; roster_index < selected_count; roster_index++) {
                KboAsianGamesRosterEntry* current = &g_kbo_asian_games_roster[roster_index];
                if (current->wildcard != 0u || current->age > 24u) {
                    continue;
                }
                if (!kbo_asian_games_roles_same_bucket(candidate->entry.role, current->role)) {
                    continue;
                }

                uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(current->original_team_id);
                if (candidate->org_team_id != 0u
                        && old_org_id != candidate->org_team_id
                        && kbo_asian_games_roster_org_count(candidate->org_team_id, selected_count) >= KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS) {
                    continue;
                }
                int old_org_required = kbo_asian_games_find_org_index(required_orgs, required_org_count, old_org_id) >= 0;
                if (old_org_required
                        && old_org_id != candidate->org_team_id
                        && kbo_asian_games_roster_org_count(old_org_id, selected_count) <= KBO_ASIAN_GAMES_TEAM_MIN_PLAYERS) {
                    continue;
                }

                int gain = candidate->entry.score - current->score;
                if (best_candidate_index < 0
                        || (candidate_military_ready && !best_was_military_ready)
                        || (candidate_military_ready == best_was_military_ready && gain > best_gain)) {
                    best_candidate_index = i;
                    best_roster_index = roster_index;
                    best_gain = gain;
                    best_was_military_ready = candidate_military_ready;
                }
            }
        }

        if (best_candidate_index < 0 || best_roster_index < 0) {
            break;
        }

        KboAsianGamesRosterEntry old_entry = g_kbo_asian_games_roster[best_roster_index];
        KboAsianGamesCandidate* replacement = &candidates[best_candidate_index];
        replacement->entry.wildcard = 1u;
        g_kbo_asian_games_roster[best_roster_index] = replacement->entry;
        replacement->selected = 1u;
        (*wildcard_count)++;
        replacements++;

        append_logf(
            "KBO Asian Games wildcard replacement slot=%d old_player=%u old_age=%u old_role=%u old_score=%d new_player=%u new_age=%u new_role=%u new_score=%d gain=%d military_ready=%d",
            best_roster_index + 1,
            old_entry.player_id,
            (uint32_t)old_entry.age,
            (uint32_t)old_entry.role,
            old_entry.score,
            replacement->entry.player_id,
            (uint32_t)replacement->entry.age,
            (uint32_t)replacement->entry.role,
            replacement->entry.score,
            best_gain,
            best_was_military_ready);
    }

    return replacements;
}

static int kbo_asian_games_replace_for_missing_org(
    KboAsianGamesCandidate* candidates,
    int candidate_count,
    uint32_t missing_org_id,
    int selected_count,
    const uint32_t* required_orgs,
    int required_org_count)
{
    if (candidates == NULL || candidate_count <= 0 || missing_org_id == 0u || selected_count <= 0) {
        return 0;
    }

    int best_candidate_index = -1;
    int best_roster_index = -1;
    int best_loss = 2147483647;

    for (int i = 0; i < candidate_count; i++) {
        KboAsianGamesCandidate* candidate = &candidates[i];
        if (candidate->selected || candidate->org_team_id != missing_org_id || candidate->entry.age > 24u) {
            continue;
        }

        for (int roster_index = 0; roster_index < selected_count; roster_index++) {
            KboAsianGamesRosterEntry* current = &g_kbo_asian_games_roster[roster_index];
            if (current->wildcard != 0u || current->age > 24u) {
                continue;
            }
            if (!kbo_asian_games_roles_same_bucket(candidate->entry.role, current->role)) {
                continue;
            }

            uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(current->original_team_id);
            if (old_org_id == missing_org_id) {
                continue;
            }
            int old_org_required = kbo_asian_games_find_org_index(required_orgs, required_org_count, old_org_id) >= 0;
            if (old_org_required && kbo_asian_games_roster_org_count(old_org_id, selected_count) <= KBO_ASIAN_GAMES_TEAM_MIN_PLAYERS) {
                continue;
            }

            int loss = current->score - candidate->entry.score;
            if (best_candidate_index < 0 || loss < best_loss) {
                best_candidate_index = i;
                best_roster_index = roster_index;
                best_loss = loss;
            }
        }
    }

    if (best_candidate_index < 0 || best_roster_index < 0) {
        return 0;
    }

    KboAsianGamesRosterEntry old_entry = g_kbo_asian_games_roster[best_roster_index];
    g_kbo_asian_games_roster[best_roster_index] = candidates[best_candidate_index].entry;
    candidates[best_candidate_index].selected = 1u;

    append_logf(
        "KBO Asian Games team-min replacement org=%u slot=%d old_player=%u old_team=%u new_player=%u new_team=%u loss=%d",
        missing_org_id,
        best_roster_index + 1,
        old_entry.player_id,
        old_entry.original_team_id,
        candidates[best_candidate_index].entry.player_id,
        candidates[best_candidate_index].entry.original_team_id,
        best_loss);
    return 1;
}

static int kbo_select_asian_games_roster(uint32_t event_yyyymmdd, const char* source)
{
    uint32_t year = event_yyyymmdd / 10000u;
    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        append_logf(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=player_vector_unavailable",
            source != NULL ? source : "",
            year);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t allowed_leagues[KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES] = {0};
    int allowed_league_count = 0;
    if (kbo_league_id != 0u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = kbo_league_id;
    }
    if (kbo_league_id == 200u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = 201u;
    }

    KboAsianGamesCandidate* candidates = (KboAsianGamesCandidate*)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(KboAsianGamesCandidate) * KBO_ASIAN_GAMES_MAX_CANDIDATES);
    if (candidates == NULL) {
        append_logf(
            "KBO Asian Games roster selection skipped source=%s year=%u reason=alloc_failed",
            source != NULL ? source : "",
            year);
        return 0;
    }

    int candidate_count = 0;
    int scanned = 0;
    int rejected_nation = 0;
    int rejected_status = 0;
    int rejected_team = 0;
    int rejected_league = 0;
    int rejected_service_team = 0;
    for (int32_t i = 0; i < player_count && candidate_count < KBO_ASIAN_GAMES_MAX_CANDIDATES; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_nation++;
            continue;
        }
        if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] != 0u
                || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
                || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
                || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u) {
            rejected_status++;
            continue;
        }
        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (!kbo_asian_games_league_allowed(current_league_id, allowed_leagues, allowed_league_count)) {
            rejected_league++;
            continue;
        }
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        uint32_t team_nation_id = current_team != NULL
            ? *(uint32_t*)(current_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET)
            : 0u;
        if (current_team_id == 0u || current_league_id == 0u
                || current_team == NULL
                || team_nation_id != OOTP27_KBO_KOREA_NATION_ID) {
            rejected_team++;
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(current_team)) {
            rejected_service_team++;
            continue;
        }

        KboAsianGamesCandidate* candidate = &candidates[candidate_count++];
        candidate->entry.player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        candidate->entry.original_team_id = current_team_id;
        candidate->entry.original_league_id = current_league_id;
        candidate->entry.departure_date = 0u;
        candidate->entry.return_date = 0u;
        candidate->entry.age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        candidate->entry.role = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        candidate->entry.wildcard = 0u;
        candidate->entry.old_restricted = 0u;
        candidate->entry.old_secondary_restricted = 0u;
        candidate->entry.old_injury_active = 0u;
        candidate->entry.old_injury_days_left = 0;
        candidate->entry.departed = 0u;
        candidate->entry.returned = 0u;
        candidate->entry.exempted = 0u;
        candidate->entry.score = kbo_asian_games_player_score(player);
        candidate->entry.player_ptr = player_ptr;
        candidate->org_team_id = kbo_asian_games_org_team_id_for_team(current_team_id);
    }

    qsort(candidates, (size_t)candidate_count, sizeof(KboAsianGamesCandidate), kbo_asian_games_candidate_compare_desc);
    memset(g_kbo_asian_games_roster, 0, sizeof(g_kbo_asian_games_roster));
    g_kbo_asian_games_roster_count = 0;
    g_kbo_asian_games_roster_year = year;

    uint32_t required_orgs[KBO_ASIAN_GAMES_MAX_ORGS] = {0};
    int required_org_count = kbo_asian_games_collect_required_orgs(
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        required_orgs,
        KBO_ASIAN_GAMES_MAX_ORGS);

    int selected_count = 0;
    int pitcher_count = 0;
    int catcher_count = 0;
    int infielder_count = 0;
    int outfielder_count = 0;
    int wildcard_count = 0;

    int required_org_initial_selected = 0;
    for (int org_index = 0; org_index < required_org_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; org_index++) {
        int best_index = -1;
        for (int i = 0; i < candidate_count; i++) {
            if (candidates[i].selected
                    || candidates[i].org_team_id != required_orgs[org_index]
                    || candidates[i].entry.age > 24u) {
                continue;
            }
            best_index = i;
            if (kbo_asian_games_try_select_candidate(
                    candidates,
                    candidate_count,
                    i,
                    &selected_count,
                    &pitcher_count,
                    &catcher_count,
                    &infielder_count,
                    &outfielder_count,
                    &wildcard_count,
                    1)) {
                required_org_initial_selected++;
                break;
            }
        }
        if (best_index >= 0 && kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) == 0) {
            append_logf(
                "KBO Asian Games team-min initial select deferred org=%u best_player=%u best_role=%u reason=position_full_or_team_max",
                required_orgs[org_index],
                candidates[best_index].entry.player_id,
                (uint32_t)candidates[best_index].entry.role);
        }
    }

    for (int i = 0; i < candidate_count && selected_count < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        if (candidates[i].entry.age > 24u) {
            continue;
        }
        kbo_asian_games_try_select_candidate(
            candidates,
            candidate_count,
            i,
            &selected_count,
            &pitcher_count,
            &catcher_count,
            &infielder_count,
            &outfielder_count,
            &wildcard_count,
            1);
    }

    int required_org_repairs = 0;
    for (int org_index = 0; org_index < required_org_count; org_index++) {
        if (kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) > 0) {
            continue;
        }
        if (kbo_asian_games_replace_for_missing_org(
                candidates,
                candidate_count,
                required_orgs[org_index],
                selected_count,
                required_orgs,
                required_org_count)) {
            required_org_repairs++;
        }
    }

    int required_org_missing = 0;
    for (int org_index = 0; org_index < required_org_count; org_index++) {
        if (kbo_asian_games_roster_org_count(required_orgs[org_index], selected_count) <= 0) {
            required_org_missing++;
        }
    }

    int wildcard_replacements = kbo_asian_games_apply_wildcard_replacements(
        candidates,
        candidate_count,
        selected_count,
        &wildcard_count,
        required_orgs,
        required_org_count);

    g_kbo_asian_games_roster_count = selected_count;
    append_logf(
        "KBO Asian Games roster selected source=%s year=%u scanned=%d candidates=%d selected=%d pitchers=%d catchers=%d infielders=%d outfielders=%d wildcards=%d wildcard_replacements=%d required_orgs=%d required_initial=%d required_repairs=%d required_missing=%d team_max=%d allowed_leagues=%u/%u vector_offset=0x%x rejected_nation=%d rejected_status=%d rejected_league=%d rejected_team=%d rejected_service_team=%d",
        source != NULL ? source : "",
        year,
        scanned,
        candidate_count,
        selected_count,
        pitcher_count,
        catcher_count,
        infielder_count,
        outfielder_count,
        wildcard_count,
        wildcard_replacements,
        required_org_count,
        required_org_initial_selected,
        required_org_repairs,
        required_org_missing,
        KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS,
        allowed_league_count > 0 ? allowed_leagues[0] : 0u,
        allowed_league_count > 1 ? allowed_leagues[1] : 0u,
        vector_offset,
        rejected_nation,
        rejected_status,
        rejected_league,
        rejected_team,
        rejected_service_team);
    for (int i = 0; i < selected_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        append_logf(
            "KBO Asian Games selected #%02d player_id=%u team=%u org=%u league=%u age=%u role=%u bucket=%s wildcard=%u score=%d",
            i + 1,
            entry->player_id,
            entry->original_team_id,
            kbo_asian_games_org_team_id_for_team(entry->original_team_id),
            entry->original_league_id,
            (uint32_t)entry->age,
            (uint32_t)entry->role,
            kbo_asian_games_role_bucket_label(entry->role),
            (uint32_t)entry->wildcard,
            entry->score);
    }

    HeapFree(GetProcessHeap(), 0, candidates);
    kbo_save_asian_games_roster_csv(source);
    return selected_count;
}

static int kbo_asian_games_roster_contains_player(uint32_t player_id)
{
    if (player_id == 0u) {
        return 0;
    }
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].player_id == player_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_asian_games_roster_wildcard_count_except(LONG except_index)
{
    int count = 0;
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (i != except_index && g_kbo_asian_games_roster[i].wildcard != 0u) {
            count++;
        }
    }
    return count;
}

static int kbo_asian_games_replacement_allowed_for_org(uint32_t new_org_id, uint32_t old_org_id, LONG old_index)
{
    if (new_org_id == 0u || new_org_id == old_org_id) {
        return 1;
    }

    int count = 0;
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    for (LONG i = 0; i < roster_count; i++) {
        if (i == old_index) {
            continue;
        }
        uint32_t roster_org_id = kbo_asian_games_org_team_id_for_team(g_kbo_asian_games_roster[i].original_team_id);
        if (roster_org_id == new_org_id) {
            count++;
        }
    }
    return count < KBO_ASIAN_GAMES_TEAM_MAX_PLAYERS;
}

static int kbo_asian_games_player_unavailable_for_departure(uint8_t* player)
{
    if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
        return 1;
    }
    return player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] != 0u
        || player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_DFA_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_LOAN_ACTIVE_FLAG_OFFSET] != 0u
        || player[OOTP27_PLAYER_MILITARY_ACTIVE_OFFSET] != 0u
        || player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] != 0u;
}

static int kbo_asian_games_find_replacement_for_entry(
    KboAsianGamesRosterEntry* old_entry,
    LONG old_index,
    KboAsianGamesRosterEntry* out_entry,
    uint32_t* out_org_id,
    const char* source)
{
    if (old_entry == NULL || out_entry == NULL) {
        return 0;
    }
    memset(out_entry, 0, sizeof(*out_entry));
    if (out_org_id != NULL) {
        *out_org_id = 0u;
    }

    uintptr_t player_vector = 0;
    int32_t player_count = 0;
    uint32_t vector_offset = 0;
    if (!find_kbo_global_player_vector(&player_vector, &player_count, &vector_offset)) {
        append_logf("KBO Asian Games replacement skipped source=%s old_player=%u reason=player_vector_unavailable", source != NULL ? source : "", old_entry->player_id);
        return 0;
    }

    uint32_t kbo_league_id = kbo_get_foreign_waiver_league_id();
    if (kbo_league_id == 0u) {
        kbo_league_id = kbo_resolve_kbo_league_id();
    }
    uint32_t allowed_leagues[KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES] = {0};
    int allowed_league_count = 0;
    if (kbo_league_id != 0u) {
        allowed_leagues[allowed_league_count++] = kbo_league_id;
    }
    if (kbo_league_id == 200u && allowed_league_count < KBO_ASIAN_GAMES_MAX_ALLOWED_LEAGUES) {
        allowed_leagues[allowed_league_count++] = 201u;
    }

    int wildcard_count_without_old = kbo_asian_games_roster_wildcard_count_except(old_index);
    uint32_t old_org_id = kbo_asian_games_org_team_id_for_team(old_entry->original_team_id);
    int best_score = -2147483647;
    KboAsianGamesRosterEntry best_entry;
    memset(&best_entry, 0, sizeof(best_entry));
    uint32_t best_org_id = 0u;
    int scanned = 0;
    int rejected_service_team = 0;

    for (int32_t i = 0; i < player_count; i++) {
        uintptr_t player_ptr = *(uintptr_t*)(player_vector + ((uintptr_t)i * sizeof(uintptr_t)));
        if (!kbo_player_pointer_plausible(player_ptr)) {
            continue;
        }
        scanned++;
        uint8_t* player = (uint8_t*)player_ptr;
        uint32_t player_id = *(uint32_t*)(player + OOTP27_PLAYER_ID_OFFSET);
        if (player_id == 0u || player_id == old_entry->player_id || kbo_asian_games_roster_contains_player(player_id)) {
            continue;
        }
        if (*(uint32_t*)(player + OOTP27_PLAYER_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (kbo_asian_games_player_unavailable_for_departure(player)) {
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        if (!kbo_asian_games_league_allowed(current_league_id, allowed_leagues, allowed_league_count)) {
            continue;
        }
        uint8_t* current_team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        if (current_team == NULL
                || *(uint32_t*)(current_team + OOTP27_KBO_TEAM_NATION_ID_OFFSET) != OOTP27_KBO_KOREA_NATION_ID) {
            continue;
        }
        if (kbo_asian_games_team_is_service_or_ulsan(current_team)) {
            rejected_service_team++;
            continue;
        }

        uint8_t role = player[OOTP27_PLAYER_POSITION_GROUP_OFFSET];
        if (!kbo_asian_games_roles_same_bucket(role, old_entry->role)) {
            continue;
        }
        uint16_t age = *(uint16_t*)(player + OOTP27_PLAYER_AGE_OFFSET);
        int wildcard = age > 24u;
        if (wildcard && wildcard_count_without_old >= KBO_ASIAN_GAMES_MAX_WILDCARDS) {
            continue;
        }
        uint32_t org_id = kbo_asian_games_org_team_id_for_team(current_team_id);
        if (!kbo_asian_games_replacement_allowed_for_org(org_id, old_org_id, old_index)) {
            continue;
        }

        int score = kbo_asian_games_player_score(player);
        if (best_entry.player_id == 0u || score > best_score) {
            memset(&best_entry, 0, sizeof(best_entry));
            best_entry.player_id = player_id;
            best_entry.original_team_id = current_team_id;
            best_entry.original_league_id = current_league_id;
            best_entry.age = age;
            best_entry.role = role;
            best_entry.wildcard = wildcard ? 1u : 0u;
            best_entry.score = score;
            best_entry.player_ptr = player_ptr;
            best_score = score;
            best_org_id = org_id;
        }
    }

    if (best_entry.player_id == 0u) {
        append_logf(
            "KBO Asian Games replacement unavailable source=%s old_player=%u old_role=%u scanned=%d rejected_service_team=%d vector_offset=0x%x",
            source != NULL ? source : "",
            old_entry->player_id,
            (uint32_t)old_entry->role,
            scanned,
            rejected_service_team,
            vector_offset);
        return 0;
    }

    *out_entry = best_entry;
    if (out_org_id != NULL) {
        *out_org_id = best_org_id;
    }
    return 1;
}

static int kbo_emit_asian_games_replacement_news(
    uint32_t event_yyyymmdd,
    KboAsianGamesRosterEntry* old_entry,
    KboAsianGamesRosterEntry* new_entry,
    const char* source);

static int kbo_asian_games_replace_unavailable_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    int replacements = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->departed != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player != NULL && kbo_player_pointer_plausible((uintptr_t)player)) {
            entry->player_ptr = (uintptr_t)player;
        }
        if (!kbo_asian_games_player_unavailable_for_departure(player)) {
            continue;
        }

        KboAsianGamesRosterEntry old_entry = *entry;
        KboAsianGamesRosterEntry new_entry;
        uint32_t new_org_id = 0u;
        if (!kbo_asian_games_find_replacement_for_entry(entry, i, &new_entry, &new_org_id, source)) {
            append_logf(
                "KBO Asian Games replacement failed source=%s date=%u old_player=%u slot=%ld",
                source != NULL ? source : "",
                event_yyyymmdd,
                entry->player_id,
                i + 1);
            continue;
        }

        g_kbo_asian_games_roster[i] = new_entry;
        replacements++;
        append_logf(
            "KBO Asian Games injury replacement source=%s date=%u slot=%ld old_player=%u old_team=%u new_player=%u new_team=%u new_org=%u wildcard=%u score=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            i + 1,
            old_entry.player_id,
            old_entry.original_team_id,
            new_entry.player_id,
            new_entry.original_team_id,
            new_org_id,
            (uint32_t)new_entry.wildcard,
            new_entry.score);
        kbo_emit_asian_games_replacement_news(event_yyyymmdd, &old_entry, &new_entry, source);
    }

    if (replacements > 0) {
        kbo_save_asian_games_roster_csv(source);
    }
    return replacements;
}

static void kbo_record_asian_games_restricted_reason(
    KboAsianGamesRosterEntry* entry,
    uint32_t event_yyyymmdd,
    uint32_t league_id,
    uint32_t team_id,
    const char* source)
{
    if (entry == NULL || entry->player_id == 0u || event_yyyymmdd == 0u || league_id == 0u || team_id == 0u) {
        return;
    }

    uint32_t year = event_yyyymmdd / 10000u;
    uint32_t month = (event_yyyymmdd / 100u) % 100u;
    uint32_t day = event_yyyymmdd % 100u;

    char player_link[128] = {0};
    char team_link[128] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));
    kbo_copy_asian_games_team_link(team_id, team_link, sizeof(team_link));

    char league_text[512] = {0};
    char team_text[512] = {0};
    char history_text[512] = {0};
    snprintf(
        league_text,
        sizeof(league_text),
        "%s: Placed %s on the restricted list for Asian Games national team duty.",
        team_link[0] != '\0' ? team_link : "KBO club",
        player_link[0] != '\0' ? player_link : "a player");
    snprintf(
        team_text,
        sizeof(team_text),
        "Placed %s on the restricted list for Asian Games national team duty.",
        player_link[0] != '\0' ? player_link : "a player");
    snprintf(
        history_text,
        sizeof(history_text),
        "Placed on the restricted list for Asian Games national team duty.");

    insert_kbo_roster_transaction_sql(
        league_id,
        team_id,
        year,
        month,
        day,
        0u,
        league_text,
        team_text,
        source);
    insert_kbo_player_history_sql(
        entry->player_id,
        year,
        month,
        day,
        history_text,
        source);
}

static int kbo_asian_games_depart_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        append_logf(
            "KBO Asian Games departure skipped source=%s date=%u reason=no_roster count=%ld year=%u",
            source != NULL ? source : "",
            event_yyyymmdd,
            roster_count,
            g_kbo_asian_games_roster_year);
        return 0;
    }

    int replacements = kbo_asian_games_replace_unavailable_players(event_yyyymmdd, source);
    if (replacements > 0) {
        roster_count = g_kbo_asian_games_roster_count;
    }

    int departed = 0;
    int missing = 0;
    int no_team = 0;
    int32_t days_left = kbo_asian_games_days_until_return(event_yyyymmdd);
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->departed != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            missing++;
            append_logf(
                "KBO Asian Games departure missing player_id=%u index=%ld",
                entry->player_id,
                i + 1);
            continue;
        }

        uint32_t current_team_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t current_league_id = *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(current_team_id, 0);
        if (team == NULL) {
            no_team++;
            append_logf(
                "KBO Asian Games departure no team player_id=%u team=%u league=%u index=%ld",
                entry->player_id,
                current_team_id,
                current_league_id,
                i + 1);
            continue;
        }

        entry->player_ptr = (uintptr_t)player;
        if (entry->original_team_id == 0u) {
            entry->original_team_id = current_team_id;
        }
        if (entry->original_league_id == 0u) {
            entry->original_league_id = current_league_id;
        }
        entry->old_restricted = player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET];
        entry->old_secondary_restricted = player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET];
        entry->old_injury_active = player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET];
        entry->old_injury_days_left = (int16_t)kbo_military_days_left(player);
        entry->departure_date = event_yyyymmdd;
        entry->return_date = kbo_asian_games_final_date_for_year(event_yyyymmdd / 10000u);

        int removed = kbo_remove_player_id_from_known_team_roster_arrays(team, entry->player_id);
        int added_restricted = kbo_add_player_id_to_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, entry->player_id);
        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = 1u;
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = 1u;
        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = 0u;
        kbo_set_military_days_left(player, days_left);
        kbo_record_asian_games_restricted_reason(
            entry,
            event_yyyymmdd,
            current_league_id,
            current_team_id,
            source);
        entry->departed = 1u;
        departed++;

        append_logf(
            "KBO Asian Games departed #%ld player_id=%u team=%u league=%u role=%u bucket=%s days_left=%d removed=%d added_restricted=%d old_restricted=%u old_secondary=%u old_injury=%u old_days=%d",
            i + 1,
            entry->player_id,
            current_team_id,
            current_league_id,
            (uint32_t)entry->role,
            kbo_asian_games_role_bucket_label(entry->role),
            days_left,
            removed,
            added_restricted,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left);
    }

    append_logf(
        "KBO Asian Games departure processed source=%s date=%u roster=%ld departed=%d missing=%d no_team=%d days_left=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        roster_count,
        departed,
        missing,
        no_team,
        days_left);
    kbo_save_asian_games_roster_csv(source);
    return departed;
}

static int kbo_asian_games_finalize_selected_players(uint32_t event_yyyymmdd, const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        append_logf(
            "KBO Asian Games final skipped source=%s date=%u reason=no_roster count=%ld year=%u",
            source != NULL ? source : "",
            event_yyyymmdd,
            roster_count,
            g_kbo_asian_games_roster_year);
        return 0;
    }

    int returned = 0;
    int exempted = 0;
    int missing = 0;
    int no_team = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->returned != 0u) {
            continue;
        }

        uint8_t* player = kbo_find_player_by_id(entry->player_id, NULL, NULL);
        if (player == NULL || !kbo_player_pointer_plausible((uintptr_t)player)) {
            missing++;
            append_logf(
                "KBO Asian Games final missing player_id=%u index=%ld",
                entry->player_id,
                i + 1);
            continue;
        }

        uint32_t team_id = entry->original_team_id != 0u
            ? entry->original_team_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET);
        uint32_t league_id = entry->original_league_id != 0u
            ? entry->original_league_id
            : *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET);
        uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 0);
        if (team == NULL) {
            no_team++;
            append_logf(
                "KBO Asian Games final no team player_id=%u team=%u league=%u index=%ld",
                entry->player_id,
                team_id,
                league_id,
                i + 1);
            continue;
        }

        int removed_restricted = kbo_remove_player_id_from_team_fixed_array(team, OOTP27_TEAM_RESTRICTED_PLAYER_IDS_OFFSET, entry->player_id);
        int added_assignment = kbo_add_player_id_to_team_assignment_arrays(team, entry->player_id);
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_TEAM_ID_OFFSET) = team_id;
        *(uint32_t*)(player + OOTP27_PLAYER_CURRENT_LEAGUE_ID_OFFSET) = league_id;
        *(uint32_t*)(player + OOTP27_PLAYER_ACTIVE_TEAM_ID_OFFSET) = team_id;

        complete_kbo_military_service_status(player);
        entry->exempted = 1u;
        exempted++;

        player[OOTP27_PLAYER_RESTRICTED_FLAG_OFFSET] = entry->old_restricted;
        player[OOTP27_PLAYER_SECONDARY_RESTRICTED_FLAG_OFFSET] = entry->old_secondary_restricted;
        player[OOTP27_PLAYER_INJURY_ACTIVE_OFFSET] = entry->old_injury_active;
        kbo_set_military_days_left(player, entry->old_injury_days_left);
        if (player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] == 0u) {
            player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET] = 1u;
        }

        entry->returned = 1u;
        returned++;
        append_logf(
            "KBO Asian Games finalized #%ld player_id=%u team=%u league=%u exempted=%u removed_restricted=%d added_assignment=%d restored_restricted=%u restored_secondary=%u restored_injury=%u restored_days=%d",
            i + 1,
            entry->player_id,
            team_id,
            league_id,
            (uint32_t)player[OOTP27_PLAYER_MILITARY_EXEMPT_OFFSET],
            removed_restricted,
            added_assignment,
            (uint32_t)entry->old_restricted,
            (uint32_t)entry->old_secondary_restricted,
            (uint32_t)entry->old_injury_active,
            (int)entry->old_injury_days_left);
    }

    append_logf(
        "KBO Asian Games final processed source=%s date=%u roster=%ld returned=%d exempted=%d missing=%d no_team=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        roster_count,
        returned,
        exempted,
        missing,
        no_team);
    kbo_save_asian_games_roster_csv(source);
    return returned;
}

static int kbo_asian_games_roster_already_finalized(const char* source)
{
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        kbo_load_asian_games_roster_csv(source);
        roster_count = g_kbo_asian_games_roster_count;
    }
    if (roster_count <= 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        return 0;
    }

    int finalized = 0;
    for (LONG i = 0; i < roster_count; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id != 0u && entry->returned != 0u) {
            finalized++;
        }
    }
    return finalized > 0 && finalized == roster_count;
}

static void kbo_copy_asian_games_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        char city[64] = {0};
        char nickname[64] = {0};
        char full_name[96] = {0};
        copy_ootp_string_object_text(team, 0x10u, city, sizeof(city));
        copy_ootp_string_object_text(team, 0x28u, nickname, sizeof(nickname));
        copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));
        if (full_name[0] != '\0' && strchr(full_name, ' ') != NULL) {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0' && !ascii_equals_ignore_case(city, nickname)) {
            snprintf(out, out_size, "%s %s", city, nickname);
            return;
        }
        if (full_name[0] != '\0') {
            snprintf(out, out_size, "%s", full_name);
            return;
        }
        if (nickname[0] != '\0') {
            snprintf(out, out_size, "%s", nickname);
            return;
        }
    }

    if (team_id != 0u) {
        snprintf(out, out_size, "his club");
    }
}

static void kbo_copy_asian_games_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    kbo_copy_asian_games_team_name(team_id, team_name, sizeof(team_name));
    if (team_name[0] == '\0' || ascii_equals_ignore_case(team_name, "his club")) {
        if (team_id != 0u) {
            snprintf(out, out_size, "<Team #%u:team#%u>", team_id, team_id);
        }
        return;
    }
    if (team_id != 0u) {
        snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
    } else {
        snprintf(out, out_size, "%s", team_name);
    }
}

static void kbo_copy_asian_games_player_link(KboAsianGamesRosterEntry* entry, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    if (entry == NULL || entry->player_id == 0u) {
        snprintf(out, out_size, "a Korea squad member");
        return;
    }

    uintptr_t player_ptr = entry->player_ptr;
    if (!kbo_player_pointer_plausible(player_ptr)) {
        player_ptr = (uintptr_t)kbo_find_player_by_id(entry->player_id, NULL, NULL);
    }

    char player_name[64] = {0};
    if (kbo_player_pointer_plausible(player_ptr)) {
        kbo_copy_player_display_name((uint8_t*)player_ptr, player_name, sizeof(player_name));
    }
    if (player_name[0] == '\0') {
        snprintf(player_name, sizeof(player_name), "Player #%u", entry->player_id);
    }

    snprintf(out, out_size, "<%s:player#%u>", player_name, entry->player_id);
}

static int kbo_asian_games_append_player_blurb(
    char* out,
    size_t out_size,
    size_t* used,
    KboAsianGamesRosterEntry* entry,
    LONG display_index,
    LONG display_count)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1 || entry == NULL
            || display_index < 0 || display_index >= display_count) {
        return 0;
    }

    char player_link[96] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));

    char team_link[128] = {0};
    kbo_copy_asian_games_team_link(entry->original_team_id, team_link, sizeof(team_link));

    const char* bucket = kbo_asian_games_role_bucket_label(entry->role);
    int wrote = snprintf(
        out + *used,
        out_size - *used,
        "%s%s%s%s",
        display_index == 0 ? "" : (display_index == display_count - 1 ? " and " : ", "),
        player_link,
        team_link[0] != '\0' ? " of " : "",
        team_link);
    if (wrote <= 0) {
        return 0;
    }
    *used += (size_t)wrote;
    if (*used < out_size - 1 && bucket[0] != '\0') {
        wrote = snprintf(out + *used, out_size - *used, " (%s)", bucket);
        if (wrote > 0) {
            *used += (size_t)wrote;
        }
    }
    return 1;
}

static int kbo_asian_games_append_roster_line(
    char* out,
    size_t out_size,
    size_t* used,
    LONG index,
    KboAsianGamesRosterEntry* entry)
{
    if (out == NULL || out_size == 0 || used == NULL || *used >= out_size - 1 || entry == NULL) {
        return 0;
    }

    char player_link[96] = {0};
    kbo_copy_asian_games_player_link(entry, player_link, sizeof(player_link));

    char team_link[128] = {0};
    kbo_copy_asian_games_team_link(entry->original_team_id, team_link, sizeof(team_link));
    if (team_link[0] == '\0') {
        snprintf(team_link, sizeof(team_link), "unattached");
    }

    const char* status = "selected";
    if (entry->returned) {
        status = entry->exempted ? "returned, exempt" : "returned";
    } else if (entry->departed) {
        status = "on tournament leave";
    }

    int wrote = snprintf(
        out + *used,
        out_size - *used,
        "%02ld. %s, %s, %s%s, age %u, %s\n",
        index + 1,
        player_link,
        kbo_asian_games_role_bucket_label(entry->role),
        team_link,
        entry->wildcard ? ", wild card" : "",
        (uint32_t)entry->age,
        status);
    if (wrote <= 0) {
        return 0;
    }
    *used += (size_t)wrote;
    return 1;
}

static KboAsianGamesRosterEntry* kbo_asian_games_choose_captain(void)
{
    KboAsianGamesRosterEntry* best = NULL;
    for (LONG i = 0; i < g_kbo_asian_games_roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id == 0u || entry->wildcard == 0u) {
            continue;
        }
        if (best == NULL || entry->score > best->score) {
            best = entry;
        }
    }
    if (best != NULL) {
        return best;
    }
    for (LONG i = 0; i < g_kbo_asian_games_roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        if (entry->player_id != 0u && (best == NULL || entry->score > best->score)) {
            best = entry;
        }
    }
    return best;
}

static void kbo_build_asian_games_news_body(char* out, size_t out_size, const char* title, const char* lead)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';

    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }

    int departed = 0;
    int returned = 0;
    int exempted = 0;
    int wildcards = 0;
    for (LONG i = 0; i < roster_count; i++) {
        if (g_kbo_asian_games_roster[i].departed) { departed++; }
        if (g_kbo_asian_games_roster[i].returned) { returned++; }
        if (g_kbo_asian_games_roster[i].exempted) { exempted++; }
        if (g_kbo_asian_games_roster[i].wildcard) { wildcards++; }
    }
    KboAsianGamesRosterEntry* captain = kbo_asian_games_choose_captain();
    char captain_link[96] = {0};
    if (captain != NULL) {
        kbo_copy_asian_games_player_link(captain, captain_link, sizeof(captain_link));
    }
    int include_roster = title != NULL && strstr(title, "Roster") != NULL;
    int include_samples = include_roster;

    int len = snprintf(
        out,
        out_size,
        "%s ",
        lead != NULL && lead[0] != '\0' ? lead : "The KBO issued an Asian Games update.");
    if (len < 0) {
        out[0] = '\0';
        return;
    }

    size_t used = strlen(out);
    if (title != NULL && strstr(title, "Roster") != NULL) {
        len = snprintf(
            out + used,
            out_size - used,
            "The Korean baseball delegation has settled on a %ld-man squad for the Asian Games, balancing young domestic standouts with %d over-age wild-card selection%s. League officials framed the group as a tournament roster built for short-series flexibility rather than a straight all-star list.\n\nThe first wave of attention is expected to fall on ",
            roster_count,
            wildcards,
            wildcards == 1 ? "" : "s");
    } else if (title != NULL && strstr(title, "Depart") != NULL) {
        len = snprintf(
            out + used,
            out_size - used,
            "%d player%s will now leave club duty for the Asian Games window, forcing several teams to cover short-term roster holes during a sensitive part of the league calendar. The delegation will gather before moving into tournament preparation, with the KBO saying clubs have already been notified of the final travel list.",
            departed,
            departed == 1 ? "" : "s");
    } else if (title != NULL && (strstr(title, "Return") != NULL || strstr(title, "Gold") != NULL || strstr(title, "Win") != NULL)) {
        len = snprintf(
            out + used,
            out_size - used,
            "%d player%s returned to their clubs after the Asian Games final, with %d player%s receiving military exemption status as part of Korea's championship result. Teams can now fold the group back into the pennant race and postseason picture.",
            returned,
            returned == 1 ? "" : "s",
            exempted,
            exempted == 1 ? "" : "s");
    } else {
        len = snprintf(
            out + used,
            out_size - used,
            "Korea's Asian Games group stands at %ld players for %u.\n\nThe squad includes ",
            roster_count,
            g_kbo_asian_games_roster_year);
    }
    if (len <= 0) {
        return;
    }
    used += (size_t)len;

    if (include_samples) {
        LONG sample_count = roster_count < 6 ? roster_count : 6;
        for (LONG i = 0; i < sample_count && used + 160u < out_size; i++) {
            LONG roster_index = (i == sample_count - 1 && roster_count > sample_count)
                ? roster_count - 1
                : i;
            kbo_asian_games_append_player_blurb(
                out,
                out_size,
                &used,
                &g_kbo_asian_games_roster[roster_index],
                i,
                sample_count);
        }
    }

    if (used < out_size - 1) {
        len = snprintf(
            out + used,
            out_size - used,
            "%s\n\nThe league said the final decisions reflected player availability, age rules and positional balance, with clubs notified before the roster was made public.",
            include_samples ? "." : "");
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster && captain_link[0] != '\0' && used < out_size - 1) {
        len = snprintf(
            out + used,
            out_size - used,
            "\n\n%s, named captain after being selected as one of the wild-card players, said the older players understand what is expected of them. \"This roster belongs to the young players,\" he said. \"Our job is to make the game slow down for them and make sure Korea is ready from the first pitch.\"",
            captain_link);
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster && used < out_size - 1) {
        len = snprintf(out + used, out_size - used, "\n\nComplete roster:\n");
        if (len > 0) {
            used += (size_t)len;
        }
    }

    if (include_roster) {
        for (LONG i = 0; i < roster_count && used + 140u < out_size; i++) {
            kbo_asian_games_append_roster_line(
                out,
                out_size,
                &used,
                i,
                &g_kbo_asian_games_roster[i]);
        }
    }
}

static int kbo_emit_asian_games_news(uint32_t event_yyyymmdd, const char* title, const char* lead, const char* source)
{
    if (event_yyyymmdd == 0u || title == NULL || title[0] == '\0') {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        append_logf(
            "KBO Asian Games news skipped source=%s title=%s reason=league_id_unavailable",
            source != NULL ? source : "",
            title);
        return 0;
    }

    char body[8192] = {0};
    kbo_build_asian_games_news_body(body, sizeof(body), title, lead);
    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        title,
        body);
    append_logf(
        "KBO Asian Games news source=%s title=%s date=%u league_id=%u created=%d",
        source != NULL ? source : "",
        title,
        event_yyyymmdd,
        league_id,
        created);
    return created;
}

static int kbo_emit_asian_games_replacement_news(
    uint32_t event_yyyymmdd,
    KboAsianGamesRosterEntry* old_entry,
    KboAsianGamesRosterEntry* new_entry,
    const char* source)
{
    if (event_yyyymmdd == 0u || old_entry == NULL || new_entry == NULL) {
        return 0;
    }

    uint32_t league_id = kbo_get_foreign_waiver_league_id();
    if (league_id == 0u) {
        league_id = kbo_resolve_kbo_league_id();
    }
    if (league_id == 0u) {
        return 0;
    }

    char old_player[96] = {0};
    char new_player[96] = {0};
    char old_team[128] = {0};
    char new_team[128] = {0};
    kbo_copy_asian_games_player_link(old_entry, old_player, sizeof(old_player));
    kbo_copy_asian_games_player_link(new_entry, new_player, sizeof(new_player));
    kbo_copy_asian_games_team_link(old_entry->original_team_id, old_team, sizeof(old_team));
    kbo_copy_asian_games_team_link(new_entry->original_team_id, new_team, sizeof(new_team));

    char body[2048] = {0};
    snprintf(
        body,
        sizeof(body),
        "The KBO announced a late change to Korea's Asian Games roster after %s%s%s was ruled unavailable through injury or roster status before the team's departure.\n\n%s%s%s has been added as the replacement, keeping the roster at %d players and preserving the same position group. League officials said the change was made before travel paperwork was finalized, allowing the national team to depart with a full tournament squad.",
        old_player,
        old_team[0] != '\0' ? " of " : "",
        old_team,
        new_player,
        new_team[0] != '\0' ? " of " : "",
        new_team,
        KBO_ASIAN_GAMES_ROSTER_SIZE);

    int created = create_kbo_native_live_news_with_body(
        event_yyyymmdd / 10000u,
        (event_yyyymmdd / 100u) % 100u,
        event_yyyymmdd % 100u,
        league_id,
        OOTP27_EVENT_TYPE_CUSTOM_EVENT,
        "[KBO] Asian Games Roster Change",
        body);
    append_logf(
        "KBO Asian Games replacement news source=%s date=%u old_player=%u new_player=%u created=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        old_entry->player_id,
        new_entry->player_id,
        created);
    return created;
}

static int kbo_handle_asian_games_selection_event(uint32_t event_yyyymmdd, const char* source)
{
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_selection_fired_date == event_yyyymmdd) {
        return 0;
    }
    int selected = kbo_select_asian_games_roster(event_yyyymmdd, source);
    if (selected <= 0) {
        append_logf(
            "KBO Asian Games selection deferred source=%s date=%u selected=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            selected);
        return 0;
    }
    g_kbo_asian_games_last_selection_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        event_yyyymmdd,
        "[KBO] Asian Games Roster Announced",
        "The KBO has announced Korea's Asian Games roster.",
        source);
    append_logf(
        "KBO Asian Games selection reached source=%s date=%u selected=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        selected);
    return 1;
}

static int kbo_handle_asian_games_departure_event(uint32_t event_yyyymmdd, const char* source)
{
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_departure_fired_date == event_yyyymmdd) {
        return 0;
    }
    int departed = kbo_asian_games_depart_selected_players(event_yyyymmdd, source);
    if (departed <= 0) {
        append_logf(
            "KBO Asian Games departure deferred source=%s date=%u departed=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            departed);
        return 0;
    }
    g_kbo_asian_games_last_departure_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        event_yyyymmdd,
        "[KBO] Asian Games Players Depart",
        "Selected players have departed their clubs for Asian Games duty.",
        source);
    append_logf(
        "KBO Asian Games departure reached source=%s date=%u departed=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        departed);
    return 1;
}

static int kbo_handle_asian_games_final_event(uint32_t event_yyyymmdd, const char* source)
{
    if (event_yyyymmdd == 0u || g_kbo_asian_games_last_final_fired_date == event_yyyymmdd) {
        return 0;
    }
    int returned = kbo_asian_games_finalize_selected_players(event_yyyymmdd, source);
    int already_finalized = returned <= 0 ? kbo_asian_games_roster_already_finalized(source) : 0;
    if (returned <= 0 && !already_finalized) {
        append_logf(
            "KBO Asian Games final deferred source=%s date=%u returned=%d",
            source != NULL ? source : "",
            event_yyyymmdd,
            returned);
        return 0;
    }
    g_kbo_asian_games_last_final_fired_date = event_yyyymmdd;
    kbo_emit_asian_games_news(
        event_yyyymmdd,
        "[KBO] Korea Wins Asian Games Gold",
        "Korea has won the Asian Games baseball title.",
        source);
    append_logf(
        "KBO Asian Games final reached source=%s date=%u returned=%d already_finalized=%d",
        source != NULL ? source : "",
        event_yyyymmdd,
        returned,
        already_finalized);
    return 1;
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
        int is_asian_games_selection_event = kbo_custom_event_name_is_asian_games_selection(name);
        int is_asian_games_departure_event = kbo_custom_event_name_is_asian_games_departure(name);
        int is_asian_games_final_event = kbo_custom_event_name_is_asian_games_final(name);

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
        } else if (is_asian_games_selection_event) {
            action_result = kbo_handle_asian_games_selection_event(event_yyyymmdd, source);
        } else if (is_asian_games_departure_event) {
            action_result = kbo_handle_asian_games_departure_event(event_yyyymmdd, source);
        } else if (is_asian_games_final_event) {
            action_result = kbo_handle_asian_games_final_event(event_yyyymmdd, source);
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
        kbo_schedule_asian_games_custom_events(g_kbo_default_event_source);
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
