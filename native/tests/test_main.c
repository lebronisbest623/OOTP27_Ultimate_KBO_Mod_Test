#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KBO_MILITARY_SERVICE_DAYS 545
#define KBO_FOREIGN_INJURY_SLOT_REGULAR 1
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA 2

typedef struct KboMilitaryServiceSeed {
    char key[40];
    uint32_t player_id;
    char service_team_code[12];
    char original_team_code[12];
    uint32_t service_start_yyyymmdd;
    uint32_t service_return_yyyymmdd;
    int32_t service_total_days;
} KboMilitaryServiceSeed;

typedef struct KboForeignReplacementPlayerSeed {
    char key[40];
    uint32_t player_id;
    uint8_t slot_type;
} KboForeignReplacementPlayerSeed;

#include "../src/core/core_text_date.inc"
#include "../src/core/core_sql_escape.inc"

static int kbo_current_date_is_valid(uint32_t* out_year, uint32_t* out_month, uint32_t* out_day)
{
    if (out_year != NULL) {
        *out_year = 0u;
    }
    if (out_month != NULL) {
        *out_month = 0u;
    }
    if (out_day != NULL) {
        *out_day = 0u;
    }
    return 0;
}

#include "../src/military_service/military_service_date.inc"
#include "../src/military_service/military_service_parse.inc"
#include "../src/military_service/seed/military_seed_line_parse.inc"
#include "../src/allstar/allstar_csv_parse.inc"
#include "../src/foreign/foreign_waiver_date.inc"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
#include "../src/foreign/replacement_seed/foreign_replacement_seed_parse.inc"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void test_core_text_and_sql_helpers(void)
{
    char date[9] = {0};
    char escaped[32] = {0};

    assert(ascii_equals_ignore_case("KBO", "kbo"));
    assert(!ascii_equals_ignore_case("KBO", "KB"));
    assert(!ascii_equals_ignore_case(NULL, "kbo"));

    assert(kbo_format_history_date(date, sizeof(date), 2024u, 2u, 29u));
    assert(strcmp(date, "20240229") == 0);
    assert(!kbo_format_history_date(date, sizeof(date), 2023u, 2u, 29u));
    assert(!kbo_format_history_date(date, 8u, 2024u, 1u, 1u));
    assert(!kbo_format_history_date(NULL, sizeof(date), 2024u, 1u, 1u));
    assert(!kbo_format_history_date(date, sizeof(date), 1799u, 12u, 31u));

    assert(kbo_sql_escape_literal(escaped, sizeof(escaped), "Kim's\tLine\nDrop"));
    assert(strcmp(escaped, "Kim''s\tLineDrop") == 0);
    assert(kbo_sql_escape_literal(escaped, sizeof(escaped), NULL));
    assert(strcmp(escaped, "") == 0);
    strcpy(escaped, "untouched");
    assert(!kbo_sql_escape_literal(escaped, 5u, "abcdef"));
    assert(strcmp(escaped, "") == 0);
    assert(!kbo_sql_escape_literal(NULL, sizeof(escaped), "x"));
    printf("test_core_text_and_sql_helpers: PASS\n");
}

static void test_date_serial(void)
{
    assert(kbo_date_serial(2024u, 3u, 16u) == kbo_date_serial(2024u, 3u, 15u) + 1u);
    assert(kbo_date_serial(2024u, 1u, 1u) == kbo_date_serial(2023u, 12u, 31u) + 1u);
    assert(kbo_date_serial(2000u, 2u, 29u) != 0u);
    assert(kbo_date_serial(1900u, 2u, 29u) == 0u);
    assert(kbo_date_serial(2024u, 4u, 31u) == 0u);
    assert(kbo_date_serial(0u, 1u, 1u) == 0u);
    assert(kbo_date_serial(2024u, 13u, 1u) == 0u);
    assert(kbo_current_date_serial() == 0u);
    printf("test_date_serial: PASS\n");
}

