#ifndef KBOFIX_SRC_CORE_CORE_SAVE_PATHS_H_
#define KBOFIX_SRC_CORE_CORE_SAVE_PATHS_H_

#include <stddef.h>

int kbo_get_current_save_path(char* out, size_t out_size);
int kbo_get_save_scoped_data_dir(char* out, size_t out_size);
int kbo_get_save_scoped_data_file(const char* file_name, char* out, size_t out_size);

#endif
