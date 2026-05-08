#ifndef KBOFIX_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_H_
#define KBOFIX_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_H_

#include <stdint.h>

#include "foreign_replacement_seed_parse.h"

#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128

void kbo_ensure_foreign_replacement_player_seeds_loaded(void);
int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type);

#endif
