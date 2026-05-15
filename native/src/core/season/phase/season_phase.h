#ifndef KBOFIX_SRC_CORE_SEASON_SEASON_PHASE_H_
#define KBOFIX_SRC_CORE_SEASON_SEASON_PHASE_H_

#include <stdint.h>

#define KBO_SEASON_PHASE_OFFSEASON_RESET 0u
#define KBO_SEASON_PHASE_OFFSEASON_STARTED 1u
#define KBO_SEASON_PHASE_PRESEASON 2u
#define KBO_SEASON_PHASE_REGULAR_SEASON 3u
#define KBO_SEASON_PHASE_POSTSEASON 4u
#define KBO_SEASON_PHASE_UNKNOWN 0xffu

typedef struct KboSeasonPhaseInfo {
    uint32_t date;
    uint32_t league_id;
    uintptr_t league_ptr;
    uint32_t league_year;
    uint8_t raw_phase;
    uint32_t raw_phase_year;
    uint8_t captured_phase;
    uint8_t capture_raw_phase_after_write;
    uint32_t capture_phase_year;
    uint32_t capture_site_rva;
    uint32_t capture_date;
    uint32_t capture_sequence;
    uint8_t effective_phase;
    uint32_t opening_day;
    uint32_t known_offseason_start;
    int raw_valid;
    int capture_valid;
    int capture_used;
    int opening_day_valid;
    int corrected;
} KboSeasonPhaseInfo;

const char* kbo_season_phase_label(uint8_t phase);
int kbo_season_phase_is_offseason(uint8_t phase);
int kbo_season_phase_can_enter_offseason(uint8_t phase);
int kbo_season_phase_is_preseason_or_regular(uint8_t phase);

uint8_t kbo_season_phase_effective_from_values(
    uint32_t today_yyyymmdd,
    uint32_t league_year,
    uint8_t raw_phase,
    uint32_t opening_day_yyyymmdd,
    uint32_t known_offseason_start_yyyymmdd,
    int* out_corrected);

int kbo_season_phase_read_raw(uint32_t league_id, KboSeasonPhaseInfo* out_info);
int kbo_season_phase_resolve(
    uint32_t league_id,
    uint32_t today_yyyymmdd,
    uint32_t known_offseason_start_yyyymmdd,
    KboSeasonPhaseInfo* out_info);

#endif
