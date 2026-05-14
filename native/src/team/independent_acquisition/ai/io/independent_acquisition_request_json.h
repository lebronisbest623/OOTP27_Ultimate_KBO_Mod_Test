#ifndef KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_INDEPENDENT_ACQUISITION_REQUEST_JSON_H_
#define KBOFIX_SRC_TEAM_INDEPENDENT_ACQUISITION_AI_IO_INDEPENDENT_ACQUISITION_REQUEST_JSON_H_

#include <stddef.h>

#include "../independent_acquisition_ai_internal.h"

void kbo_independent_acquisition_json_append_escaped(
    char* out,
    size_t out_size,
    const char* text);
int kbo_independent_acquisition_parse_request_line(
    const char* line,
    KboIndependentAcquisitionQueuedRequest* out);

#endif
