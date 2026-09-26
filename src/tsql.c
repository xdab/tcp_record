#include <math.h>

#include "tsql.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* gate decision cadence */
#define TSQL_TICK_MS 10

/* lock-in integration time constant (attack = release) */
#define TSQL_TAU 0.25f

void tsql_init(tsql_state_t *t, double tone, int sample_rate,
               float level_dbfs, float delay_ms)
{
    t->tone = tone;
    t->sample_rate = sample_rate;

    double w = 2.0 * M_PI * tone / (double)sample_rate;
    t->step_r = cos(w);
    t->step_i = sin(w);
    t->nco_r = 1.0;
    t->nco_i = 0.0;

    t->alpha_up = 1.0f / ((float)sample_rate * TSQL_TAU);
    t->alpha_dn = t->alpha_up;
    t->env_r = 0.0f;
    t->env_i = 0.0f;

    t->tick_size = sample_rate * TSQL_TICK_MS / 1000;
    if (t->tick_size < 1)
        t->tick_size = 1;
    t->tick_sum = 0.0;
    t->tick_count = 0;
    t->tick_dbfs = -200.0f;

    t->open = 0;
    t->open_idx = 0;
    t->close_ms = 0;
    t->level_dbfs = level_dbfs;
    t->delay_ms = delay_ms;
}

static void tick_complete(tsql_state_t *t, int i)
{
    float amp = (float)(2.0 * t->tick_sum / t->tick_size);
    t->tick_dbfs = 20.0f * log10f(amp + 1e-12f);
    t->tick_sum = 0.0;
    t->tick_count = 0;

    if (t->open)
    {
        if (t->tick_dbfs < t->level_dbfs)
        {
            t->close_ms += TSQL_TICK_MS;
            if (t->close_ms >= t->delay_ms)
            {
                t->open = 0;
                t->close_ms = 0;
            }
        }
        else
        {
            t->close_ms = 0;
        }
        return;
    }

    if (t->tick_dbfs > t->level_dbfs)
    {
        t->open = 1;
        t->open_idx = i;
        t->close_ms = 0;
    }
}

void tsql_process(tsql_state_t *t, const float *in, int len)
{
    for (int i = 0; i < len; i++)
    {
        double pr = t->nco_r;
        double pi = t->nco_i;
        t->nco_r = pr * t->step_r - pi * t->step_i;
        t->nco_i = pi * t->step_r + pr * t->step_i;

        float zr = in[i] * (float)pr;
        float zi = in[i] * (float)(-pi);
        float ms = sqrtf(zr * zr + zi * zi);
        float ma = sqrtf(t->env_r * t->env_r + t->env_i * t->env_i);
        float al = (ms > ma) ? t->alpha_up : t->alpha_dn;
        t->env_r += al * (zr - t->env_r);
        t->env_i += al * (zi - t->env_i);

        t->tick_sum += sqrtf(t->env_r * t->env_r + t->env_i * t->env_i);
        if (++t->tick_count >= t->tick_size)
            tick_complete(t, i);
    }
}

int tsql_open(const tsql_state_t *t)
{
    return t->open;
}
