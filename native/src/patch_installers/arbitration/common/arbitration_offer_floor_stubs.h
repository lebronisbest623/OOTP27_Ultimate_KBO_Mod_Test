#ifndef KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_ARBITRATION_OFFER_FLOOR_STUBS_H_
#define KBOFIX_SRC_PATCH_INSTALLERS_ARBITRATION_ARBITRATION_OFFER_FLOOR_STUBS_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

uint8_t* build_kbo_salary_arbitration_final_zero_tender_stub(void* non_tender_continuation, void* tender_continuation);
uint8_t* build_kbo_salary_arbitration_ai_offer_write_681012_stub(void* offer_above_strong_continuation, void* offer_not_above_strong_continuation);
uint8_t* build_kbo_salary_arbitration_ai_offer_write_6827cd_stub(void* original_lea_value, void* continuation);
uint8_t* build_kbo_salary_arbitration_zero_offer_check_682089_stub(void* non_tender_continuation, void* continuation);
uint8_t* build_kbo_salary_arbitration_high_limit_non_tender_6820af_stub(void* tender_continuation, void* non_tender_call_continuation);

#endif
