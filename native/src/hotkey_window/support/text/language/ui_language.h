#ifndef KBO_HOTKEY_WINDOW_UI_LANGUAGE_H
#define KBO_HOTKEY_WINDOW_UI_LANGUAGE_H

#define KBO_HUB_LANG_KO 0
#define KBO_HUB_LANG_EN 1

int kbo_hub_language(void);
void kbo_hub_set_language(int language);
const char* kbo_hub_text(const char* ko, const char* en);
const char* kbo_hub_text_key(const char* key, const char* ko, const char* en);
void kbo_hub_load_language_setting(void);
void kbo_hub_save_language_setting(void);

#endif
