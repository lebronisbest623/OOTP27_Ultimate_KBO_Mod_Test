#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_TEAM_POLICY_PARSE_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_SERVICE_TEAM_POLICY_PARSE_H_

typedef struct KboMilitaryServiceTeamPolicyRow {
    char team_csv_id[16];
    int enabled;
} KboMilitaryServiceTeamPolicyRow;

int kbo_parse_military_service_team_policy_line(
    const char* line,
    KboMilitaryServiceTeamPolicyRow* out);

#endif
