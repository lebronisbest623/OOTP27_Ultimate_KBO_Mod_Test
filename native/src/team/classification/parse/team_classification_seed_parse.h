#ifndef KBOFIX_SRC_TEAM_CLASSIFICATION_SEED_PARSE_H_
#define KBOFIX_SRC_TEAM_CLASSIFICATION_SEED_PARSE_H_

typedef struct KboTeamClassificationSeedRow {
    char team_csv_id[16];
    int enabled;
    char team_type[32];
    char league_level[32];
    char display_name[96];
} KboTeamClassificationSeedRow;

#define KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_NONE    0
#define KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_FUTURES 1
#define KBO_TEAM_CLASSIFICATION_INDEPENDENT_KIND_LEAGUE  2

int kbo_parse_team_classification_seed_line(
    const char* line,
    KboTeamClassificationSeedRow* out);
int kbo_team_classification_seed_row_independent_kind(
    const KboTeamClassificationSeedRow* row);
int kbo_team_classification_seed_row_is_independent_futures(
    const KboTeamClassificationSeedRow* row);

#endif
