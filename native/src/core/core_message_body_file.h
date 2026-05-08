#ifndef KBOFIX_SRC_CORE_CORE_MESSAGE_BODY_FILE_H_
#define KBOFIX_SRC_CORE_CORE_MESSAGE_BODY_FILE_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

int write_kbo_message_body_file(uint32_t message_id, const char* title, const char* body, const char* source);

#endif
