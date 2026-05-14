#include "../../runtime/common/custom_events_common.h"
#include "body.h"
#include <stdio.h>
#include <string.h>
#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/logging/core_log.h"
#include "../../../core/dates/core_current_date.h"
#include "../../../core/files/save_paths/core_save_paths.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../links/links.h"

static int kbo_asian_games_news_uses_korean(void)
{
    const char* language_dir = kbo_custom_news_language_dir();
    return language_dir == NULL || strcmp(language_dir, "en") != 0;
}

static void kbo_asian_games_copy_text(char* out, size_t out_size, const char* text)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    snprintf(out, out_size, "%s", text != NULL ? text : "");
}

typedef struct KboAsianGamesKoreanNameMap {
    const char* raw;
    const char* korean;
} KboAsianGamesKoreanNameMap;

static const char* kbo_asian_games_korean_host_name(const char* raw)
{
    static const KboAsianGamesKoreanNameMap map[] = {
        { "Abu Dhabi", "\xec\x95\x84\xeb\xb6\x80\xeb\x8b\xa4\xeb\xb9\x84" },
        { "Aichi-Nagoya", "\xec\x95\x84\xec\x9d\xb4\xec\xb9\x98\xc2\xb7\xeb\x82\x98\xea\xb3\xa0\xec\x95\xbc" },
        { "Almaty", "\xec\x95\x8c\xeb\xa7\x88\xed\x8b\xb0" },
        { "Astana", "\xec\x95\x84\xec\x8a\xa4\xed\x83\x80\xeb\x82\x98" },
        { "Bangkok", "\xeb\xb0\xa9\xec\xbd\x95" },
        { "Busan", "\xeb\xb6\x80\xec\x82\xb0" },
        { "Cebu", "\xec\x84\xb8\xeb\xb6\x80" },
        { "Chiang Mai", "\xec\xb9\x98\xec\x95\x99\xeb\xa7\x88\xec\x9d\xb4" },
        { "China", "\xec\xa4\x91\xea\xb5\xad" },
        { "Daegu", "\xeb\x8c\x80\xea\xb5\xac" },
        { "Daejeon", "\xeb\x8c\x80\xec\xa0\x84" },
        { "Doha", "\xeb\x8f\x84\xed\x95\x98" },
        { "Dubai", "\xeb\x91\x90\xeb\xb0\x94\xec\x9d\xb4" },
        { "Gwangju", "\xea\xb4\x91\xec\xa3\xbc" },
        { "Hanoi", "\xed\x95\x98\xeb\x85\xb8\xec\x9d\xb4" },
        { "Ho Chi Minh City", "\xed\x98\xb8\xec\xb0\x8c\xeb\xaf\xbc" },
        { "Incheon", "\xec\x9d\xb8\xec\xb2\x9c" },
        { "Indonesia", "\xec\x9d\xb8\xeb\x8f\x84\xeb\x84\xa4\xec\x8b\x9c\xec\x95\x84" },
        { "Jakarta", "\xec\x9e\x90\xec\xb9\xb4\xeb\xa5\xb4\xed\x83\x80" },
        { "Japan", "\xec\x9d\xbc\xeb\xb3\xb8" },
        { "Jeddah", "\xec\xa0\x9c\xeb\x8b\xa4" },
        { "Kaohsiung", "\xea\xb0\x80\xec\x98\xa4\xec\x8a\x9d" },
        { "Kazakhstan", "\xec\xb9\xb4\xec\x9e\x90\xed\x9d\x90\xec\x8a\xa4\xed\x83\x84" },
        { "Korea", "\xed\x95\x9c\xea\xb5\xad" },
        { "Kuala Lumpur", "\xec\xbf\xa0\xec\x95\x8c\xeb\x9d\xbc\xeb\xa3\xb8\xed\x91\xb8\xeb\xa5\xb4" },
        { "Kuwait", "\xec\xbf\xa0\xec\x9b\xa8\xec\x9d\xb4\xed\x8a\xb8" },
        { "Kuwait City", "\xec\xbf\xa0\xec\x9b\xa8\xec\x9d\xb4\xed\x8a\xb8\xec\x8b\x9c\xed\x8b\xb0" },
        { "Malaysia", "\xeb\xa7\x90\xeb\xa0\x88\xec\x9d\xb4\xec\x8b\x9c\xec\x95\x84" },
        { "Manila", "\xeb\xa7\x88\xeb\x8b\x90\xeb\x9d\xbc" },
        { "Mongolia", "\xeb\xaa\xbd\xea\xb3\xa8" },
        { "Muscat", "\xeb\xac\xb4\xec\x8a\xa4\xec\xb9\xb4\xed\x8a\xb8" },
        { "Oman", "\xec\x98\xa4\xeb\xa7\x8c" },
        { "Penang", "\xed\x8e\x98\xeb\x82\xad" },
        { "Philippines", "\xed\x95\x84\xeb\xa6\xac\xed\x95\x80" },
        { "Qatar", "\xec\xb9\xb4\xed\x83\x80\xeb\xa5\xb4" },
        { "Riyadh", "\xeb\xa6\xac\xec\x95\xbc\xeb\x93\x9c" },
        { "Saudi Arabia", "\xec\x82\xac\xec\x9a\xb0\xeb\x94\x94\xec\x95\x84\xeb\x9d\xbc\xeb\xb9\x84\xec\x95\x84" },
        { "Seoul", "\xec\x84\x9c\xec\x9a\xb8" },
        { "Singapore", "\xec\x8b\xb1\xea\xb0\x80\xed\x8f\xac\xeb\xa5\xb4" },
        { "Surabaya", "\xec\x88\x98\xeb\x9d\xbc\xeb\xb0\x94\xec\x95\xbc" },
        { "Taipei", "\xed\x83\x80\xec\x9d\xb4\xeb\xb2\xa0\xec\x9d\xb4" },
        { "Taiwan", "\xeb\x8c\x80\xeb\xa7\x8c" },
        { "Tashkent", "\xed\x83\x80\xec\x8a\x88\xec\xbc\x84\xed\x8a\xb8" },
        { "Thailand", "\xed\x83\x9c\xea\xb5\xad" },
        { "Ulaanbaatar", "\xec\x9a\xb8\xeb\x9e\x80\xeb\xb0\x94\xed\x86\xa0\xeb\xa5\xb4" },
        { "United Arab Emirates", "\xec\x95\x84\xeb\x9e\x8d\xec\x97\x90\xeb\xaf\xb8\xeb\xa6\xac\xed\x8a\xb8" },
        { "Uzbekistan", "\xec\x9a\xb0\xec\xa6\x88\xeb\xb2\xa0\xed\x82\xa4\xec\x8a\xa4\xed\x83\x84" },
        { "Vietnam", "\xeb\xb2\xa0\xed\x8a\xb8\xeb\x82\xa8" },
    };
    if (raw == NULL || raw[0] == '\0') {
        return "";
    }
    for (size_t i = 0u; i < sizeof(map) / sizeof(map[0]); i++) {
        if (ascii_equals_ignore_case(raw, map[i].raw)) {
            return map[i].korean;
        }
    }
    return raw;
}

