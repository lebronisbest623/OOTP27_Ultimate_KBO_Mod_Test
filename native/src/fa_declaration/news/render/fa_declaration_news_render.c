#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "fa_declaration_news_render.h"

#include <stdio.h>
#include <string.h>

#include "../../../bootstrap/abi/ootp_offsets.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/news/templates/core_news_templates.h"
#include "../../../runtime_memory/runtime_memory.h"
#include "../../../team/lookup/team_lookup.h"
#include "../../../team/names/team_string.h"

static int kbo_fa_declaration_news_candidate_listed(
    const uint32_t* used_ids,
    int used_count,
    uint32_t player_id)
{
    if (used_ids == NULL || player_id == 0u) {
        return 0;
    }
    for (int i = 0; i < used_count; i++) {
        if (used_ids[i] == player_id) {
            return 1;
        }
    }
    return 0;
}

static int kbo_fa_declaration_candidate_is_retry_after_down_year(const KboFaDeclarationCandidate* candidate)
{
    return candidate != NULL
        && candidate->declared == 0u
        && strstr(candidate->decision_reason, "retry_after_down_year") != NULL;
}

const KboFaDeclarationCandidate* kbo_fa_declaration_news_find_best_candidate(
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    int retry_filter,
    const uint32_t* used_ids,
    int used_count)
{
    const KboFaDeclarationCandidate* best = NULL;
    for (int i = 0; i < candidate_count; i++) {
        const KboFaDeclarationCandidate* c = &candidates[i];
        if (c->player_id == 0u || (c->declared ? 1 : 0) != declared_filter) {
            continue;
        }
        if (retry_filter >= 0) {
            int is_retry = kbo_fa_declaration_candidate_is_retry_after_down_year(c);
            if ((retry_filter != 0) != (is_retry != 0)) {
                continue;
            }
        }
        if (kbo_fa_declaration_news_candidate_listed(used_ids, used_count, c->player_id)) {
            continue;
        }
        if (best == NULL
                || c->score > best->score
                || (c->score == best->score && c->player_id < best->player_id)) {
            best = c;
        }
    }
    return best;
}

static int kbo_fa_declaration_news_uses_korean(void)
{
    const char* language_dir = kbo_custom_news_language_dir();
    return language_dir == NULL || strcmp(language_dir, "en") != 0;
}

static char kbo_fa_declaration_ascii_lower(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
}

static int kbo_fa_declaration_ascii_equal_ignore_case(const char* a, const char* b, size_t len)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    for (size_t i = 0u; i < len; i++) {
        if (kbo_fa_declaration_ascii_lower(a[i]) != kbo_fa_declaration_ascii_lower(b[i])) {
            return 0;
        }
    }
    return 1;
}

static int kbo_fa_declaration_should_strip_team_suffix(
    const char* text,
    size_t first_len,
    const char* suffix,
    size_t suffix_len)
{
    if (text == NULL || suffix == NULL || first_len == 0u || suffix_len < 2u || suffix_len > 4u) {
        return 0;
    }
    if (first_len == suffix_len && kbo_fa_declaration_ascii_equal_ignore_case(text, suffix, suffix_len)) {
        return 1;
    }
    if (first_len >= suffix_len && kbo_fa_declaration_ascii_equal_ignore_case(text, suffix, suffix_len)) {
        return 1;
    }
    char first = kbo_fa_declaration_ascii_lower(text[0]);
    for (size_t i = 0u; i < suffix_len; i++) {
        if (kbo_fa_declaration_ascii_lower(suffix[i]) != first) {
            return 0;
        }
    }
    return 1;
}

static void kbo_fa_declaration_copy_display_team_name(const char* raw, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (raw == NULL || raw[0] == '\0') {
        return;
    }

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
                && kbo_fa_declaration_should_strip_team_suffix(raw, first_len, raw + suffix_start, len - suffix_start)) {
            len = split;
            while (len > 0u && (raw[len - 1u] == ' ' || raw[len - 1u] == '\t')) {
                len--;
            }
        }
    }

    if (len >= out_size) {
        len = out_size - 1u;
    }
    memcpy(out, raw, len);
    out[len] = '\0';
}

void kbo_fa_declaration_copy_team_name(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
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
        if (full_name[0] != '\0'
                && (strchr(full_name, ' ') != NULL || kbo_ootp_text_has_non_ascii(full_name))) {
            kbo_fa_declaration_copy_display_team_name(full_name, out, out_size);
            return;
        }
        if (city[0] != '\0' && nickname[0] != '\0' && _stricmp(city, nickname) != 0) {
            char combined[128] = {0};
            snprintf(combined, sizeof(combined), "%s %s", city, nickname);
            kbo_fa_declaration_copy_display_team_name(combined, out, out_size);
            return;
        }
        if (full_name[0] != '\0') {
            kbo_fa_declaration_copy_display_team_name(full_name, out, out_size);
            return;
        }
        if (nickname[0] != '\0') {
            kbo_fa_declaration_copy_display_team_name(nickname, out, out_size);
            return;
        }
        if (city[0] != '\0') {
            kbo_fa_declaration_copy_display_team_name(city, out, out_size);
            return;
        }
    }

    snprintf(
        out,
        out_size,
        "%s",
        kbo_fa_declaration_news_uses_korean()
            ? "\xec\x9b\x90\xec\x86\x8c\xec\x86\x8d\xed\x8c\x80"
            : "original club");
}

