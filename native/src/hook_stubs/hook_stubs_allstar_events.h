#ifndef KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_ALLSTAR_EVENTS_H_
#define KBOFIX_SRC_HOOK_STUBS_HOOK_STUBS_ALLSTAR_EVENTS_H_

#include <stdint.h>

uint8_t* build_allstar_events_prepare_stub(
    void* return_address,
    void* allstar_prep_address,
    uint32_t game_flag_offset);
uint8_t* build_allstar_schedule_import_capture_stub_r(
    void* return_address,
    uint32_t game_flag_offset,
    int reg_idx);
uint8_t* build_allstar_schedule_import_capture_stub(
    void* return_address,
    uint32_t game_flag_offset);
uint8_t* build_allstar_voting_begin_prepare_stub(
    void* return_address,
    void* no_game_address,
    void* allstar_team_setup_address,
    uint32_t game_flag_offset);

#endif
