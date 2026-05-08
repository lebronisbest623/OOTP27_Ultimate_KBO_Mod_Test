#ifndef KBO_BOOTSTRAP_PROFILER_H
#define KBO_BOOTSTRAP_PROFILER_H

#include <windows.h>

int kbo_profiler_begin(LARGE_INTEGER* out_start);
void kbo_profiler_end(const char* name, const LARGE_INTEGER* start);

#endif
