#ifndef KBOFIX_SRC_PERF_SNAPSHOT_PERF_SNAPSHOT_H_
#define KBOFIX_SRC_PERF_SNAPSHOT_PERF_SNAPSHOT_H_

#include <stddef.h>

int kbo_dump_perf_player_snapshot(const char* source, char* out_path, size_t out_path_size);

#endif
