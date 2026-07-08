/* Mock power backend for host tests.
 *
 * Simulates the deep-sleep -> reboot -> wake cycle deterministically:
 *   - a queue of wake causes is delivered one per cycle by wake_cause();
 *   - enter_deep_sleep() simply counts the sleep and returns (DRYRUN semantics).
 */
#ifndef MOCK_POWER_H
#define MOCK_POWER_H

#include "core/power.h"

#define MOCK_POWER_MAX 32

typedef struct {
    wake_cause_t causes[MOCK_POWER_MAX];
    int  n;      /* number of causes queued */
    int  idx;    /* next cause index */
    int  sleeps; /* enter_deep_sleep call count */
} mock_power_t;

/* Initialize the mock and bind it into `pc` at the given mode. */
void mock_power_init(mock_power_t *m, power_ctrl_t *pc, power_mode_t mode);

/* Queue a wake cause to be delivered on a future cycle. */
void mock_power_push(mock_power_t *m, wake_cause_t cause);

#endif /* MOCK_POWER_H */
