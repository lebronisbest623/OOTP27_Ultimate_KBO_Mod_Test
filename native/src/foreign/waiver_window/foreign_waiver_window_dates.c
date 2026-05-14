#include <stdint.h>

#include "../waiver_core/api/foreign_waiver_core.h"
#include "state/foreign_waiver_window_state.h"

int kbo_current_foreign_waiver_window_dates(uint32_t* out_start, uint32_t* out_end)
{
    if (out_start != NULL) { *out_start = 0u; }
    if (out_end != NULL) { *out_end = 0u; }

    uint32_t start = 0u;
    uint32_t end = 0u;
    if (kbo_read_foreign_waiver_window(&start, &end)) {
        if (out_start != NULL) { *out_start = start; }
        if (out_end != NULL) { *out_end = end; }
        return start != 0u && end != 0u;
    }

    if (g_kbo_foreign_waiver_start_event_date != 0u && g_kbo_foreign_waiver_close_event_end_date != 0u) {
        if (out_start != NULL) { *out_start = g_kbo_foreign_waiver_start_event_date; }
        if (out_end != NULL) { *out_end = g_kbo_foreign_waiver_close_event_end_date; }
        return 1;
    }

    return 0;
}
