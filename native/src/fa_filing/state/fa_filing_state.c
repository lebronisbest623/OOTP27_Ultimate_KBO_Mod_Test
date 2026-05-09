#include "../fa_filing_internal.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../bootstrap/abi/ootp_offsets.h"
#include "../../bootstrap/profiling/profiler.h"
#include "../../core/logging/core_log.h"
#include "../../core/files/save_paths/core_save_paths.h"
#include "../../runtime_memory/runtime_memory.h"
#include "../../team/lookup/team_lookup.h"
#include "../../team/names/team_name_cache.h"
#include "../fa_filing.h"
#include "../fa_filing_parts/fa_filing_csv_parse.h"
#include "../fa_filing_parts/fa_filing_csv_write_helpers.h"

volatile LONG g_kbo_fa_filing_lock = 0;
volatile LONG g_kbo_fa_filing_cache_dirty = 1;
KboFaFilingRecord* g_kbo_fa_filing_cache_rows = NULL;
int g_kbo_fa_filing_cache_count = 0;
char g_kbo_fa_filing_cache_path[MAX_PATH] = {0};

#define KBO_FA_FILING_NEGATIVE_CACHE_MAX 512
uint32_t g_kbo_fa_filing_negative_cache[KBO_FA_FILING_NEGATIVE_CACHE_MAX];
volatile LONG g_kbo_fa_filing_negative_cache_cursor = 0;













