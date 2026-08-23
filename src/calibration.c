#include <math.h>
#include <stdio.h>
#include <string.h>

#include "calibration.h"

void cal_init(cal_state_t *c, int sample_rate)
{
    memset(c, 0, sizeof(cal_state_t));
    c->sample_rate = sample_rate;
}

void cal_accumulate(cal_state_t *c, float envelope, int n)
{
    int b = (int)(envelope / CAL_HIST_BUCKET_WIDTH);
    if (b < 0) b = 0;
    if (b >= CAL_HIST_BUCKETS) b = CAL_HIST_BUCKETS - 1;
    c->hist[b] += n;
    c->total_samples += n;
    c->interval_samples += n;
}

int cal_should_print(cal_state_t *c)
{
    return c->interval_samples >= c->sample_rate;
}

void cal_print(cal_state_t *c)
{
    int max_bar = 50;

    fprintf(stderr, "\033[2J\033[H");
    for (int i = 0; i < CAL_HIST_BUCKETS; i++)
    {
        float lo = i * CAL_HIST_BUCKET_WIDTH;
        float hi = lo + CAL_HIST_BUCKET_WIDTH;
        fprintf(stderr, " %5.2f-%.2f: ", lo, hi);
        if (c->hist[i] > 0)
        {
            float logval = logf((float)c->hist[i]) / logf(5.0f);
            int bars = (int)((logval - 6.0f) * 10.0f);
            if (bars < 1) bars = 1;
            if (bars > max_bar) bars = max_bar;
            for (int j = 0; j < bars; j++)
                fputc('#', stderr);
            fprintf(stderr, " %d", c->hist[i]);
        }
        fputc('\n', stderr);
    }
    c->interval_samples = 0;
}
