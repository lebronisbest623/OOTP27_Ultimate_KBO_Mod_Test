#include "../../internal/captain_selection_internal.h"

#include "captain_news_render.h"

#include "../../../core/news/templates/core_news_templates.h"

static const char* kbo_captain_news_player_name(const KboCaptainSelectionRow* row)
{
    if (row != NULL && row->player_name[0] != '\0') {
        return row->player_name;
    }
    return "the new captain";
}

static const char* kbo_captain_news_team_name(const KboCaptainSelectionRow* row)
{
    if (row != NULL && row->team_name[0] != '\0') {
        return row->team_name;
    }
    return "Team";
}

static char kbo_captain_news_ascii_lower(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
}

static int kbo_captain_news_ascii_equal_ignore_case(const char* a, const char* b, size_t len)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    for (size_t i = 0u; i < len; i++) {
        if (kbo_captain_news_ascii_lower(a[i]) != kbo_captain_news_ascii_lower(b[i])) {
            return 0;
        }
    }
    return 1;
}

static int kbo_captain_news_should_strip_team_suffix(
    const char* text,
    size_t first_len,
    const char* suffix,
    size_t suffix_len)
{
    if (text == NULL || suffix == NULL || first_len == 0u || suffix_len < 2u || suffix_len > 4u) {
        return 0;
    }
    if (first_len == suffix_len && kbo_captain_news_ascii_equal_ignore_case(text, suffix, suffix_len)) {
        return 1;
    }
    if (first_len >= suffix_len && kbo_captain_news_ascii_equal_ignore_case(text, suffix, suffix_len)) {
        return 1;
    }
    char first = kbo_captain_news_ascii_lower(text[0]);
    for (size_t i = 0u; i < suffix_len; i++) {
        if (kbo_captain_news_ascii_lower(suffix[i]) != first) {
            return 0;
        }
    }
    return 1;
}

static void kbo_captain_news_copy_display_team_name(const KboCaptainSelectionRow* row, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    const char* raw = kbo_captain_news_team_name(row);
    size_t len = strlen(raw);
    while (len > 0u && (raw[len - 1u] == ' ' || raw[len - 1u] == '\t')) {
        len--;
    }

    size_t split = len;
    while (split > 0u && raw[split - 1u] != ' ' && raw[split - 1u] != '\t') {
        split--;
    }
    if (split > 0u) {
        size_t first_len = 0u;
        while (first_len < len && raw[first_len] != ' ' && raw[first_len] != '\t') {
            first_len++;
        }

        size_t suffix_start = split;
        while (suffix_start < len && (raw[suffix_start] == ' ' || raw[suffix_start] == '\t')) {
            suffix_start++;
        }
        if (suffix_start < len
                && kbo_captain_news_should_strip_team_suffix(raw, first_len, raw + suffix_start, len - suffix_start)) {
            len = split;
            while (len > 0u && (raw[len - 1u] == ' ' || raw[len - 1u] == '\t')) {
                len--;
            }
        }
    }

    if (len == 0u) {
        snprintf(out, out_size, "Team");
        return;
    }
    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, raw, len);
    out[len] = '\0';
}

static void kbo_captain_news_append_team_link(
    char* out,
    size_t out_size,
    const KboCaptainSelectionRow* row)
{
    char team_name[128] = {0};
    kbo_captain_news_copy_display_team_name(row, team_name, sizeof(team_name));
    if (row != NULL && row->team_id != 0u) {
        kbo_news_text_appendf(out, out_size, "<%s:team#%u>", team_name, row->team_id);
    } else {
        kbo_news_text_appendf(out, out_size, "%s", team_name);
    }
}

static void kbo_captain_news_append_player_link(
    char* out,
    size_t out_size,
    const KboCaptainSelectionRow* row)
{
    if (row != NULL && row->player_id != 0u) {
        kbo_news_text_appendf(out, out_size, "<%s:player#%u>", kbo_captain_news_player_name(row), row->player_id);
    } else {
        kbo_news_text_appendf(out, out_size, "%s", kbo_captain_news_player_name(row));
    }
}

typedef struct KboCaptainNewsRenderContext {
    char season[16];
    char team_name[128];
    char team_link[192];
    char player_name[128];
    char player_link[192];
    char old_player_name[128];
    char old_player_link[192];
    KboNewsTemplateVar vars[9];
} KboCaptainNewsRenderContext;

static int kbo_captain_news_build_render_context(
    uint32_t season,
    const KboCaptainSelectionRow* row,
    const KboCaptainSelectionRow* old_row,
    KboCaptainNewsRenderContext* ctx)
{
    if (ctx == NULL) {
        return 0;
    }
    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->season, sizeof(ctx->season), "%u", season);
    kbo_captain_news_copy_display_team_name(row, ctx->team_name, sizeof(ctx->team_name));
    snprintf(ctx->player_name, sizeof(ctx->player_name), "%s", kbo_captain_news_player_name(row));
    snprintf(ctx->old_player_name, sizeof(ctx->old_player_name), "%s", kbo_captain_news_player_name(old_row));
    kbo_captain_news_append_team_link(ctx->team_link, sizeof(ctx->team_link), row);
    kbo_captain_news_append_player_link(ctx->player_link, sizeof(ctx->player_link), row);
    kbo_captain_news_append_player_link(ctx->old_player_link, sizeof(ctx->old_player_link), old_row);

    ctx->vars[0] = (KboNewsTemplateVar){ "season", ctx->season };
    ctx->vars[1] = (KboNewsTemplateVar){ "team_name", ctx->team_name };
    ctx->vars[2] = (KboNewsTemplateVar){ "team_link", ctx->team_link };
    ctx->vars[3] = (KboNewsTemplateVar){ "player_name", ctx->player_name };
    ctx->vars[4] = (KboNewsTemplateVar){ "player_link", ctx->player_link };
    ctx->vars[5] = (KboNewsTemplateVar){ "new_player_name", ctx->player_name };
    ctx->vars[6] = (KboNewsTemplateVar){ "new_player_link", ctx->player_link };
    ctx->vars[7] = (KboNewsTemplateVar){ "old_player_name", ctx->old_player_name };
    ctx->vars[8] = (KboNewsTemplateVar){ "old_player_link", ctx->old_player_link };
    return 9;
}

int kbo_captain_news_render_key(
    const char* key,
    uint32_t season,
    const KboCaptainSelectionRow* row,
    const KboCaptainSelectionRow* old_row,
    char* out,
    size_t out_size,
    char* source_path,
    size_t source_path_size,
    const char* source)
{
    KboCaptainNewsRenderContext ctx;
    int var_count = kbo_captain_news_build_render_context(season, row, old_row, &ctx);
    return kbo_news_template_render_key_with_source(
        key,
        ctx.vars,
        var_count,
        out,
        out_size,
        source_path,
        source_path_size,
        source);
}
