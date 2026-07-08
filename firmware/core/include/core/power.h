/* Power / sleep abstraction.
 *
 * Models the deep-sleep -> wake cycle behind a backend interface so the whole
 * mission can be developed and unit-tested on a host with no real power-down.
 *
 * Modes:
 *   STAY_AWAKE - never sleeps; enter_deep_sleep is a no-op (initial development).
 *   DRYRUN     - simulates a sleep->wake reboot: records the sleep, then the next
 *                wake_cause() delivers the next scheduled cause, and returns.
 *   REAL       - device backend calls esp_deep_sleep_start(); does not return.
 */
#ifndef CORE_POWER_H
#define CORE_POWER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    POWER_MODE_STAY_AWAKE = 0,
    POWER_MODE_DRYRUN,
    POWER_MODE_REAL,
} power_mode_t;

typedef enum {
    WAKE_CAUSE_UNKNOWN = 0,
    WAKE_CAUSE_POWER_ON,       /* cold boot */
    WAKE_CAUSE_SOUND_TRIGGER,  /* analog trigger via ULP / EXT1 */
    WAKE_CAUSE_BUTTON,         /* manual wake */
    WAKE_CAUSE_TIMER,          /* periodic timer wake */
} wake_cause_t;

typedef struct power_ctrl power_ctrl_t;

typedef struct {
    /* Perform or simulate deep sleep. Returns in DRYRUN; must not return in REAL. */
    void (*enter_deep_sleep)(power_ctrl_t *pc);
    /* Report the wake cause for the current cycle. */
    wake_cause_t (*wake_cause)(power_ctrl_t *pc);
    void *ctx; /* backend private state */
} power_backend_t;

struct power_ctrl {
    power_mode_t    mode;
    power_backend_t backend;
    uint32_t        sleep_count;       /* times sleep was actually entered/simulated */
    uint32_t        false_wake_count;  /* rejected wakes (bookkeeping) */
};

void power_init(power_ctrl_t *pc, power_mode_t mode, const power_backend_t *backend);

/* Wake cause for the current cycle. STAY_AWAKE with no backend reports POWER_ON. */
wake_cause_t power_wake_cause(power_ctrl_t *pc);

/* Request deep sleep.
 *   STAY_AWAKE: no-op, returns true.
 *   DRYRUN:     simulates sleep via backend, returns true.
 *   REAL:       calls backend (does not return on device).
 * Returns true if control returned to the caller. */
bool power_enter_deep_sleep(power_ctrl_t *pc);

void power_note_false_wake(power_ctrl_t *pc);

const char *power_wake_cause_str(wake_cause_t c);
const char *power_mode_str(power_mode_t m);

#endif /* CORE_POWER_H */