static uint32_t kbo_asian_games_context_year(uint32_t event_yyyymmdd)
{
    uint32_t year = g_kbo_asian_games_roster_year;
    if (year < 1982u || year > 2200u) {
        year = event_yyyymmdd / 10000u;
    }
    if (year < 1982u || year > 2200u) {
        uint32_t current_year = 0u;
        uint32_t current_month = 0u;
        uint32_t current_day = 0u;
        if (kbo_current_date_is_valid(&current_year, &current_month, &current_day)) {
            year = current_year;
        }
    }
    return year;
}

void kbo_asian_games_build_news_context(
    uint32_t event_yyyymmdd,
    char* roster_year_text,
    size_t roster_year_size,
    char* host_city,
    size_t host_city_size,
    char* host_country,
    size_t host_country_size,
    char* host_place,
    size_t host_place_size,
    char* tournament_label,
    size_t tournament_label_size,
    char* tournament_phrase,
    size_t tournament_phrase_size)
{
    uint32_t year = kbo_asian_games_context_year(event_yyyymmdd);
    char year_text[16] = {0};
    snprintf(year_text, sizeof(year_text), "%u", year);
    kbo_asian_games_copy_text(roster_year_text, roster_year_size, year_text);
    kbo_asian_games_copy_text(host_city, host_city_size, "");
    kbo_asian_games_copy_text(host_country, host_country_size, "");
    kbo_asian_games_copy_text(host_place, host_place_size, "");
    kbo_asian_games_copy_text(tournament_label, tournament_label_size, "");
    kbo_asian_games_copy_text(tournament_phrase, tournament_phrase_size, "");

    KboAsianGamesScheduleSeed schedule;
    memset(&schedule, 0, sizeof(schedule));
    int has_schedule = year != 0u && kbo_get_asian_games_schedule_for_year(year, &schedule);
    int use_korean = kbo_asian_games_news_uses_korean();
    const char* city = "";
    const char* country = "";
    if (has_schedule) {
        city = use_korean ? kbo_asian_games_korean_host_name(schedule.host_city) : schedule.host_city;
        country = use_korean ? kbo_asian_games_korean_host_name(schedule.host_country) : schedule.host_country;
    }
    kbo_asian_games_copy_text(host_city, host_city_size, city);
    kbo_asian_games_copy_text(host_country, host_country_size, country);

    char place[128] = {0};
    if (city[0] != '\0' && country[0] != '\0') {
        if (use_korean) {
            snprintf(place, sizeof(place), "%s %s", country, city);
        } else {
            snprintf(place, sizeof(place), "%s, %s", city, country);
        }
    } else if (city[0] != '\0') {
        snprintf(place, sizeof(place), "%s", city);
    } else if (country[0] != '\0') {
        snprintf(place, sizeof(place), "%s", country);
    }
    kbo_asian_games_copy_text(host_place, host_place_size, place);

    if (use_korean) {
        if (place[0] != '\0') {
            snprintf(
                tournament_label,
                tournament_label_size,
                "%s %u\xeb\x85\x84 \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84",
                place,
                year);
            snprintf(
                tournament_phrase,
                tournament_phrase_size,
                "%s\xec\x97\x90\xec\x84\x9c \xec\x97\xb4\xeb\xa6\xac\xeb\x8a\x94 %u\xeb\x85\x84 \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84",
                place,
                year);
        } else {
            snprintf(
                tournament_label,
                tournament_label_size,
                "%u\xeb\x85\x84 \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84",
                year);
            snprintf(
                tournament_phrase,
                tournament_phrase_size,
                "%u\xeb\x85\x84 \xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84",
                year);
        }
    } else {
        if (place[0] != '\0') {
            snprintf(tournament_label, tournament_label_size, "%u Asian Games in %s", year, place);
            snprintf(tournament_phrase, tournament_phrase_size, "the %u Asian Games in %s", year, place);
        } else {
            snprintf(tournament_label, tournament_label_size, "%u Asian Games", year);
            snprintf(tournament_phrase, tournament_phrase_size, "the %u Asian Games", year);
        }
    }
}

