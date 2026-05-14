#ifndef KBOFIX_SRC_CORE_TEXT_OOTP_TEXT_ENCODING_H_
#define KBOFIX_SRC_CORE_TEXT_OOTP_TEXT_ENCODING_H_

int kbo_text_has_non_ascii(const char* text);
char* kbo_alloc_ootp_internal_text(const char* text);
void kbo_free_ootp_internal_text(char* text);

#endif
