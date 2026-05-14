#ifndef KBOFIX_SRC_CAPTAIN_SEED_CAPTAIN_SEED_PARSE_H_
#define KBOFIX_SRC_CAPTAIN_SEED_CAPTAIN_SEED_PARSE_H_

#include <stdint.h>

#define KBO_CAPTAIN_SEED_MAX 128
#define KBO_CAPTAIN_SEED_KEY_BYTES 48
#define KBO_CAPTAIN_SEED_TEAM_CODE_BYTES 32
#define KBO_CAPTAIN_SEED_PLAYER_NAME_BYTES 96
#define KBO_CAPTAIN_SEED_SOURCE_BYTES 24

typedef struct KboCaptainSeed {
    uint32_t season;
    uint32_t league_id;
    uint32_t team_id;
    char team_code[KBO_CAPTAIN_SEED_TEAM_CODE_BYTES];
    uint32_t player_id;
    char player_key[KBO_CAPTAIN_SEED_KEY_BYTES];
    char player_name[KBO_CAPTAIN_SEED_PLAYER_NAME_BYTES];
    int32_t priority;
    uint8_t active;
    uint8_t source_rank;
    char source[KBO_CAPTAIN_SEED_SOURCE_BYTES];
} KboCaptainSeed;

int kbo_captain_parse_u32_full_token(const char* text, uint32_t* out);
int kbo_captain_parse_i32_full_token(const char* text, int32_t* out);
int kbo_parse_captain_seed_fields(char fields[][128], int field_count, KboCaptainSeed* out);
int kbo_parse_captain_seed_line(const char* line, KboCaptainSeed* out);

#endif
