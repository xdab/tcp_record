#ifndef CALIBRATION_H
#define CALIBRATION_H

#define CAL_HIST_BUCKETS 20
#define CAL_HIST_BUCKET_WIDTH 0.025f

typedef struct
{
    int hist[CAL_HIST_BUCKETS];
    int total_samples;
    int interval_samples;
    int sample_rate;
} cal_state_t;

void cal_init(cal_state_t *c, int sample_rate);
void cal_accumulate(cal_state_t *c, float envelope, int n);
int cal_should_print(cal_state_t *c);
void cal_print(cal_state_t *c);

#endif /* CALIBRATION_H */
