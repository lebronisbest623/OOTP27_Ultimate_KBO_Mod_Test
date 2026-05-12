#ifndef KBOFIX_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_H_
#define KBOFIX_SRC_FOREIGN_REPLACEMENT_SEED_FOREIGN_REPLACEMENT_SEED_H_

#include <stdint.h>

#include "../parse/foreign_replacement_seed_parse.h"

#define KBO_FOREIGN_REPLACEMENT_PLAYER_SEED_MAX 128

void kbo_ensure_foreign_replacement_player_seeds_loaded(void);
uint32_t kbo_resolve_foreign_replacement_player_seed_key(const char* key, uint8_t* out_slot_type);
int kbo_foreign_replacement_player_seed_matches_loaded(uint8_t* player, uint8_t* out_slot_type);

#endif
