#include <math.h>
#include <string.h>

#include "squelch.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void squelch_init(squelch_state_t *s, squelch_mode_t mode, int sample_rate,
                  float open_threshold, float close_threshold, int signal_bw)
{
    memset(s, 0, sizeof(squelch_state_t));
    s->mode = mode;
    s->open = 1;

    /* high-pass filter: 1st order IIR, cutoff ~3.2 kHz */
    float hp_freq = 3200.0f;
    float rc = 1.0f / (2.0f * (float)M_PI * hp_freq);
    float dt = 1.0f / (float)sample_rate;
    s->hp_coef = rc / (rc + dt);

    /* envelope follower: fast attack, slow decay */
    float attack_ms = 2.0f;
    float decay_ms = 100.0f;
    s->env_attack = expf(-1.0f / ((attack_ms / 1000.0f) * (float)sample_rate));
    s->env_decay = expf(-1.0f / ((decay_ms / 1000.0f) * (float)sample_rate));

    /* envelope normalization: model = a * ratio^b / (1 + ratio^c) * (ro/C)^d
     * ratio = 2*sbw/ro, C = 3200
     * a=0.4827, b=0.6272, c=0.7769, d=0.2916 */
    s->env_norm = 0.0f;
    if (signal_bw > 0)
    {
        float ratio = 2.0f * (float)signal_bw / (float)sample_rate;
        if (ratio < 0.001f) ratio = 0.001f;
        if (ratio > 10.0f) ratio = 10.0f;
        float model = 0.4827f
                      * powf(ratio, 0.6272f)
                      / (1.0f + powf(ratio, 0.7769f))
                      * powf((float)sample_rate / hp_freq, 0.2916f);
        if (model > 0.0f)
            s->env_norm = 1.5f / model;
    }

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

        /* threshold check per-sample */
        float env = s->envelope;
        if (s->env_norm > 0.0f)
            env *= s->env_norm;

        if (s->open)
        {
            if (env > s->close_threshold)
                s->open = 0;
        }
        else
        {
            if (env < s->open_threshold)
            {
                s->open = 1;
                s->open_idx = i;
            }
        }

        /* gate audio */
        out[i] = s->open ? in[i] : 0.0f;
    }

    return s->open;
}