static uint32_t kbo_asian_games_news_hash_add(uint32_t hash, uint32_t value)
{
    hash ^= value;
    hash *= 16777619u;
    hash ^= hash >> 13;
    return hash;
}

void kbo_asian_games_build_final_matchup(
    uint32_t event_yyyymmdd,
    int gold_won,
    char* final_opponent,
    size_t final_opponent_size,
    char* final_score,
    size_t final_score_size,
    char* korea_score,
    size_t korea_score_size,
    char* opponent_score,
    size_t opponent_score_size)
{
    int use_korean = kbo_asian_games_news_uses_korean();
    static const char* korean_opponents[] = {
        "\xec\x9d\xbc\xeb\xb3\xb8",
        "\xeb\x8c\x80\xeb\xa7\x8c",
        "\xec\xa4\x91\xea\xb5\xad",
        "\xec\x9d\xbc\xeb\xb3\xb8",
        "\xeb\x8c\x80\xeb\xa7\x8c",
        "\xed\x95\x84\xeb\xa6\xac\xed\x95\x80",
    };
    static const char* english_opponents[] = {
        "Japan",
        "Taiwan",
        "China",
        "Japan",
        "Taiwan",
        "the Philippines",
    };

    uint32_t hash = 2166136261u;
    hash = kbo_asian_games_news_hash_add(hash, event_yyyymmdd);
    hash = kbo_asian_games_news_hash_add(hash, g_kbo_asian_games_roster_year);
    hash = kbo_asian_games_news_hash_add(hash, (uint32_t)gold_won);
    LONG roster_count = g_kbo_asian_games_roster_count;
    if (roster_count < 0 || roster_count > KBO_ASIAN_GAMES_ROSTER_SIZE) {
        roster_count = 0;
    }
    hash = kbo_asian_games_news_hash_add(hash, (uint32_t)roster_count);
    for (LONG i = 0; i < roster_count && i < KBO_ASIAN_GAMES_ROSTER_SIZE; i++) {
        const KboAsianGamesRosterEntry* entry = &g_kbo_asian_games_roster[i];
        hash = kbo_asian_games_news_hash_add(hash, entry->player_id);
        hash = kbo_asian_games_news_hash_add(hash, entry->original_team_id);
        hash = kbo_asian_games_news_hash_add(hash, (uint32_t)entry->age);
        hash = kbo_asian_games_news_hash_add(hash, (uint32_t)entry->role);
        hash = kbo_asian_games_news_hash_add(hash, (uint32_t)entry->wildcard);
        hash = kbo_asian_games_news_hash_add(hash, (uint32_t)entry->score);
    }

    int opponent_count = (int)(sizeof(korean_opponents) / sizeof(korean_opponents[0]));
    const char* opponent = use_korean
        ? korean_opponents[hash % (uint32_t)opponent_count]
        : english_opponents[hash % (uint32_t)opponent_count];
    int korea_runs = gold_won
        ? 3 + (int)(hash % 5u)
        : 1 + (int)(hash % 5u);
    int margin = 1 + (int)((hash >> 8) % 3u);
    int opponent_runs = gold_won ? korea_runs - margin : korea_runs + margin;
    if (gold_won && opponent_runs < 0) {
        opponent_runs = 0;
    }
    if (gold_won && korea_runs <= opponent_runs) {
        korea_runs = opponent_runs + 1;
    }
    if (!gold_won && opponent_runs <= korea_runs) {
        opponent_runs = korea_runs + 1;
    }

    char korea_text[16] = {0};
    char opponent_text[16] = {0};
    snprintf(korea_text, sizeof(korea_text), "%d", korea_runs);
    snprintf(opponent_text, sizeof(opponent_text), "%d", opponent_runs);
    kbo_asian_games_copy_text(final_opponent, final_opponent_size, opponent);
    snprintf(final_score, final_score_size, "%d-%d", korea_runs, opponent_runs);
    kbo_asian_games_copy_text(korea_score, korea_score_size, korea_text);
    kbo_asian_games_copy_text(opponent_score, opponent_score_size, opponent_text);
}

