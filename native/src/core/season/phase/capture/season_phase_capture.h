#ifndef KBOFIX_SRC_CORE_SEASON_PHASE_SEASON_PHASE_CAPTURE_H_
#define KBOFIX_SRC_CORE_SEASON_PHASE_SEASON_PHASE_CAPTURE_H_

#include <stdint.h>
#include <windows.h>

#include "../season_phase.h"

#define KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE 64u
#define KBO_SEASON_PHASE_CAPTURE_EVENT_RING_MASK (KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE - 1u)

typedef struct KboSeasonPhaseCaptureSnapshot {
    uint32_t sequence;
    uint32_t date;
    uint32_t league_id;
    uintptr_t league_ptr;
    uint32_t league_year;
    uint8_t phase;
    uint8_t raw_phase_after_write;
    uint32_t phase_year;
    uint32_t site_rva;
} KboSeasonPhaseCaptureSnapshot;

extern volatile LONG g_kbo_season_phase_capture_event_write_cursor;
extern volatile LONG g_kbo_season_phase_capture_event_published_sequence;
extern volatile LONG g_kbo_season_phase_capture_event_consumed_sequence;
extern uintptr_t g_kbo_season_phase_capture_event_league_ptrs[KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE];
extern uint32_t g_kbo_season_phase_capture_event_values[KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE];
extern uint32_t g_kbo_season_phase_capture_event_site_rvas[KBO_SEASON_PHASE_CAPTURE_EVENT_RING_SIZE];

void kbo_season_phase_capture_drain(uint32_t league_id, uint32_t today_yyyymmdd);
int kbo_season_phase_capture_latest(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    KboSeasonPhaseCaptureSnapshot* out_snapshot);

#endif
