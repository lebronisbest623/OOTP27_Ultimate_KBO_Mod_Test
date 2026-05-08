#ifndef KBOFIX_SRC_TEAM_TEAM_STRING_H_
#define KBOFIX_SRC_TEAM_TEAM_STRING_H_

#include <stddef.h>
#include <stdint.h>

int copy_limited_ascii_string(const char* source, char* out, size_t out_size);
int copy_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, char* out, size_t out_size);
int assign_ootp_string_object_text(uint8_t* object_base, uint32_t string_offset, const char* text);
int assign_ootp_string_object_text_if_different(uint8_t* object_base, uint32_t string_offset, const char* text);
int team_has_ootp_string_text(uint8_t* team, const char* expected);

#endif