static const char* kbo_asian_games_news_role_label(uint8_t role, int use_korean)
{
    const char* bucket = kbo_asian_games_role_bucket_label(role);
    if (!use_korean) {
        return bucket;
    }
    if (strcmp(bucket, "P") == 0) {
        return "\xed\x88\xac\xec\x88\x98";
    }
    if (strcmp(bucket, "C") == 0) {
        return "\xed\x8f\xac\xec\x88\x98";
    }
    if (strcmp(bucket, "IF") == 0) {
        return "\xeb\x82\xb4\xec\x95\xbc\xec\x88\x98";
    }
    if (strcmp(bucket, "OF") == 0) {
        return "\xec\x99\xb8\xec\x95\xbc\xec\x88\x98";
    }
    return "\xec\x95\xbc\xec\x88\x98";
}

int kbo_asian_games_append_player_blurb(
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

    int use_korean = kbo_asian_games_news_uses_korean();
    const char* bucket = kbo_asian_games_news_role_label(entry->role, use_korean);
    char role_suffix[64] = {0};
    if (bucket[0] != '\0') {
        snprintf(role_suffix, sizeof(role_suffix), " (%s)", bucket);
    }
    const char* separator = "";
    if (display_index != 0) {
        separator = use_korean ? ", " : (display_index == display_count - 1 ? " and " : ", ");
    }
    const char* team_prefix = "";
    if (team_link[0] != '\0') {
        team_prefix = use_korean ? " / " : " of ";
    }

    KboNewsTemplateVar vars[] = {
        { "separator", separator },
        { "player_link", player_link },
        { "team_prefix", team_prefix },
        { "team_link", team_link },
        { "role_suffix", role_suffix },
    };
    char rendered[256] = {0};
    if (!kbo_news_template_render_key(
            "asian_games.player_blurb",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            "asian_games_body")) {
        return 0;
    }
    kbo_news_text_append(out, out_size, rendered);
    *used = strlen(out);
    return 1;
}

