static const char* kbo_hub_nav_label(int index)
{
    switch (index) {
    case KBO_HUB_VIEW_MOD_INFO:       return kbo_hub_text("\xeb\xaa\xa8\xeb\x93\x9c \xec\xa0\x95\xeb\xb3\xb4", "MOD INFO");
    case KBO_HUB_VIEW_MILITARY:       return kbo_hub_text("\xea\xb5\xb0\xea\xb2\xbd\xed\x8c\x80",  "SERVICE TEAMS");
    case KBO_HUB_VIEW_FOREIGN_RIGHTS: return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c",  "RIGHTS");
    case KBO_HUB_VIEW_ASIAN_QUOTA:    return kbo_hub_text("\xec\x99\xb8\xea\xb5\xad\xec\x9d\xb8 \xec\x84\xa0\xec\x88\x98", "FOREIGN PLAYERS");
    case KBO_HUB_VIEW_ASIAN_GAMES:    return kbo_hub_text("\xec\x95\x84\xec\x8b\x9c\xec\x95\x88\xea\xb2\x8c\xec\x9e\x84", "AGAMES");
    case KBO_HUB_VIEW_UPCOMING_FA:    return "";
    case KBO_HUB_VIEW_FA_CASES:       return kbo_hub_text("FA", "FA");
    case KBO_HUB_VIEW_SETTINGS:       return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95",    "SETTINGS");
    case KBO_HUB_VIEW_REPUTATION:     return kbo_hub_text("\xed\x8f\x89\xed\x8c\x90", "REPUTATION");
    default:                          return "";
    }
}

static const char* kbo_hub_foreign_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_FOREIGN_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "ROSTER");
    case KBO_HUB_FOREIGN_SUBVIEW_RIGHTS:
        return kbo_hub_text("\xeb\xb3\xb4\xeb\xa5\x98\xea\xb6\x8c", "RIGHTS");
    default:
        return "";
    }
}

static const char* kbo_hub_agames_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_AGAMES_SUBVIEW_TOURNAMENTS:
        return kbo_hub_text("\xeb\x8c\x80\xed\x9a\x8c", "TOURNAMENTS");
    case KBO_HUB_AGAMES_SUBVIEW_SCHEDULE:
        return kbo_hub_text("\xec\x9d\xbc\xec\xa0\x95", "SCHEDULE");
    case KBO_HUB_AGAMES_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "ROSTER");
    default:
        return "";
    }
}

static const char* kbo_hub_fa_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_FA_SUBVIEW_MARKET:
        return kbo_hub_text("\xec\x8b\x9c\xec\x9e\xa5", "MARKET");
    case KBO_HUB_FA_SUBVIEW_COMPENSATION:
        return kbo_hub_text("\xeb\xb3\xb4\xec\x83\x81", "COMPENSATION");
    default:
        return "";
    }
}

static const char* kbo_hub_military_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_MILITARY_SUBVIEW_ROSTER:
        return kbo_hub_text("\xeb\xa1\x9c\xec\x8a\xa4\xed\x84\xb0", "ROSTER");
    case KBO_HUB_MILITARY_SUBVIEW_APPLICANTS:
        return kbo_hub_text("\xec\xa7\x80\xec\x9b\x90\xec\x9e\x90", "APPLICANTS");
    case KBO_HUB_MILITARY_SUBVIEW_RESULTS:
        return kbo_hub_text("\xeb\xb0\x9c\xed\x91\x9c \xea\xb2\xb0\xea\xb3\xbc", "RESULTS");
    default:
        return "";
    }
}

static const char* kbo_hub_mod_subnav_label(int index)
{
    switch (index) {
    case KBO_HUB_MOD_SUBVIEW_README:
        return kbo_hub_text("\xeb\xa6\xac\xeb\x93\x9c\xeb\xaf\xb8", "README");
    case KBO_HUB_MOD_SUBVIEW_LICENSE:
        return kbo_hub_text("\xeb\x9d\xbc\xec\x9d\xb4\xec\x84\xa0\xec\x8a\xa4", "LICENSE");
    case KBO_HUB_MOD_SUBVIEW_CREDITS:
        return kbo_hub_text("\xed\x81\xac\xeb\xa0\x88\xeb\x94\xa7", "CREDITS");
    case KBO_HUB_MOD_SUBVIEW_CONTRIBUTIONS:
        return kbo_hub_text("\xea\xb8\xb0\xec\x97\xac", "CONTRIBUTIONS");
    case KBO_HUB_MOD_SUBVIEW_SETTINGS:
        return kbo_hub_text("\xec\x84\xa4\xec\xa0\x95", "SETTINGS");
    default:
        return "";
    }
}
