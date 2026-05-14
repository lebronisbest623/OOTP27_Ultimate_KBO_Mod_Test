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
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Lotte"))   { snprintf(out, out_size, "LOT"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Doosan"))  { snprintf(out, out_size, "DOO"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Samsung")) { snprintf(out, out_size, "SAM"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "KIA"))     { snprintf(out, out_size, "KIA"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "SSG"))     { snprintf(out, out_size, "SSG"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Hanwha"))  { snprintf(out, out_size, "HH");  return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Kiwoom"))  { snprintf(out, out_size, "KIW"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Nexen"))   { snprintf(out, out_size, "NEX"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Heroes"))  { snprintf(out, out_size, "KIW"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Dinos"))   { snprintf(out, out_size, "NC");  return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "NC"))      { snprintf(out, out_size, "NC");  return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Wiz"))     { snprintf(out, out_size, "kt");  return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "KT"))      { snprintf(out, out_size, "kt");  return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Twins"))   { snprintf(out, out_size, "LG");  return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "LG"))      { snprintf(out, out_size, "LG");  return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Sangmu"))  { snprintf(out, out_size, "SANG"); return; }
    if (kbo_player_team_seasons_ascii_contains_ignore_case(name, "Police"))  { snprintf(out, out_size, "KPB"); return; }
}
