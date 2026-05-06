static const char* kbo_hub_nav_label(int index)
{
    switch (index) {
    case KBO_HUB_VIEW_MOD_INFO:       return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    case KBO_HUB_VIEW_MILITARY:       return kbo_hub_text("\xea\xb5\xb0\xeb\xb3\xb5\xeb\xac\xb4",  "SERVICE");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c",  "RIGHTS");
    case KBO_HUB_VIEW_ASIAN_QUOTA:    return kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8", "FOREIGN");
    case KBO_HUB_VIEW_ASIAN_GAMES:    return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84", "AGAMES");
    case KBO_HUB_VIEW_SETTINGS:       return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",    "SETTINGS");
    default:                          return "";
    }
}
