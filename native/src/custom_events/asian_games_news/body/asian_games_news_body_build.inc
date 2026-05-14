void kbo_build_asian_games_news_body(char* out, size_t out_size, const char* template_prefix, const char* lead, const char* source)
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
    int is_selection = template_prefix != NULL && strcmp(template_prefix, "asian_games.selection") == 0;
    int is_departure = template_prefix != NULL && strcmp(template_prefix, "asian_games.departure") == 0;
    int is_final_gold = template_prefix != NULL && strcmp(template_prefix, "asian_games.final") == 0;
    int is_final_failure = template_prefix != NULL && strcmp(template_prefix, "asian_games.final.failure") == 0;
    int is_final = is_final_gold || is_final_failure;
    int include_roster = is_selection;
    int include_samples = 0;

    kbo_news_text_append(out, out_size, lead != NULL && lead[0] != '\0' ? lead : "");
    if (out[0] != '\0') {
        kbo_news_text_append(out, out_size, " ");
    }

    size_t used = strlen(out);
    (void)used;

    char roster_count_text[16] = {0};
    char wildcards_text[16] = {0};
    char departed_text[16] = {0};
    char returned_text[16] = {0};
    char exempted_text[16] = {0};
    char roster_year_text[16] = {0};
    kbo_asian_games_u32_text((uint32_t)roster_count, roster_count_text, sizeof(roster_count_text));
    kbo_asian_games_u32_text((uint32_t)wildcards, wildcards_text, sizeof(wildcards_text));
    kbo_asian_games_u32_text((uint32_t)departed, departed_text, sizeof(departed_text));
    kbo_asian_games_u32_text((uint32_t)returned, returned_text, sizeof(returned_text));
    kbo_asian_games_u32_text((uint32_t)exempted, exempted_text, sizeof(exempted_text));
    kbo_asian_games_u32_text(g_kbo_asian_games_roster_year, roster_year_text, sizeof(roster_year_text));

    KboNewsTemplateVar body_vars[] = {
        { "roster_count", roster_count_text },
        { "wildcards", wildcards_text },
        { "wildcards_plural", kbo_asian_games_plural(wildcards) },
        { "departed", departed_text },
        { "departed_plural", kbo_asian_games_plural(departed) },
        { "returned", returned_text },
        { "returned_plural", kbo_asian_games_plural(returned) },
        { "exempted", exempted_text },
        { "exempted_plural", kbo_asian_games_plural(exempted) },
        { "roster_year", roster_year_text },
    };
    const char* body_key = is_selection
        ? "asian_games.selection.body"
        : (is_departure
            ? "asian_games.departure.body"
            : (is_final_failure
                ? "asian_games.final.failure.body"
                : (is_final ? "asian_games.final.body" : "asian_games.default.body")));
    char rendered[2048] = {0};
    if (!kbo_news_template_render_key(
            body_key,
            body_vars,
            (int)(sizeof(body_vars) / sizeof(body_vars[0])),
            rendered,
            sizeof(rendered),
            source)) {
        out[0] = '\0';
        return;
    }
    kbo_news_text_append(out, out_size, rendered);
    used = strlen(out);

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
        KboNewsTemplateVar suffix_vars[] = {
            { "period", include_samples ? "." : "" },
        };
        char suffix[512] = {0};
        if (kbo_news_template_render_key(
                "asian_games.samples_suffix",
                suffix_vars,
                (int)(sizeof(suffix_vars) / sizeof(suffix_vars[0])),
                suffix,
                sizeof(suffix),
                source)) {
            kbo_news_text_append(out, out_size, suffix);
            used = strlen(out);
        }
    }

    if (include_roster && captain_link[0] != '\0' && used < out_size - 1) {
        KboNewsTemplateVar quote_vars[] = {
            { "captain_link", captain_link },
        };
        char quote[768] = {0};
        if (kbo_news_template_render_key(
                "asian_games.captain_quote",
                quote_vars,
                (int)(sizeof(quote_vars) / sizeof(quote_vars[0])),
                quote,
                sizeof(quote),
                source)) {
            kbo_news_text_append(out, out_size, quote);
            used = strlen(out);
        }
    }

    if (include_roster && used < out_size - 1) {
        char header[128] = {0};
        if (kbo_news_template_load("asian_games.complete_roster_header", header, sizeof(header), NULL, 0u, source)) {
            kbo_news_text_append(out, out_size, header);
            used = strlen(out);
        }
    }

    if (include_roster) {
        for (int group = 0; group < 5 && used + 140u < out_size; group++) {
            int group_count = 0;
            for (LONG i = 0; i < roster_count; i++) {
                if (kbo_asian_games_role_group_index(g_kbo_asian_games_roster[i].role) == group) {
                    group_count++;
                }
            }
            if (group_count <= 0) {
                continue;
            }
            if (!kbo_asian_games_append_roster_group_header(out, out_size, &used, group)) {
                continue;
            }
            LONG listed_in_group = 0;
            for (LONG i = 0; i < roster_count && used + 140u < out_size; i++) {
                if (kbo_asian_games_role_group_index(g_kbo_asian_games_roster[i].role) != group) {
                    continue;
                }
                kbo_asian_games_append_roster_line(
                    out,
                    out_size,
                    &used,
                    listed_in_group,
                    &g_kbo_asian_games_roster[i]);
                listed_in_group++;
            }
        }
    }
}
