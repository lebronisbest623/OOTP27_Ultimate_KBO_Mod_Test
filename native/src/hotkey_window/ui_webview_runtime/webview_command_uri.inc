static int kbo_webview_handle_command_uri(const char* uri, HWND hwnd)
{
    if (uri == NULL || strncmp(uri, "kbo://", 6) != 0) {
        return 0;
    }
    const char* cmd = uri + 6;

#include "webview_command_uri_parts/external_and_foreign_actions.inc"
#include "webview_command_uri_parts/view_navigation.inc"
#include "webview_command_uri_parts/mod_settings_commands.inc"
#include "webview_command_uri_parts/event_and_fa_commands.inc"
#include "webview_command_uri_parts/settings_commands.inc"
#include "webview_command_uri_parts/selection_dropdown_commands.inc"

    return 1;
}
