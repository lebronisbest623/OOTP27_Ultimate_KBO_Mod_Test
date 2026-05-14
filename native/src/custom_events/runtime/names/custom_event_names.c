#include "../common/custom_events_common.h"
#include "custom_event_names.h"

#include <stdio.h>
#include <string.h>

#include "../../../core/news/templates/core_news_templates.h"
#include "../../../core/core_flags/api/flags_api.h"
#include "../../../core/dates/core_text_date.h"
#include "../../../core/text/ootp_text_encoding.h"

#define KBO_CUSTOM_EVENT_TITLE_MAX 160u
#define KBO_CUSTOM_EVENT_LANGUAGE_COUNT 2u

typedef struct KboCustomEventDefinition {
    KboCustomEventKind kind;
    const char* key;
    const char* legacy_title;
} KboCustomEventDefinition;

static const KboCustomEventDefinition g_kbo_custom_event_definitions[] = {
    {
        KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN,
        "custom_event.foreign_priority.open.title",
        g_kbo_foreign_priority_open_event_title,
    },
    {
        KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE,
        "custom_event.foreign_priority.close.title",
        g_kbo_foreign_priority_close_event_title,
    },
    {
        KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION,
        "custom_event.military.selection.title",
        g_kbo_military_selection_event_title,
    },
    {
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION,
        "custom_event.asian_games.selection.title",
        g_kbo_asian_games_selection_event_title,
    },
    {
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE,
        "custom_event.asian_games.departure.title",
        g_kbo_asian_games_departure_event_title,
    },
    {
        KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL,
        "custom_event.asian_games.final.title",
        g_kbo_asian_games_final_event_title,
    },
    {
        KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE,
        "custom_event.cbt.exception_deadline.title",
        g_kbo_cbt_exception_deadline_event_title,
    },
    {
        KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT,
        "custom_event.cbt.announcement.title",
        g_kbo_cbt_announcement_event_title,
    },
};

static INIT_ONCE g_kbo_custom_event_titles_once = INIT_ONCE_STATIC_INIT;
static char g_kbo_custom_event_titles[KBO_CUSTOM_EVENT_KIND_COUNT][KBO_CUSTOM_EVENT_LANGUAGE_COUNT][KBO_CUSTOM_EVENT_TITLE_MAX];

static const KboCustomEventDefinition* kbo_custom_event_definition_for_kind(KboCustomEventKind kind)
{
    for (size_t i = 0u; i < sizeof(g_kbo_custom_event_definitions) / sizeof(g_kbo_custom_event_definitions[0]); i++) {
        if (g_kbo_custom_event_definitions[i].kind == kind) {
            return &g_kbo_custom_event_definitions[i];
        }
    }
    return NULL;
}

static int kbo_custom_event_language_index_from_dir(const char* language_dir)
{
    return language_dir != NULL && strcmp(language_dir, "en") == 0 ? 1 : 0;
}

static const char* kbo_custom_event_language_dir_from_index(int language_index)
{
    return language_index == 1 ? "en" : "ko";
}

static BOOL CALLBACK kbo_custom_event_titles_init_once(
    PINIT_ONCE init_once,
    PVOID parameter,
    PVOID* context)
{
    (void)init_once;
    (void)parameter;
    (void)context;

    for (size_t i = 0u; i < sizeof(g_kbo_custom_event_definitions) / sizeof(g_kbo_custom_event_definitions[0]); i++) {
        const KboCustomEventDefinition* definition = &g_kbo_custom_event_definitions[i];
        if (definition->kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || definition->kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
            continue;
        }

        for (int language_index = 0; language_index < (int)KBO_CUSTOM_EVENT_LANGUAGE_COUNT; language_index++) {
            char* title = g_kbo_custom_event_titles[definition->kind][language_index];
            const char* language_dir = kbo_custom_event_language_dir_from_index(language_index);
            title[0] = '\0';
            if (!kbo_news_template_load_for_language(
                    language_dir,
                    definition->key,
                    title,
                    KBO_CUSTOM_EVENT_TITLE_MAX,
                    NULL,
                    0u,
                    "custom_event_title")) {
                snprintf(title, KBO_CUSTOM_EVENT_TITLE_MAX, "%s", definition->legacy_title);
            }
        }
    }

    return TRUE;
}

static void kbo_custom_event_ensure_title_cache(void)
{
    InitOnceExecuteOnce(
        &g_kbo_custom_event_titles_once,
        kbo_custom_event_titles_init_once,
        NULL,
        NULL);
}

const char* kbo_custom_event_legacy_title_for_kind(KboCustomEventKind kind)
{
    const KboCustomEventDefinition* definition = kbo_custom_event_definition_for_kind(kind);
    return definition != NULL ? definition->legacy_title : "";
}

int kbo_custom_event_title_for_kind(KboCustomEventKind kind, char* out, size_t out_size)
{
    if (out == NULL || out_size == 0u || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }
    out[0] = '\0';

    kbo_custom_event_ensure_title_cache();

    int language_index = kbo_custom_event_language_index_from_dir(kbo_custom_news_language_dir());
    snprintf(out, out_size, "%s", g_kbo_custom_event_titles[kind][language_index]);
    return out[0] != '\0';
}

int kbo_custom_event_name_equals_title(const char* name, const char* title)
{
    if (name == NULL || title == NULL || name[0] == '\0' || title[0] == '\0') {
        return 0;
    }
    if (strcmp(name, title) == 0 || ascii_equals_ignore_case(name, title)) {
        return 1;
    }

    char* internal_title = kbo_alloc_ootp_internal_text(title);
    int matches = internal_title != NULL
        && (strcmp(name, internal_title) == 0 || ascii_equals_ignore_case(name, internal_title));
    kbo_free_ootp_internal_text(internal_title);
    return matches;
}

int kbo_custom_event_name_is_kind(const char* name, KboCustomEventKind kind)
{
    if (name == NULL || name[0] == '\0' || kind <= KBO_CUSTOM_EVENT_KIND_UNKNOWN || kind >= KBO_CUSTOM_EVENT_KIND_COUNT) {
        return 0;
    }

    kbo_custom_event_ensure_title_cache();

    for (int language_index = 0; language_index < (int)KBO_CUSTOM_EVENT_LANGUAGE_COUNT; language_index++) {
        if (kbo_custom_event_name_equals_title(name, g_kbo_custom_event_titles[kind][language_index])) {
            return 1;
        }
    }

    return kbo_custom_event_name_equals_title(name, kbo_custom_event_legacy_title_for_kind(kind));
}

int kbo_custom_event_name_is_open(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_OPEN);
}

int kbo_custom_event_name_is_close(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_FOREIGN_PRIORITY_CLOSE);
}

int kbo_custom_event_name_is_military_selection(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_MILITARY_SELECTION);
}

int kbo_custom_event_name_is_asian_games_selection(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_SELECTION);
}

int kbo_custom_event_name_is_asian_games_departure(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_DEPARTURE);
}

int kbo_custom_event_name_is_asian_games_final(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_ASIAN_GAMES_FINAL);
}

int kbo_custom_event_name_is_cbt_exception_deadline(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_CBT_EXCEPTION_DEADLINE);
}

int kbo_custom_event_name_is_cbt_announcement(const char* name)
{
    return kbo_custom_event_name_is_kind(name, KBO_CUSTOM_EVENT_KIND_CBT_ANNOUNCEMENT);
}
