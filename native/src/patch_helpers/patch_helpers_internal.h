#ifndef NATIVE_SRC_PATCH_HELPERS_PATCH_HELPERS_C_INTERNAL_H
#define NATIVE_SRC_PATCH_HELPERS_PATCH_HELPERS_C_INTERNAL_H

#include "patch_helpers.h"
#include <stdio.h>
#include <string.h>
#include "../bootstrap/abi/ootp_offsets.h"
#include "../core/logging/core_log.h"
#include "../core/dates/core_current_date.h"
#include "../core/files/save_paths/core_save_paths.h"
#include "../core/dates/core_text_date.h"
#include "../core/core_flags/api/flags_api.h"
#include "../runtime_memory/runtime_memory.h"


void write_u64(uint8_t* dst, uint64_t value);
void write_u32(uint8_t* dst, uint32_t value);
int is_r11_absolute_jump_patch(const uint8_t* target);
int is_rax_absolute_jump_patch(const uint8_t* target);
int is_rip_absolute_jump_patch(const uint8_t* target);
void log_patch_bytes_mismatch(const char* label, const uint8_t* target, size_t size);
void log_extended_context(const char* label, const uint8_t* target, int pre_bytes, int total_bytes);
int kbo_memory_matches_masked_pattern(const uint8_t* data, const uint8_t* pattern, const uint8_t* mask, size_t size);
int patch_static_bytes(const char* label, uint8_t* target, const uint8_t* expected, const uint8_t* patch, size_t size);

#endif
