#ifndef KBO_FOREIGN_REPLACEMENT_SEED_PARSE_H
#define KBO_FOREIGN_REPLACEMENT_SEED_PARSE_H

#include <stdint.h>

#ifndef KBO_FOREIGN_INJURY_SLOT_REGULAR
#define KBO_FOREIGN_INJURY_SLOT_REGULAR 1
#endif

#ifndef KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA
#define KBO_FOREIGN_INJURY_SLOT_ASIAN_QUOTA 2
#endif

#define KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES 40

typedef struct KboForeignReplacementPlayerSeed {
    char key[KBO_FOREIGN_REPLACEMENT_PLAYER_KEY_BYTES];
    uint32_t player_id;
    uint8_t slot_type;
} KboForeignReplacementPlayerSeed;

int kbo_ascii_is_seed_id_char(char ch);
uint8_t kbo_parse_foreign_replacement_seed_slot_type(const char* text);
int kbo_parse_foreign_replacement_player_seed_line(
    const char* line,
    KboForeignReplacementPlayerSeed* out);

#endif