static void test_foreign_waiver_date_helpers(void)
{
    uint32_t parsed = 0u;

    assert(kbo_days_in_month(2024u, 2u) == 29);
    assert(kbo_days_in_month(2023u, 2u) == 28);
    assert(kbo_days_in_month(2024u, 0u) == 0);
    assert(kbo_add_one_month_yyyymmdd(20240131u) == 20240229u);
    assert(kbo_add_one_month_yyyymmdd(20230228u) == 20230328u);
    assert(kbo_add_one_month_yyyymmdd(20231231u) == 20240131u);
    assert(kbo_add_one_month_yyyymmdd(19791231u) == 0u);
    assert(kbo_add_days_yyyymmdd(20240227u, 2u) == 20240229u);
    assert(kbo_add_days_yyyymmdd(20240228u, 2u) == 20240301u);
    assert(kbo_add_days_yyyymmdd(20241231u, 1u) == 20250101u);
    assert(kbo_add_years_yyyymmdd(20200229u, 1u) == 20210228u);
    assert(kbo_add_years_yyyymmdd(21991231u, 10u) == 22001231u);
    assert(kbo_add_years_yyyymmdd(22010101u, 1u) == 0u);

    assert(kbo_parse_yyyymmdd("20240229", &parsed) && parsed == 20240229u);
    assert(kbo_parse_yyyymmdd("2024-02-29", &parsed) && parsed == 20240229u);
    assert(kbo_parse_yyyymmdd("20240229 extra", &parsed) && parsed == 20240229u);
    assert(!kbo_parse_yyyymmdd("2024/02/29", &parsed));
    assert(!kbo_parse_yyyymmdd("2024-2-29", &parsed));
    assert(!kbo_parse_yyyymmdd(NULL, &parsed));
    assert(!kbo_parse_yyyymmdd("20240229", NULL));
    assert(!kbo_get_foreign_waiver_current_yyyymmdd(&parsed));
    printf("test_foreign_waiver_date_helpers: PASS\n");
}

static void test_military_csv_parse(void)
{
    char token[] = "  \" player-01 \"\r\n";
    uint32_t value = 0u;

    kbo_military_trim_csv_token_in_place(token);
    assert(strcmp(token, "player-01") == 0);
    assert(kbo_military_ascii_is_seed_id_char('A'));
    assert(kbo_military_ascii_is_seed_id_char('_'));
    assert(kbo_military_ascii_is_seed_id_char('-'));
    assert(!kbo_military_ascii_is_seed_id_char('.'));

    assert(kbo_military_parse_u32_full_token(" 4294967295", &value));
    assert(value == 0xffffffffu);
    assert(!kbo_military_parse_u32_full_token("4294967296", &value));
    assert(!kbo_military_parse_u32_full_token("12x", &value));
    assert(kbo_military_parse_u32_full_token(" 12", &value));
    assert(value == 12u);
    assert(!kbo_military_parse_u32_full_token("", &value));

    assert(kbo_military_parse_yyyymmdd("2024-02-29") == 20240229u);
    assert(kbo_military_parse_yyyymmdd("report:20240229") == 20240229u);
    assert(kbo_military_parse_yyyymmdd("2023-02-29") == 0u);
    assert(kbo_military_parse_yyyymmdd("2024-04-31") == 0u);
    assert(kbo_military_parse_yyyymmdd("202402") == 0u);
    printf("test_military_csv_parse: PASS\n");
}

static void test_military_date_round_trip(void)
{
    uint32_t leap_serial = kbo_military_yyyymmdd_to_serial(20240229u);
    uint32_t today = kbo_military_yyyymmdd_to_serial(20240101u);

    assert(leap_serial != 0u);
    assert(kbo_military_serial_to_yyyymmdd(leap_serial) == 20240229u);
    assert(kbo_military_yyyymmdd_to_serial(20230229u) == 0u);
    assert(kbo_military_days_in_month(2024u, 2u) == 29u);
    assert(kbo_military_days_in_month(2023u, 2u) == 28u);
    assert(kbo_military_days_in_month(2023u, 13u) == 0u);

    assert(kbo_military_yyyymmdd_add_days(20240228u, 1) == 20240229u);
    assert(kbo_military_yyyymmdd_add_days(20240228u, 2) == 20240301u);
    assert(kbo_military_yyyymmdd_add_days(20241231u, 1) == 20250101u);
    assert(kbo_military_yyyymmdd_add_days(20240229u, -1) == 0u);

    assert(kbo_military_days_left_from_return_serial(today + 10u, today) == 10);
    assert(kbo_military_days_left_from_return_serial(today, today) == 0);
    assert(kbo_military_days_left_from_return_serial(today + 40000u, today) == 32767);
    printf("test_military_date_round_trip: PASS\n");
}

