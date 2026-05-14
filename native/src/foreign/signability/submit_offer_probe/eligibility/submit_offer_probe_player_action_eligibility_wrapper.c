#include "../internal/submit_offer_probe_internal.h"

__declspec(noinline) uint8_t ootp_kbo_player_action_eligibility_wrapper(
    uintptr_t action_context,
    int32_t action_id,
    uint8_t strict_check,
    uintptr_t original_func_ptr)
{
    /* OOTP player action ids 0x35/0x36 are Offer Minor Lg Contract/Extension. */
    if (action_id == 0x35 || action_id == 0x36) {
        static LONG blocked_log_count = 0;
        LONG slot = InterlockedIncrement(&blocked_log_count);
        if (slot <= 80) {
            append_logf(
                "KBO no-minor player action blocked: context=%p action=0x%x strict=%u",
                (void*)action_context,
                action_id,
                (unsigned)strict_check);
        }
        return 0;
    }

    OotpPlayerActionEligibilityFn original_func = (OotpPlayerActionEligibilityFn)original_func_ptr;
    if (original_func == NULL) {
        return 0;
    }
    return original_func((void*)action_context, action_id, strict_check);
}

