#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#include "cbt_draft_order_penalty.h"
#include "../../../bootstrap/abi/hook_entrypoints.h"

typedef void (__fastcall *OotpDraftOrderCreateFn)(uintptr_t draft_state, uint8_t random_flag);

__declspec(noinline) void ootp_kbo_cbt_draft_order_create_wrapper(
    uintptr_t draft_state,
    uint8_t random_flag,
    uintptr_t original_func_ptr)
{
    OotpDraftOrderCreateFn original_func = (OotpDraftOrderCreateFn)original_func_ptr;
    if (original_func != NULL) {
        original_func(draft_state, random_flag);
    }

    kbo_cbt_note_draft_order_state(draft_state);
    kbo_cbt_apply_draft_order_penalties(draft_state, "draft_order_create_hook");
}
