#include <math.h>
#include <string.h>

#include "squelch.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void squelch_init(squelch_state_t *s, squelch_mode_t mode, int sample_rate,
                  float open_threshold, float close_threshold)
{
    memset(s, 0, sizeof(squelch_state_t));
    s->mode = mode;
    s->open = 1;

    /* high-pass filter: 1st order IIR, cutoff ~3 kHz */
    float hp_freq = 3000.0f;
    float rc = 1.0f / (2.0f * (float)M_PI * hp_freq);
    float dt = 1.0f / (float)sample_rate;
    s->hp_coef = rc / (rc + dt);

    /* envelope follower: fast attack, slow decay */
    float attack_ms = 2.0f;
    float decay_ms = 100.0f;
    s->env_attack = expf(-1.0f / ((attack_ms / 1000.0f) * (float)sample_rate));
    s->env_decay = expf(-1.0f / ((decay_ms / 1000.0f) * (float)sample_rate));

    /* bandwidth normalization: scale envelope to [0,1] for white noise */
    float nyquist = (float)sample_rate / 2.0f;
    s->norm_factor = 4.0f * sqrtf(nyquist / (nyquist - hp_freq));

    /* threshold + hysteresis */
    s->open_threshold = open_threshold;
    s->close_threshold = close_threshold;
}

static inline float high_pass(squelch_state_t *s, float in)
{
    float out = s->hp_coef * (s->hp_state + in - s->hp_prev);
    s->hp_prev = in;
    s->hp_state = out;
    return out;
}

int squelch_process(squelch_state_t *s, const float *in, float *out, int len)
{
    for (int i = 0; i < len; i++)
    {
        /* high-pass filter to isolate noise */
        float noise = high_pass(s, in[i]);

        /* full-wave rectify */
        float rect = fabsf(noise);

        /* envelope follower: fast attack, slow decay */
        float coeff = (rect > s->envelope) ? s->env_attack : s->env_decay;
        s->envelope = coeff * s->envelope + (1.0f - coeff) * rect;

        /* gate audio */
        out[i] = s->open ? in[i] : 0.0f;
    }

    /* threshold + hysteresis (per-block, uses bandwidth-normalized envelope) */
    float norm_env = s->envelope * s->norm_factor;
    if (s->open)
    {
        if (norm_env > s->close_threshold)
            s->open = 0;
    }
    else
    {
        if (norm_env < s->open_threshold)
            s->open = 1;
    }

    return s->open;
}
