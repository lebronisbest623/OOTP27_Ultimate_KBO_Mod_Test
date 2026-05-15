#include "core_news_links.h"

#include <string.h>

void kbo_news_related_ids_init(KboNewsRelatedIds* out)
{
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
}

static int kbo_news_related_ids_contains(const uint32_t* ids, int count, uint32_t id)
{
    if (ids == NULL || id == 0u) {
        return 1;
    }
    for (int i = 0; i < count; i++) {
        if (ids[i] == id) {
            return 1;
        }
    }
    return 0;
}

static void kbo_news_related_ids_add_player(KboNewsRelatedIds* out, uint32_t id)
{
    if (out == NULL || id == 0u || out->player_count >= KBO_NEWS_RELATED_PLAYER_MAX
            || kbo_news_related_ids_contains(out->player_ids, out->player_count, id)) {
        return;
    }
    out->player_ids[out->player_count++] = id;
}

static void kbo_news_related_ids_add_team(KboNewsRelatedIds* out, uint32_t id)
{
    if (out == NULL || id == 0u || out->team_count >= KBO_NEWS_RELATED_TEAM_MAX
            || kbo_news_related_ids_contains(out->team_ids, out->team_count, id)) {
        return;
    }
    out->team_ids[out->team_count++] = id;
}

static uint32_t kbo_news_parse_link_id(const char* p)
{
    if (p == NULL) {
        return 0u;
    }
    uint32_t value = 0u;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        uint32_t digit = (uint32_t)(*p - '0');
        if (value > 429496729u || (value == 429496729u && digit > 5u)) {
            return 0u;
        }
        value = value * 10u + digit;
        digits++;
        p++;
    }
    if (digits == 0 || *p != '>') {
        return 0u;
    }
    return value;
}

void kbo_news_related_ids_collect(KboNewsRelatedIds* out, const char* text)
{
    if (out == NULL || text == NULL || text[0] == '\0') {
        return;
    }

    const char* p = text;
    while (*p != '\0') {
        const char* player = strstr(p, ":player#");
        const char* team = strstr(p, ":team#");
        if (player == NULL && team == NULL) {
            break;
        }
        if (player != NULL && (team == NULL || player < team)) {
            kbo_news_related_ids_add_player(out, kbo_news_parse_link_id(player + 8));
            p = player + 8;
        } else {
            kbo_news_related_ids_add_team(out, kbo_news_parse_link_id(team + 6));
            p = team + 6;
        }
    }
}

void kbo_news_related_ids_collect_pair(KboNewsRelatedIds* out, const char* title, const char* body)
{
    kbo_news_related_ids_init(out);
    kbo_news_related_ids_collect(out, title);
    kbo_news_related_ids_collect(out, body);
}