int kbo_asian_games_append_roster_line(
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
    int use_korean = kbo_asian_games_news_uses_korean();
    if (team_link[0] == '\0') {
        snprintf(
            team_link,
            sizeof(team_link),
            "%s",
            use_korean ? "\xec\x86\x8c\xec\x86\x8d \xec\x97\x86\xec\x9d\x8c" : "unattached");
    }

    const char* status = use_korean ? "\xec\x84\xa0\xeb\xb0\x9c" : "selected";
    if (entry->returned) {
        status = use_korean
            ? (entry->exempted
                ? "\xeb\xb3\xb5\xea\xb7\x80, \xeb\xb3\x91\xec\x97\xad \xed\x98\x9c\xed\x83\x9d"
                : "\xeb\xb3\xb5\xea\xb7\x80, \xed\x98\x9c\xed\x83\x9d \xec\x97\x86\xec\x9d\x8c")
            : (entry->exempted ? "returned, exempt" : "returned, no exemption");
    } else if (entry->departed) {
        status = use_korean ? "\xec\xb0\xa8\xec\xb6\x9c \xec\xa4\x91" : "on tournament leave";
    }

    char index_text[16] = {0};
    char age_text[16] = {0};
    snprintf(index_text, sizeof(index_text), "%02ld", index + 1);
    snprintf(age_text, sizeof(age_text), "%u", (uint32_t)entry->age);
    KboNewsTemplateVar vars[] = {
        { "index", index_text },
        { "player_link", player_link },
        { "role_bucket", kbo_asian_games_news_role_label(entry->role, use_korean) },
        { "team_link", team_link },
        { "wildcard_text", entry->wildcard
            ? (use_korean ? ", \xec\x99\x80\xec\x9d\xbc\xeb\x93\x9c\xec\xb9\xb4\xeb\x93\x9c" : ", wild card")
            : "" },
        { "age", age_text },
        { "status", status },
    };
    char rendered[256] = {0};
    if (!kbo_news_template_render_key(
            "asian_games.roster_line",
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            "asian_games_body")) {
        return 0;
    }
    kbo_news_text_append(out, out_size, rendered);
    *used = strlen(out);
    return 1;
}

KboAsianGamesRosterEntry* kbo_asian_games_choose_captain(void)
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
