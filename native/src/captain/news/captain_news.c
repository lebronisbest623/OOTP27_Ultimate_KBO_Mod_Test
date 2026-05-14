#include "../internal/captain_selection_internal.h"

#include "captain_news_markers.h"

#include <stdint.h>

#include "../../core/news/live/core_live_news.h"
#include "../../core/news/templates/core_news_templates.h"

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
                && kbo_captain_news_should_strip_team_suffix(
                    raw,
                    first_len,
                    raw + suffix_start,
                    len - suffix_start)) {
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

static int kbo_captain_news_render_key(
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

int kbo_emit_captain_initial_selection_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* rows,
    int row_count,
    const char* source)
{
    if (date == 0u || season < 1982u || season > 2200u || league_id == 0u || rows == NULL || row_count <= 0) {
        return 0;
    }

    char marker[128] = {0};
    snprintf(marker, sizeof(marker), "summary|%u|%u", season, league_id);
    if (kbo_captain_news_marker_exists(marker)) {
        return 0;
    }

    char template_path[MAX_PATH] = {0};
    char title[160] = {0};
    char intro[4096] = {0};
    char outro[4096] = {0};
    if (!kbo_captain_news_render_key(
                "captain.summary.title",
                season,
                NULL,
                NULL,
                title,
                sizeof(title),
                template_path,
                sizeof(template_path),
                source)
            || !kbo_captain_news_render_key(
                "captain.summary.intro",
                season,
                NULL,
                NULL,
                intro,
                sizeof(intro),
                NULL,
                0u,
                source)
            || !kbo_captain_news_render_key(
                "captain.summary.outro",
                season,
                NULL,
                NULL,
                outro,
                sizeof(outro),
                NULL,
                0u,
                source)) {
        append_logf(
            "KBO captain summary news skipped source=%s season=%u league_id=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }

    int listed = 0;
    char body[8192] = {0};
    char rendered[4096] = {0};
    if (intro[0] != '\0') {
        kbo_news_text_append(body, sizeof(body), intro);
        kbo_news_text_appendf(body, sizeof(body), "\n\n");
    }
    for (int i = 0; i < row_count; i++) {
        if (rows[i].team_id == 0u || rows[i].player_id == 0u) {
            continue;
        }
        if (!kbo_captain_news_render_key(
                "captain.summary.line",
                season,
                &rows[i],
                NULL,
                rendered,
                sizeof(rendered),
                NULL,
                0u,
                source)) {
            append_logf(
                "KBO captain summary news skipped source=%s season=%u league_id=%u reason=line_template_unavailable",
                source != NULL ? source : "",
                season,
                league_id);
            return 0;
        }
        kbo_news_text_append(body, sizeof(body), rendered);
        if (!kbo_news_text_ends_with_newline(rendered)) {
            kbo_news_text_appendf(body, sizeof(body), "\n");
        }
        listed++;
    }
    if (listed <= 0) {
        append_logf(
            "KBO captain summary news skipped source=%s season=%u league_id=%u reason=no_listed_captains",
            source != NULL ? source : "",
            season,
            league_id);
        return 0;
    }
    if (outro[0] != '\0') {
        if (!kbo_news_text_ends_with_newline(body)) {
            kbo_news_text_appendf(body, sizeof(body), "\n");
        }
        kbo_news_text_appendf(body, sizeof(body), "\n");
        kbo_news_text_append(body, sizeof(body), outro);
    }

    int created = create_kbo_native_live_news_with_body_live_required(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_captain_news_persist_marker(marker, source);
    }
    append_logf(
        "KBO captain summary news source=%s date=%u season=%u league_id=%u listed=%d created=%d template=%s",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        listed,
        created,
        template_path);
    return created;
}

int kbo_emit_captain_replacement_news(
    uint32_t date,
    uint32_t season,
    uint32_t league_id,
    const KboCaptainSelectionRow* old_row,
    const KboCaptainSelectionRow* new_row,
    const char* source)
{
    if (date == 0u || season < 1982u || season > 2200u || league_id == 0u
            || new_row == NULL || new_row->team_id == 0u || new_row->player_id == 0u) {
        return 0;
    }

    uint32_t old_player_id = old_row != NULL ? old_row->player_id : 0u;
    char marker[160] = {0};
    snprintf(
        marker,
        sizeof(marker),
        "replacement|%u|%u|%u|%u|%u",
        season,
        league_id,
        new_row->team_id,
        old_player_id,
        new_row->player_id);
    if (kbo_captain_news_marker_exists(marker)) {
        return 0;
    }

    char template_path[MAX_PATH] = {0};
    char title[180] = {0};
    char body[2048] = {0};
    const char* body_key = old_row != NULL && old_row->player_id != 0u
        ? "captain.replacement.body_departed"
        : "captain.replacement.body_vacant";
    if (!kbo_captain_news_render_key(
                "captain.replacement.title",
                season,
                new_row,
                old_row,
                title,
                sizeof(title),
                template_path,
                sizeof(template_path),
                source)
            || !kbo_captain_news_render_key(
                body_key,
                season,
                new_row,
                old_row,
                body,
                sizeof(body),
                NULL,
                0u,
                source)) {
        append_logf(
            "KBO captain replacement news skipped source=%s season=%u league_id=%u team=%u old_player=%u new_player=%u reason=templates_unavailable",
            source != NULL ? source : "",
            season,
            league_id,
            new_row->team_id,
            old_player_id,
            new_row->player_id);
        return 0;
    }

    int created = create_kbo_native_live_news_with_body(
        date / 10000u,
        (date / 100u) % 100u,
        date % 100u,
        league_id,
        10u,
        title,
        body);
    if (created) {
        kbo_captain_news_persist_marker(marker, source);
    }
    append_logf(
        "KBO captain replacement news source=%s date=%u season=%u league_id=%u team=%u old_player=%u new_player=%u created=%d template=%s",
        source != NULL ? source : "",
        date,
        season,
        league_id,
        new_row->team_id,
        old_player_id,
        new_row->player_id,
        created,
        template_path);
    return created;
}
