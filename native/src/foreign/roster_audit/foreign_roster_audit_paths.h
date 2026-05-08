#ifndef KBOFIX_SRC_FOREIGN_ROSTER_AUDIT_FOREIGN_ROSTER_AUDIT_PATHS_H_
#define KBOFIX_SRC_FOREIGN_ROSTER_AUDIT_FOREIGN_ROSTER_AUDIT_PATHS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int get_kbo_foreign_roster_audit_csv_path(char* out, size_t out_size);
int get_kbo_foreign_roster_snapshot_csv_path(char* out, size_t out_size);

#endif
