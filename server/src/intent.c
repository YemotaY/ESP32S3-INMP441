#include "server/intent.h"

#include <ctype.h>
#include <string.h>

/* Case-insensitive search for `needle` as a substring of `haystack`. */
static int contains_ci(const char *haystack, const char *needle, size_t nlen)
{
    if (nlen == 0) {
        return 1;
    }
    for (const char *h = haystack; *h; h++) {
        size_t i = 0;
        while (i < nlen && h[i] &&
               tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == nlen) {
            return 1;
        }
    }
    return 0;
}

/* True if every space-separated keyword in `keywords` is present in `transcript`. */
static int all_keywords_present(const char *transcript, const char *keywords)
{
    const char *p = keywords;
    while (*p) {
        while (*p == ' ') {
            p++;
        }
        const char *start = p;
        while (*p && *p != ' ') {
            p++;
        }
        size_t klen = (size_t)(p - start);
        if (klen > 0 && !contains_ci(transcript, start, klen)) {
            return 0;
        }
    }
    return 1;
}

int intent_match(const intent_table_t *table, const char *transcript,
                 const char **out_name)
{
    if (out_name) {
        *out_name = NULL;
    }
    if (!table || !transcript) {
        return INTENT_NONE;
    }
    for (size_t i = 0; i < table->count; i++) {
        const intent_entry_t *e = &table->entries[i];
        if (e->keywords && all_keywords_present(transcript, e->keywords)) {
            if (out_name) {
                *out_name = e->name;
            }
            return e->id;
        }
    }
    return INTENT_NONE;
}
