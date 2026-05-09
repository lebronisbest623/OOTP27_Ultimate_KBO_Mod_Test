#ifndef KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_AMATEUR_REPUTATION_CSV_H_
#define KBOFIX_SRC_AMATEUR_PLAYER_QUALITY_AMATEUR_REPUTATION_CSV_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_read_amateur_reputation_seed_file(const char* path, char** out_buffer, DWORD* out_size);
void kbo_amateur_reputation_read_cell(const char** cursor, char* out, size_t out_size);
uint32_t kbo_amateur_reputation_parse_u32(const char* text);

#endif
