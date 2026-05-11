#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cbt_rules.h"
#include "../../core/core_flags/api/flags_api.h"
#include "../../core/files/save_paths/core_save_paths_internal.h"
#include "../../core/core_flags/json/json_bool_parser.h"
#include "../../core/logging/core_log.h"

/* KBO official CBT thresholds in millions of KRW (백만원).
   Assumes OOTP salary field is in millions of KRW — override via cbt_rules.json
   if your league uses a different salary unit. */
typedef struct { uint32_t season; int32_t threshold; } KboCbtBuiltinRow;
static const KboCbtBuiltinRow g_kbo_cbt_builtin[] = {
    { 2023u, 11426 },
    { 2024u, 11426 },
    { 2025u, 13711 },
    { 2026u, 14397 },
    { 2027u, 15117 },
    { 2028u, 15872 },
};
#define KBO_CBT_BUILTIN_COUNT ((int)(sizeof(g_kbo_cbt_builtin) / sizeof(g_kbo_cbt_builtin[0])))

static void kbo_cbt_rules_init_defaults(KboCbtRules* out)
{
    memset(out, 0, sizeof(*out));
    out->enabled                     = 1u;
    out->top_player_count            = 40u;
    out->draft_penalty_min_consecutive = 2u;
    out->draft_penalty_stages        = 9u;
    out->tax_rate_1                  = 50u;
    out->tax_rate_2                  = 100u;
    out->tax_rate_3plus              = 150u;
    out->annual_increase_pct         = 5u;
    out->threshold_override          = 0;
}

static void kbo_cbt_rules_parse_json(KboCbtRules* out, const char* json, DWORD json_size)
{
    int v = 0;
    if (kbo_find_flag_value_in_json(json, json_size, "enabled", NULL, &v)) {
        out->enabled = v ? 1u : 0u;
    }
    if (kbo_find_int_value_in_json(json, json_size, "top_player_count", &v) && v > 0 && v <= 200) {
        out->top_player_count = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "threshold_override", &v) && v > 0) {
        out->threshold_override = (int32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "tax_rate_1", &v) && v > 0 && v <= 1000) {
        out->tax_rate_1 = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "tax_rate_2", &v) && v > 0 && v <= 1000) {
        out->tax_rate_2 = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "tax_rate_3plus", &v) && v > 0 && v <= 1000) {
        out->tax_rate_3plus = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "annual_increase_pct", &v) && v >= 0 && v <= 100) {
        out->annual_increase_pct = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "draft_penalty_min_consecutive", &v) && v > 0 && v <= 10) {
        out->draft_penalty_min_consecutive = (uint32_t)v;
    }
    if (kbo_find_int_value_in_json(json, json_size, "draft_penalty_stages", &v) && v > 0 && v <= 100) {
        out->draft_penalty_stages = (uint32_t)v;
    }
}

void kbo_cbt_rules_load(KboCbtRules* out)
{
    if (out == NULL) {
        return;
    }
    kbo_cbt_rules_init_defaults(out);

    char path[MAX_PATH] = {0};
    if (!kbo_get_localappdata_utf8(path, sizeof(path))) {
        return;
    }

    char json_path[MAX_PATH] = {0};
    snprintf(json_path, sizeof(json_path), "%s\\OOTP-KBO\\cbt_rules.json", path);

    HANDLE file = CreateFileA(json_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size == 0u || size > 65536u) {
        CloseHandle(file);
        return;
    }

    char* buf = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size + 1u);
    if (buf == NULL) {
        CloseHandle(file);
        return;
    }

    DWORD read = 0;
    if (ReadFile(file, buf, size, &read, NULL) && read > 0) {
        buf[read] = '\0';
        kbo_cbt_rules_parse_json(out, buf, read);
        append_logf("KBO CBT rules loaded path=%s enabled=%u top=%u override=%d",
            json_path, (uint32_t)out->enabled, out->top_player_count, out->threshold_override);
    }
    CloseHandle(file);
    HeapFree(GetProcessHeap(), 0, buf);
}

int32_t kbo_cbt_get_threshold(const KboCbtRules* rules, uint32_t season)
{
    if (rules == NULL) {
        return 0;
    }
    if (rules->threshold_override > 0) {
        return rules->threshold_override;
    }

    /* Look up the builtin table */
    int32_t best = g_kbo_cbt_builtin[0].threshold;
    uint32_t best_season = 0u;
    for (int i = 0; i < KBO_CBT_BUILTIN_COUNT; i++) {
        if (season == g_kbo_cbt_builtin[i].season) {
            return g_kbo_cbt_builtin[i].threshold;
        }
        if (g_kbo_cbt_builtin[i].season <= season && g_kbo_cbt_builtin[i].season >= best_season) {
            best = g_kbo_cbt_builtin[i].threshold;
            best_season = g_kbo_cbt_builtin[i].season;
        }
    }

    /* Apply annual increase for seasons beyond the last known entry */
    uint32_t last_known = g_kbo_cbt_builtin[KBO_CBT_BUILTIN_COUNT - 1].season;
    if (season > last_known && rules->annual_increase_pct > 0u) {
        uint32_t years_past = season - last_known;
        int32_t val = g_kbo_cbt_builtin[KBO_CBT_BUILTIN_COUNT - 1].threshold;
        for (uint32_t y = 0u; y < years_past; y++) {
            val = val + (int32_t)((int64_t)val * (int32_t)rules->annual_increase_pct / 100LL);
        }
        return val;
    }

    return best;
}

uint32_t kbo_cbt_get_tax_rate(const KboCbtRules* rules, uint32_t consecutive_count)
{
    if (rules == NULL || consecutive_count == 0u) {
        return 0u;
    }
    if (consecutive_count == 1u) {
        return rules->tax_rate_1;
    }
    if (consecutive_count == 2u) {
        return rules->tax_rate_2;
    }
    return rules->tax_rate_3plus;
}
