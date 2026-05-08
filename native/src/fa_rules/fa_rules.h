#ifndef KBOFIX_SRC_FA_RULES_FA_RULES_H_
#define KBOFIX_SRC_FA_RULES_FA_RULES_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>

#define KBO_FA_RULES_MAX_GRADES 8
#define KBO_FA_RULES_MAX_CASES 16

typedef struct KboFaRuleGradeConfig {
    char grade[12];
    uint32_t protect_count;
    uint32_t cash_with_player_percent;
    uint32_t cash_only_percent;
    uint8_t requires_player_compensation;
    uint8_t enabled;
} KboFaRuleGradeConfig;

typedef struct KboFaRules {
    uint32_t version;
    uint8_t exclude_foreign_players;
    uint32_t protected_list_due_days;
    uint8_t age_grade_override_enabled;
    uint32_t age_grade_min_age;
    char age_grade[12];
    int grade_count;
    KboFaRuleGradeConfig grades[KBO_FA_RULES_MAX_GRADES];
    int compensable_case_count;
    char compensable_cases[KBO_FA_RULES_MAX_CASES][48];
    char source_path[MAX_PATH];
    uint8_t loaded_from_file;
} KboFaRules;

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
