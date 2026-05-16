#include "../internal/player_team_seasons_internal.h"

static int kbo_player_team_seasons_ascii_lower(int ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
}

static int kbo_player_team_seasons_ascii_contains_ignore_case(const char* text, const char* needle)
{
    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return 0;
    }
    size_t needle_len = strlen(needle);
    for (const char* p = text; *p != '\0'; p++) {
        size_t i = 0;
        for (; i < needle_len; i++) {
            if (p[i] == '\0'
                    || kbo_player_team_seasons_ascii_lower((unsigned char)p[i])
                        != kbo_player_team_seasons_ascii_lower((unsigned char)needle[i])) {
                break;
            }
        }
        if (i == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int kbo_player_team_seasons_name_contains(const char* text, const char* marker)
{
    if (text == NULL || marker == NULL || marker[0] == '\0') {
        return 0;
    }
    return kbo_player_team_seasons_ascii_contains_ignore_case(text, marker) || strstr(text, marker) != NULL;
}

static void kbo_player_team_seasons_copy_team_name_by_id(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team == NULL || !memory_range_readable(team, OOTP27_KBO_TEAM_READABLE_BYTES)) {
        return;
    }

    char city[64] = {0};
    char nickname[64] = {0};
    char full_name[96] = {0};
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_CITY_STRING_OFFSET, city, sizeof(city));
    copy_ootp_string_object_text(team, OOTP27_KBO_TEAM_NICKNAME_STRING_OFFSET, nickname, sizeof(nickname));
    copy_ootp_string_object_text(team, 0x40u, full_name, sizeof(full_name));

    if (full_name[0] != '\0') {
        snprintf(out, out_size, "%s", full_name);
    } else if (city[0] != '\0' && nickname[0] != '\0' && _stricmp(city, nickname) != 0) {
        snprintf(out, out_size, "%s %s", city, nickname);
    } else if (nickname[0] != '\0') {
        snprintf(out, out_size, "%s", nickname);
    } else if (city[0] != '\0') {
        snprintf(out, out_size, "%s", city);
    }
}

void kbo_player_team_seasons_copy_team_seed_code(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    uint32_t parent_team_id = 0u;
    uint8_t* team = find_kbo_team_by_numeric_id_any_league(team_id, 1);
    if (team != NULL && memory_range_readable(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET, sizeof(uint32_t))) {
        parent_team_id = *(uint32_t*)(team + OOTP27_KBO_TEAM_PARENT_TEAM_ID_OFFSET);
    }
    if (parent_team_id != 0u && parent_team_id != team_id) {
        kbo_player_team_seasons_copy_team_seed_code(parent_team_id, out, out_size);
        return;
    }

    char name[96] = {0};
    kbo_player_team_seasons_copy_team_name_by_id(team_id, name, sizeof(name));
    if (kbo_player_team_seasons_name_contains(name, "Lotte")
            || kbo_player_team_seasons_name_contains(name, "Giants")
            || kbo_player_team_seasons_name_contains(name, "\xeb\xa1\xaf\xeb\x8d\xb0")
            || kbo_player_team_seasons_name_contains(name, "\xec\x9e\x90\xec\x9d\xb4\xec\x96\xb8\xec\xb8\xa0")) {
        snprintf(out, out_size, "LOT");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Doosan")
            || kbo_player_team_seasons_name_contains(name, "Bears")
            || kbo_player_team_seasons_name_contains(name, "\xeb\x91\x90\xec\x82\xb0")
            || kbo_player_team_seasons_name_contains(name, "\xeb\xb2\xa0\xec\x96\xb4\xec\x8a\xa4")) {
        snprintf(out, out_size, "DOO");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Samsung")
            || kbo_player_team_seasons_name_contains(name, "Lions")
            || kbo_player_team_seasons_name_contains(name, "\xec\x82\xbc\xec\x84\xb1")
            || kbo_player_team_seasons_name_contains(name, "\xeb\x9d\xbc\xec\x9d\xb4\xec\x98\xa8\xec\xa6\x88")) {
        snprintf(out, out_size, "SAM");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "KIA")
            || kbo_player_team_seasons_name_contains(name, "Tigers")
            || kbo_player_team_seasons_name_contains(name, "\xea\xb8\xb0\xec\x95\x84")
            || kbo_player_team_seasons_name_contains(name, "\xed\x83\x80\xec\x9d\xb4\xea\xb1\xb0\xec\xa6\x88")) {
        snprintf(out, out_size, "KIA");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "SSG")
            || kbo_player_team_seasons_name_contains(name, "Landers")
            || kbo_player_team_seasons_name_contains(name, "\xeb\x9e\x9c\xeb\x8d\x94\xec\x8a\xa4")) {
        snprintf(out, out_size, "SSG");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Hanwha")
            || kbo_player_team_seasons_name_contains(name, "Eagles")
            || kbo_player_team_seasons_name_contains(name, "\xed\x95\x9c\xed\x99\x94")
            || kbo_player_team_seasons_name_contains(name, "\xec\x9d\xb4\xea\xb8\x80\xec\x8a\xa4")) {
        snprintf(out, out_size, "HH");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Nexen")
            || kbo_player_team_seasons_name_contains(name, "\xeb\x84\xa5\xec\x84\xbc")) {
        snprintf(out, out_size, "NEX");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Kiwoom")
            || kbo_player_team_seasons_name_contains(name, "Heroes")
            || kbo_player_team_seasons_name_contains(name, "\xed\x82\xa4\xec\x9b\x80")
            || kbo_player_team_seasons_name_contains(name, "\xed\x9e\x88\xec\x96\xb4\xeb\xa1\x9c\xec\xa6\x88")) {
        snprintf(out, out_size, "KIW");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Dinos")
            || kbo_player_team_seasons_name_contains(name, "NC")
            || kbo_player_team_seasons_name_contains(name, "\xec\x97\x94\xec\x94\xa8")
            || kbo_player_team_seasons_name_contains(name, "\xeb\x8b\xa4\xec\x9d\xb4\xeb\x85\xb8\xec\x8a\xa4")) {
        snprintf(out, out_size, "NC");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Wiz")
            || kbo_player_team_seasons_name_contains(name, "KT")
            || kbo_player_team_seasons_name_contains(name, "\xec\xbc\x80\xec\x9d\xb4\xed\x8b\xb0")
            || kbo_player_team_seasons_name_contains(name, "\xec\x9c\x84\xec\xa6\x88")) {
        snprintf(out, out_size, "kt");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Twins")
            || kbo_player_team_seasons_name_contains(name, "LG")
            || kbo_player_team_seasons_name_contains(name, "\xec\x97\x98\xec\xa7\x80")
            || kbo_player_team_seasons_name_contains(name, "\xed\x8a\xb8\xec\x9c\x88\xec\x8a\xa4")) {
        snprintf(out, out_size, "LG");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Sangmu")
            || kbo_player_team_seasons_name_contains(name, "\xec\x83\x81\xeb\xac\xb4")) {
        snprintf(out, out_size, "SANG");
        return;
    }
    if (kbo_player_team_seasons_name_contains(name, "Police")
            || kbo_player_team_seasons_name_contains(name, "\xea\xb2\xbd\xec\xb0\xb0")) {
        snprintf(out, out_size, "KPB");
        return;
    }
}
