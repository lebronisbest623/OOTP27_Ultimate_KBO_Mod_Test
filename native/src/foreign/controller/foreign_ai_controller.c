#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "../../core/core_flags/api/flags_api.h"
#include "foreign_ai_controller.h"

int kbo_foreign_ai_controller_enabled(void)
{
    return kbo_fix_enabled()
        && read_kbo_localappdata_flag_file("enable_foreign_ai_controller.txt")
        && !read_kbo_localappdata_flag_file("disable_foreign_ai_controller.txt");
}