static void test_military_seed_line_parse(void)
{
    KboMilitaryServiceSeed seed;

    assert(!kbo_parse_military_service_seed_line(NULL, &seed));
    assert(!kbo_parse_military_service_seed_line("# comment", &seed));
    assert(!kbo_parse_military_service_seed_line("source_key,service,original,return,player", &seed));

    assert(kbo_parse_military_service_seed_line("player-A, SANG, LG, 2025-03-01, 12345", &seed));
    assert(strcmp(seed.key, "player-A") == 0);
    assert(seed.player_id == 12345u);
    assert(strcmp(seed.service_team_code, "SANG") == 0);
    assert(strcmp(seed.original_team_code, "LG") == 0);
    assert(seed.service_return_yyyymmdd == 20250301u);
    assert(seed.service_total_days == KBO_MILITARY_SERVICE_DAYS);

    assert(kbo_parse_military_service_seed_line("  \"numeric-77\" , KPB , KT , 2024/02/29 , 67890  ", &seed));
    assert(strcmp(seed.key, "numeric-77") == 0);
    assert(seed.player_id == 67890u);
    assert(seed.service_return_yyyymmdd == 20240229u);

    assert(kbo_parse_military_service_seed_line("legacy, SANG, DOO, 2024-01-01, 30, 24680", &seed));
    assert(seed.player_id == 24680u);
    assert(seed.service_start_yyyymmdd == 20240101u);
    assert(seed.service_total_days == 30);
    assert(seed.service_return_yyyymmdd == 20240131u);

    assert(kbo_parse_military_service_seed_line("98765, SANG, HANWHA, 2025-12-31", &seed));
    assert(seed.player_id == 98765u);
    assert(strcmp(seed.key, "98765") == 0);
    assert(seed.service_return_yyyymmdd == 20251231u);
    printf("test_military_seed_line_parse: PASS\n");
}

static void test_allstar_csv_parse(void)
{
    assert(kbo_parse_allstar_side("Nanum") == 1u);
    assert(kbo_parse_allstar_side("Dream") == 2u);
    assert(kbo_parse_allstar_side("North") == 1u);
    assert(kbo_parse_allstar_side("South") == 2u);
    assert(kbo_parse_allstar_side("Western") == 1u);
    assert(kbo_parse_allstar_side("Eastern") == 2u);
    assert(kbo_parse_allstar_side("unknown") == 0u);

    int year_col = -1;
    int team_id_col = -1;
    int name_col = -1;
    int allstar_col = -1;
    kbo_csv_find_allstar_team_columns(
        "yearID,teamID,name,allstar_team",
        &year_col,
        &team_id_col,
        &name_col,
        &allstar_col);
    assert(year_col == 0);
    assert(team_id_col == 1);
    assert(name_col == 2);
    assert(allstar_col == 3);

    uint16_t year = 0u;
    char team_id[16] = {0};
    char team_name[96] = {0};
    char current_city[64] = {0};
    uint8_t side = 0u;
    kbo_csv_extract_allstar_team_fields(
        "2026,LG,LG Twins,Nanum",
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        &year,
        team_id,
        sizeof(team_id),
        team_name,
        sizeof(team_name),
        current_city,
        sizeof(current_city),
        &side);
    assert(year == 2026u);
    assert(strcmp(team_id, "LG") == 0);
    assert(strcmp(team_name, "LG Twins") == 0);
    assert(strcmp(current_city, "LG") == 0);
    assert(side == 1u);

    kbo_csv_find_allstar_team_columns(
        "\"year\",\"team_id\",\"name\",\"all_star_division\"",
        &year_col,
        &team_id_col,
        &name_col,
        &allstar_col);
    assert(year_col == 0);
    assert(team_id_col == 1);
    assert(name_col == 2);
    assert(allstar_col == 3);

    kbo_csv_extract_allstar_team_fields(
        "2027,\"SSG\",\"SSG Landers, Incheon\",\"dong-gun\"",
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        &year,
        team_id,
        sizeof(team_id),
        team_name,
        sizeof(team_name),
        current_city,
        sizeof(current_city),
        &side);
    assert(year == 2027u);
    assert(strcmp(team_id, "SSG") == 0);
    assert(strcmp(team_name, "SSG Landers, Incheon") == 0);
    assert(strcmp(current_city, "SSG") == 0);
    assert(side == 2u);

    kbo_csv_extract_allstar_team_fields(
        "1700,KIWOOM,Kiwoom Heroes,unknown",
        year_col,
        team_id_col,
        name_col,
        allstar_col,
        &year,
        team_id,
        sizeof(team_id),
        team_name,
        sizeof(team_name),
        current_city,
        sizeof(current_city),
        &side);
    assert(year == 0u);
    assert(side == 0u);
    printf("test_allstar_csv_parse: PASS\n");
}

