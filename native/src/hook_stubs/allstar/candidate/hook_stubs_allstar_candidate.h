#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_ALLSTAR_CANDIDATE_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_ALLSTAR_CANDIDATE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_allstar_candidate_team_split_stub(void* return_address, void* seeded_address, void* vector_push_back_address, uint32_t subleague_array_offset, uint32_t subleague_count_offset);
uint8_t* build_allstar_candidate_team_roster_push_filter_stub(void* return_address, void* vector_push_back_address);
uint8_t* build_allstar_candidate_player_push_filter_stub(void* return_address, void* skip_address, void* vector_push_back_address);
uint8_t* build_allstar_candidate_ranked_player_push_filter_stub(void* return_address, void* skip_address, void* vector_push_back_address, int vector_base_in_rax);

#endif
