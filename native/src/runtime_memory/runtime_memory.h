#ifndef KBOFIX_RUNTIME_MEMORY_H
#define KBOFIX_RUNTIME_MEMORY_H

#include <stdint.h>
#include <windows.h>

int memory_range_readable(const void* address, SIZE_T size);
uintptr_t get_ootp_global_database(void);
uint8_t* find_ootp_executable_pattern(const uint8_t* pattern, size_t pattern_len);
int is_kbo_historical_league_context(uintptr_t league_ptr);

#endif
