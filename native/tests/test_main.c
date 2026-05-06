#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int kbo_is_leap_year(uint32_t year)
{
    return (year % 4u == 0u && year % 100u != 0u) || (year % 400u == 0u);
}

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

static int ascii_equals_ignore_case(const char* a, const char* b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

#include "../src/military_service/military_service_date.inc"
#include "../src/military_service/military_service_parse.inc"
#include "../src/allstar/allstar_csv_parse.inc"

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

    assert(kbo_military_parse_yyyymmdd("2024-02-29") == 20240229u);
    assert(kbo_military_parse_yyyymmdd("2023-02-29") == 0u);
    assert(kbo_military_parse_yyyymmdd("2024-04-31") == 0u);
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
    printf("test_allstar_csv_parse: PASS\n");
}

int main(void)
{
    test_date_serial();
    test_military_csv_parse();
    test_military_date_round_trip();
    test_allstar_csv_parse();
    printf("All tests passed.\n");
    return 0;
}
