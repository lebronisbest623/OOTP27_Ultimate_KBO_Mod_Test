#ifndef KBOFIX_SRC_MILITARY_SERVICE_MILITARY_NATIVE_LOAN_H_
#define KBOFIX_SRC_MILITARY_SERVICE_MILITARY_NATIVE_LOAN_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int kbo_player_native_on_loan(uint8_t* player);
int kbo_clear_native_player_loan(uint8_t* player);

#endif