static void test_foreign_replacement_seed_parse(void)
{
    KboForeignReplacementPlayerSeed seed;
    char token[] = " \t\"seed-key\"\r\n";

    kbo_trim_csv_token_in_place(token);
    assert(strcmp(token, "seed-key") == 0);
    assert(kbo_ascii_is_seed_id_char('Z'));
    assert(kbo_ascii_is_seed_id_char('7'));
    assert(kbo_ascii_is_seed_id_char('_'));
    assert(!kbo_ascii_is_seed_id_char('.'));

    assert(kbo_parse_foreign_replacement_seed_slot_type("asian") == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA);
    assert(kbo_parse_foreign_replacement_seed_slot_type("2") == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA);
    assert(kbo_parse_foreign_replacement_seed_slot_type("foreign") == KBO_FOREIGN_INJURY_SLOT_REGULAR);
    assert(kbo_parse_foreign_replacement_seed_slot_type("regular") == KBO_FOREIGN_INJURY_SLOT_REGULAR);
    assert(kbo_parse_foreign_replacement_seed_slot_type("bogus") == 0u);

    assert(!kbo_parse_foreign_replacement_player_seed_line(NULL, &seed));
    assert(!kbo_parse_foreign_replacement_player_seed_line("; comment", &seed));
    assert(!kbo_parse_foreign_replacement_player_seed_line("player_id,slot_type", &seed));

    assert(kbo_parse_foreign_replacement_player_seed_line("  \"slug-01\" , asian_quota ", &seed));
    assert(strcmp(seed.key, "slug-01") == 0);
    assert(seed.player_id == 0u);
    assert(seed.slot_type == KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA);

    assert(kbo_parse_foreign_replacement_player_seed_line("123456, regular, ignored", &seed));
    assert(strcmp(seed.key, "123456") == 0);
    assert(seed.player_id == 123456u);
    assert(seed.slot_type == KBO_FOREIGN_INJURY_SLOT_REGULAR);

    assert(kbo_parse_foreign_replacement_player_seed_line("abc, unknown", &seed));
    assert(strcmp(seed.key, "abc") == 0);
    assert(seed.slot_type == 0u);
    printf("test_foreign_replacement_seed_parse: PASS\n");
}

int main(void)
{
    test_core_text_and_sql_helpers();
    test_date_serial();
    test_foreign_waiver_date_helpers();
    test_military_csv_parse();
    test_military_date_round_trip();
    test_military_seed_line_parse();
    test_allstar_csv_parse();
    test_foreign_replacement_seed_parse();
    printf("All tests passed.\n");
    return 0;
}
