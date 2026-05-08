__declspec(noinline) int ootp_kbo_fa_offer_screen_callback_probe_wrapper(
    uintptr_t screen_ptr,
    uintptr_t sender_ptr,
    uintptr_t callback_id,
    uintptr_t value,
    uintptr_t original_func_ptr)
{
    int result = 0;
    uint32_t player_id = 0;
    int32_t selected_mode = -1;
    uintptr_t contract_type_control = 0;
    uintptr_t major_option_control = 0;
    uintptr_t submit_control = 0;

    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0x310)) {
        player_id = *(uint32_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_PLAYER_ID_OFFSET);
        contract_type_control = *(uintptr_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_CONTRACT_TYPE_CONTROL_OFFSET);
        major_option_control = *(uintptr_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_MAJOR_OPTION_CONTROL_OFFSET);
        submit_control = *(uintptr_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_SUBMIT_CONTROL_OFFSET);
        uintptr_t owner = *(uintptr_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_OWNER_OFFSET);
        if (owner != 0 && memory_range_readable((void*)owner, 0x108)) {
            selected_mode = *(int32_t*)(owner + OOTP27_FA_OFFER_SCREEN_OWNER_MODE_OFFSET);
        }
    }

    static LONG callback_log_count = 0;
    LONG slot = InterlockedIncrement(&callback_log_count);
    if (slot <= 400 && callback_id >= 0xf60u && callback_id <= 0xfa0u) {
        append_logf(
            "KBO no-minor offer callback: screen=%p sender=%p id=0x%llx value=%p player=%u selected=%d contract_ctl=%p major_option_ctl=%p submit_ctl=%p",
            (void*)screen_ptr,
            (void*)sender_ptr,
            (unsigned long long)callback_id,
            (void*)value,
            player_id,
            selected_mode,
            (void*)contract_type_control,
            (void*)major_option_control,
            (void*)submit_control);
    }

    OotpFaOfferScreenCallbackProbeFn original_func = (OotpFaOfferScreenCallbackProbeFn)original_func_ptr;
    if (original_func != NULL) {
        result = original_func((void*)screen_ptr, (void*)sender_ptr, callback_id, value);
    }

    if (screen_ptr != 0 && memory_range_readable((void*)screen_ptr, 0x310)) {
        uintptr_t owner = *(uintptr_t*)(screen_ptr + OOTP27_FA_OFFER_SCREEN_OWNER_OFFSET);
        int32_t after_mode = -1;
        if (owner != 0 && memory_range_readable((void*)owner, 0x108)) {
            after_mode = *(int32_t*)(owner + OOTP27_FA_OFFER_SCREEN_OWNER_MODE_OFFSET);
        }
        LONG after_slot = InterlockedIncrement(&callback_log_count);
        if (after_slot <= 400 && callback_id >= 0xf60u && callback_id <= 0xfa0u) {
            append_logf(
                "KBO no-minor offer callback after: screen=%p id=0x%llx result=%d selected=%d",
                (void*)screen_ptr,
                (unsigned long long)callback_id,
                result,
                after_mode);
        }
    }

    return result;
}

__declspec(noinline) int ootp_kbo_fa_contract_offer_callback_probe_wrapper(
    uintptr_t offer_ptr,
    uintptr_t sender_ptr,
    uintptr_t callback_id,
    uintptr_t value,
    uintptr_t original_func_ptr)
{
    int result = 0;
    int32_t selected_player = -1;
    int32_t before_salary = -1;
    int32_t before_option_salary = -1;
    uint8_t before_major_flag = 0xffu;

    if (offer_ptr != 0 && memory_range_readable((void*)offer_ptr, 0xd0)) {
        selected_player = *(int32_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_PLAYER_ID_OFFSET);
        before_salary = *(int32_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_SALARY_OFFSET);
        before_option_salary = *(int32_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_OPTION_SALARY_OFFSET);
        before_major_flag = *(uint8_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_MAJOR_FLAG_OFFSET);
    }

    static LONG contract_callback_log_count = 0;
    LONG slot = InterlockedIncrement(&contract_callback_log_count);
    if (slot <= 600) {
        append_logf(
            "KBO no-minor contract callback: offer=%p sender=%p id=0x%llx value=%p player=%d salary=%d option_salary=%d major_flag=%u",
            (void*)offer_ptr,
            (void*)sender_ptr,
            (unsigned long long)callback_id,
            (void*)value,
            selected_player,
            before_salary,
            before_option_salary,
            (unsigned)before_major_flag);
    }

    OotpFaContractOfferCallbackProbeFn original_func = (OotpFaContractOfferCallbackProbeFn)original_func_ptr;
    if (original_func != NULL) {
        result = original_func((void*)offer_ptr, (void*)sender_ptr, callback_id, value);
    }

    if (offer_ptr != 0 && memory_range_readable((void*)offer_ptr, 0xd0)) {
        int32_t after_salary = *(int32_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_SALARY_OFFSET);
        int32_t after_option_salary = *(int32_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_OPTION_SALARY_OFFSET);
        uint8_t after_major_flag = *(uint8_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_MAJOR_FLAG_OFFSET);
        *(uint8_t*)(offer_ptr + OOTP27_FA_CONTRACT_OFFER_MAJOR_FLAG_OFFSET) = 1u;

        LONG after_slot = InterlockedIncrement(&contract_callback_log_count);
        if (after_slot <= 600) {
            append_logf(
                "KBO no-minor contract callback after: offer=%p id=0x%llx result=%d salary=%d option_salary=%d major_flag=%u forced_major_flag=1",
                (void*)offer_ptr,
                (unsigned long long)callback_id,
                result,
                after_salary,
                after_option_salary,
                (unsigned)after_major_flag);
        }
    }

    return result;
}

