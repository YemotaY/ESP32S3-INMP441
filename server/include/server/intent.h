/* Intent matching: map a recognised transcript to a command intent.
 *
 * A tiny, allocation-free keyword matcher over a caller-provided table. Matching is
 * case-insensitive and token/substring based so "please turn on the light" resolves to
 * LIGHT_ON. Pure logic, host-tested in tests/host/test_intent.c.
 */
#ifndef SERVER_INTENT_H
#define SERVER_INTENT_H

#include <stddef.h>

/* One intent: an id plus the keyword phrase that triggers it. All keywords in a phrase
 * (space-separated) must appear in the transcript for the intent to match. */
typedef struct {
    int         id;        /* caller-defined intent id (>= 0) */
    const char *name;      /* human-readable label (for the dashboard/logs) */
    const char *keywords;  /* space-separated required keywords, lowercase */
} intent_entry_t;

#define INTENT_NONE (-1)

typedef struct {
    const intent_entry_t *entries;
    size_t                count;
} intent_table_t;

/* Return the id of the first matching intent in `table`, or INTENT_NONE.
 * If `out_name` is non-NULL it receives the matched entry's name (or NULL). */
int intent_match(const intent_table_t *table, const char *transcript,
                 const char **out_name);

#endif /* SERVER_INTENT_H */
