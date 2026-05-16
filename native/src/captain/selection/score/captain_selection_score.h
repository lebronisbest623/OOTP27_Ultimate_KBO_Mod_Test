#ifndef KBOFIX_SRC_CAPTAIN_SELECTION_SCORE_H_
#define KBOFIX_SRC_CAPTAIN_SELECTION_SCORE_H_

#include "../../internal/captain_selection_internal.h"

int32_t kbo_captain_candidate_score(KboCaptainSelectionRow* row);
int kbo_captain_candidate_should_replace(
    const KboCaptainSelectionRow* current,
    const KboCaptainSelectionRow* candidate);

#endif