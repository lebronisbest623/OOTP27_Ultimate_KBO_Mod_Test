#ifndef NATIVE_SRC_FA_RULES_FA_RULES_C_INTERNAL_H
#define NATIVE_SRC_FA_RULES_FA_RULES_C_INTERNAL_H

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../core/core_flags/json/json_bool_parser.h"
#include "fa_rules.h"
#include "fa_rules_parts/fa_rules_paths.h"


KboFaRuleGradeConfig* kbo_fa_rules_ensure_grade(KboFaRules* rules, const char* grade);
const char* kbo_fa_rules_json_value_end(const char* value, const char* end);
const KboFaRuleGradeConfig* kbo_fa_rules_find_grade(const KboFaRules* rules, const char* grade);
void kbo_fa_rules_copy_text(const char* value, char* out, size_t out_size);
void kbo_fa_rules_add_case(KboFaRules* rules, const char* case_label);
void kbo_fa_rules_init_defaults(KboFaRules* rules);
int kbo_fa_rules_find_json_value_span(
    const char* start,
    const char* end,
    const char* key,
    const char** out_start,
    const char** out_end);
int kbo_fa_rules_read_bool(const char* start, const char* end, const char* key, uint8_t* out_value);
int kbo_fa_rules_read_u32(const char* start, const char* end, const char* key, uint32_t* out_value);
int kbo_fa_rules_copy_json_string_value(
    const char* value_start,
    const char* value_end,
    char* out,
    size_t out_size);
int kbo_fa_rules_read_string(
    const char* start,
    const char* end,
    const char* key,
    char* out,
    size_t out_size);
void kbo_fa_rules_parse_cases(KboFaRules* rules, const char* start, const char* end);
void kbo_fa_rules_parse_grade_object(
    KboFaRules* rules,
    const char* grade,
    const char* start,
    const char* end);
void kbo_fa_rules_parse_json(KboFaRules* rules, const char* json, DWORD json_size);
int kbo_fa_rules_load(KboFaRules* rules);
int kbo_fa_rules_case_is_compensable(const KboFaRules* rules, const char* case_label);
int kbo_fa_rules_grade_is_compensable(const KboFaRules* rules, const char* grade);
void kbo_fa_rules_calculate_compensation(
    const KboFaRules* rules,
    const char* grade,
    int32_t previous_salary,
    uint32_t* out_cash_with_player,
    uint32_t* out_cash_only,
    uint32_t* out_protect_count,
    uint8_t* out_requires_player);

#endif
