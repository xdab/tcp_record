#ifndef TSQL_H
#define TSQL_H

typedef struct
{
    double tone;
    int sample_rate;

    /* NCO phasor for mixing down to DC */
    double nco_r;
    double nco_i;
    double step_r;
    double step_i;

    /* attack/release envelope of the mixed-down tone */
    float env_r;
    float env_i;
    float alpha_up;
    float alpha_dn;

    /* 10 ms tick accumulator: mean envelope amplitude */
    double tick_sum;
    int tick_count;
    int tick_size;
    float tick_dbfs;

    /* hysteresis state: instant open above level, close after the
     * output has stayed below level for delay_ms */
    int open;
    int open_idx;
    int close_ms;
    float level_dbfs;
    float delay_ms;
} tsql_state_t;

void tsql_init(tsql_state_t *t, double tone, int sample_rate,
               float level_dbfs, float delay_ms);
void tsql_process(tsql_state_t *t, const float *in, int len);
int tsql_open(const tsql_state_t *t);

#endif /* TSQL_H */
