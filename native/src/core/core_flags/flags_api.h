#ifndef KBO_CORE_FLAGS_FLAGS_API_H
#define KBO_CORE_FLAGS_FLAGS_API_H

#include <stdint.h>

int read_kbo_localappdata_flag_file(const char* file_name);

int kbo_get_intl_established_fa_multiplier(void);
int kbo_set_intl_established_fa_multiplier(int value);
int kbo_clamp_intl_established_fa_multiplier(int value);

int32_t kbo_clamp_foreign_fa_demand_baseline_value(int32_t value);
int32_t kbo_get_foreign_fa_demand_baseline_value(int index);
int32_t kbo_get_asian_quota_fa_demand_baseline_value(int index);
int32_t kbo_get_foreign_fa_demand_baseline_value_for_player(int index, int asian_quota);
int kbo_set_foreign_fa_demand_baseline_value(int index, int32_t value);
int kbo_set_asian_quota_fa_demand_baseline_value(int index, int32_t value);

int kbo_get_foreign_fa_quality_cap_enabled_setting(void);
int kbo_set_foreign_fa_quality_cap_enabled_setting(int enabled);

int kbo_get_profiler_enabled_setting(void);
int kbo_set_profiler_enabled_setting(int enabled);

int kbo_get_allow_all_ui_team_actions_setting(void);
int kbo_set_allow_all_ui_team_actions_setting(int enabled);

int kbo_fix_enabled(void);
void kbo_request_runtime_threads_stop(void);
int kbo_runtime_threads_should_continue(void);
int kbo_runtime_sleep_should_continue(uint32_t total_ms);

#endif