void kbo_fa_declaration_copy_team_link(uint32_t team_id, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';

    char team_name[96] = {0};
    kbo_fa_declaration_copy_team_name(team_id, team_name, sizeof(team_name));
    if (team_id != 0u) {
        snprintf(out, out_size, "<%s:team#%u>", team_name, team_id);
    } else {
        snprintf(out, out_size, "%s", team_name);
    }
}

static const char* kbo_fa_declaration_news_reason_label(const KboFaDeclarationCandidate* candidate)
{
    int use_korean = kbo_fa_declaration_news_uses_korean();
    if (candidate == NULL || candidate->decision_reason[0] == '\0') {
        return use_korean
            ? "\xec\x8b\xa0\xec\xb2\xad\xec\x9d\x84 \xeb\xaf\xb8\xeb\xa3\xa8\xea\xb3\xa0 \xec\x9e\x94\xeb\xa5\x98"
            : "deferred and stayed with original club";
    }
    if (strstr(candidate->decision_reason, "retry_after_down_year") != NULL) {
        return use_korean
            ? "\xeb\xb6\x80\xec\xa7\x84 \xec\x8b\x9c\xec\xa6\x8c \xed\x9b\x84 \xeb\x8b\xa4\xec\x9d\x8c \xea\xb8\xb0\xed\x9a\x8c\xeb\xa5\xbc \xed\x83\x9d\xed\x95\xa8"
            : "waiting for a better platform season";
    }
    if (strstr(candidate->decision_reason, "no_market_stay_original") != NULL) {
        return use_korean
            ? "\xec\x8b\x9c\xec\x9e\xa5 \xec\x83\x81\xed\x99\xa9\xec\x9d\x84 \xeb\xb3\xb4\xea\xb3\xa0 \xec\x9e\x94\xeb\xa5\x98\xeb\xa5\xbc \xed\x83\x9d\xed\x95\xa8"
            : "stayed after weighing the market";
    }
    return use_korean
        ? "\xec\x8b\xa0\xec\xb2\xad\xec\x9d\x84 \xeb\xaf\xb8\xeb\xa3\xa8\xea\xb3\xa0 \xec\x9e\x94\xeb\xa5\x98"
        : "deferred and stayed with original club";
}

static void kbo_fa_declaration_news_append_candidate_line(
    char* out,
    size_t out_size,
    const KboFaDeclarationCandidate* candidate,
    const char* template_key,
    const char* source)
{
    if (out == NULL || out_size == 0u || candidate == NULL || candidate->player_id == 0u) {
        return;
    }

    const char* name = candidate->player_name[0] != '\0'
        ? candidate->player_name
        : "FA candidate";
    char player_link[160] = {0};
    char team_name[96] = {0};
    char team_link[128] = {0};
    char team_id_text[16] = {0};
    char score_text[16] = {0};
    snprintf(player_link, sizeof(player_link), "<%s:player#%u>", name, candidate->player_id);
    kbo_fa_declaration_copy_team_name(candidate->team_id, team_name, sizeof(team_name));
    kbo_fa_declaration_copy_team_link(candidate->team_id, team_link, sizeof(team_link));
    snprintf(team_id_text, sizeof(team_id_text), "%u", candidate->team_id);
    snprintf(score_text, sizeof(score_text), "%d", candidate->score);

    KboNewsTemplateVar vars[] = {
        { "player_link", player_link },
        { "team_name", team_name },
        { "team_link", team_link },
        { "team_id", team_id_text },
        { "grade", candidate->grade[0] != '\0' ? candidate->grade : "-" },
        { "score", score_text },
        { "reason", kbo_fa_declaration_news_reason_label(candidate) },
    };

    char rendered[512] = {0};
    if (!kbo_news_template_render_key(
            template_key,
            vars,
            (int)(sizeof(vars) / sizeof(vars[0])),
            rendered,
            sizeof(rendered),
            source)) {
        snprintf(
            rendered,
            sizeof(rendered),
            "- %s / %s / %s",
            player_link,
            team_link,
            candidate->grade[0] != '\0' ? candidate->grade : "-");
    }

    kbo_news_text_append(out, out_size, rendered);
    if (!kbo_news_text_ends_with_newline(rendered)) {
        kbo_news_text_append(out, out_size, "\n");
    }
}

void kbo_fa_declaration_news_build_candidate_list(
    char* out,
    size_t out_size,
    const KboFaDeclarationCandidate* candidates,
    int candidate_count,
    int declared_filter,
    int retry_filter,
    int limit,
    const char* template_key,
    const char* source)
{
    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (candidates == NULL || candidate_count <= 0 || limit <= 0) {
        return;
    }

    uint32_t used_ids[16] = {0};
    int listed = 0;
    while (listed < limit && listed < (int)(sizeof(used_ids) / sizeof(used_ids[0]))) {
        const KboFaDeclarationCandidate* candidate = kbo_fa_declaration_news_find_best_candidate(
            candidates,
            candidate_count,
            declared_filter,
            retry_filter,
            used_ids,
            listed);
        if (candidate == NULL) {
            break;
        }
        used_ids[listed] = candidate->player_id;
        kbo_fa_declaration_news_append_candidate_line(out, out_size, candidate, template_key, source);
        listed++;
    }

    if (listed <= 0) {
        char none[128] = {0};
        if (!kbo_news_template_render_key(
                "fa_declaration.summary.none",
                NULL,
                0,
                none,
                sizeof(none),
                source)) {
            snprintf(none, sizeof(none), "- None");
        }
        kbo_news_text_append(out, out_size, none);
        if (!kbo_news_text_ends_with_newline(none)) {
            kbo_news_text_append(out, out_size, "\n");
        }
    }
}
