#include "../fa_rules_internal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../../core/core_flags/json/json_bool_parser.h"
#include "../fa_rules.h"
#include "../fa_rules_parts/fa_rules_paths.h"


KboFaRuleGradeConfig* kbo_fa_rules_ensure_grade(KboFaRules* rules, const char* grade)
{
    if (rules == NULL || grade == NULL || grade[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < rules->grade_count; i++) {
        if (_stricmp(rules->grades[i].grade, grade) == 0) {
            return &rules->grades[i];
        }
    }
    if (rules->grade_count >= KBO_FA_RULES_MAX_GRADES) {
        return NULL;
    }
    KboFaRuleGradeConfig* rule = &rules->grades[rules->grade_count++];
    memset(rule, 0, sizeof(*rule));
    kbo_fa_rules_copy_text(grade, rule->grade, sizeof(rule->grade));
    rule->enabled = 1u;
    return rule;
}



const char* kbo_fa_rules_json_value_end(const char* value, const char* end)
{
    value = kbo_json_skip_ws(value, end);
    if (value >= end) {
        return value;
    }
    if (*value == '"') {
        const char* stop = kbo_json_find_string_end(value, end);
        return stop != NULL ? stop + 1 : value;
    }
    if (*value == '{' || *value == '[') {
        char open = *value;
        char close = open == '{' ? '}' : ']';
        int depth = 0;
        int in_string = 0;
        int escaped = 0;
        const char* p = value;
        while (p < end) {
            char ch = *p;
            if (in_string) {
                if (escaped) {
                    escaped = 0;
                } else if (ch == '\\') {
                    escaped = 1;
                } else if (ch == '"') {
                    in_string = 0;
                }
                p++;
                continue;
            }
            if (ch == '"') {
                in_string = 1;
                p++;
                continue;
            }
            if (ch == open) {
                depth++;
            } else if (ch == close) {
                depth--;
                if (depth == 0) {
                    return p + 1;
                }
            }
            p++;
        }
        return value;
    }

    const char* p = value;
    while (p < end && *p != ',' && *p != '}' && *p != ']') {
        p++;
    }
    while (p > value && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\r' || p[-1] == '\n')) {
        p--;
    }
    return p;
}











const KboFaRuleGradeConfig* kbo_fa_rules_find_grade(const KboFaRules* rules, const char* grade)
{
    if (rules == NULL || grade == NULL || grade[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < rules->grade_count; i++) {
        if (rules->grades[i].enabled && _stricmp(rules->grades[i].grade, grade) == 0) {
            return &rules->grades[i];
        }
    }
    return NULL;
}


