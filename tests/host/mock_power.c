#include "mock_power.h"

static void mock_enter_deep_sleep(power_ctrl_t *pc)
{
    mock_power_t *m = (mock_power_t *)pc->backend.ctx;
    m->sleeps++;
    /* DRYRUN semantics: return so the host loop can run the next cycle. */
}

static wake_cause_t mock_wake_cause(power_ctrl_t *pc)
{
    mock_power_t *m = (mock_power_t *)pc->backend.ctx;
    if (m->idx < m->n) {
        return m->causes[m->idx++];
    }
    return WAKE_CAUSE_UNKNOWN;
}

void mock_power_init(mock_power_t *m, power_ctrl_t *pc, power_mode_t mode)
{
    m->n = 0;
    m->idx = 0;
    m->sleeps = 0;

    power_backend_t backend = {
        .enter_deep_sleep = mock_enter_deep_sleep,
        .wake_cause = mock_wake_cause,
        .ctx = m,
    };
    power_init(pc, mode, &backend);
}

void mock_power_push(mock_power_t *m, wake_cause_t cause)
{
    if (m->n < MOCK_POWER_MAX) {
        m->causes[m->n++] = cause;
    }
}
