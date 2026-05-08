#include "foreign_roster_audit_paths.h"
#include <stdio.h>
#include <string.h>
#include "../../bootstrap/ootp_offsets.h"
#include "../../core/core_log.h"
#include "../../core/core_save_paths.h"
#include "../../core/core_text_date.h"
#include "../../core/core_flags/flags_api.h"
#include "../../runtime_memory/runtime_memory.h"

/* Foreign roster audit output paths. Included from foreign_roster_audit.inc. */

int get_kbo_foreign_roster_audit_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_roster_audit.csv", out, out_size);
}

int get_kbo_foreign_roster_snapshot_csv_path(char* out, size_t out_size)
{
    if (out == NULL || out_size < 2) {
        return 0;
    }
    return kbo_get_save_scoped_data_file("foreign_roster_snapshot.csv", out, out_size);
}
