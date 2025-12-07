#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

typedef struct {
    adc_oneshot_unit_handle_t unit;
    adc_cali_handle_t         cali_handle;
    bool                      calibrated;
} simple_adc_t;

// Initialize ADC oneshot + calibration for one unit/channel
esp_err_t simple_adc_init(simple_adc_t *adc,
                          adc_unit_t unit,
                          adc_channel_t channel,
                          adc_atten_t atten);

// Read a channel and return voltage in millivolts
esp_err_t simple_adc_read_mv(simple_adc_t *adc,
                             adc_channel_t channel,
                             int *out_mv);
