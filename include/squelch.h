#ifndef SQUELCH_H
#define SQUELCH_H

typedef enum
{
    SQUELCH_FM,
    SQUELCH_AM
} squelch_mode_t;

typedef struct
{
    squelch_mode_t mode;

    /* high-pass filter state (1st order IIR) */
    float hp_prev;
    float hp_state;
    float hp_coef;

    /* envelope detector state */
    float envelope;
    float env_attack;       /* fast attack coefficient */
    float env_decay;        /* slow decay coefficient */

    /* envelope normalization */
    float env_norm;         /* 1.0 / model_prediction(sbw, ro), 0 = disabled */

    /* threshold + hysteresis */
    int open;
    float open_threshold;
    float close_threshold;
} squelch_state_t;

void squelch_init(squelch_state_t *s, squelch_mode_t mode, int sample_rate,
                  float open_threshold, float close_threshold, int signal_bw);
int squelch_process(squelch_state_t *s, const float *in, float *out, int len);

#endif /* SQUELCH_H */
