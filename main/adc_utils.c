#include "adc_utils.h"
#include "esp_log.h"

static const char *TAG = "SIMPLE_ADC";

esp_err_t simple_adc_init(simple_adc_t *adc,
                          adc_unit_t unit,
                          adc_channel_t channel,
                          adc_atten_t atten) {
    if (!adc) return ESP_ERR_INVALID_ARG;

    esp_err_t err;

    // --------------------------------------------------
    // 1) Create ADC oneshot unit
    // --------------------------------------------------
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = unit,
    };
    err = adc_oneshot_new_unit(&unit_cfg, &adc->unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        return err;
    }

    // --------------------------------------------------
    // 2) Configure channel
    // --------------------------------------------------
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = atten, // for S3: typically ADC_ATTEN_DB_11
    };
    err = adc_oneshot_config_channel(adc->unit, channel, &chan_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel failed: %s", esp_err_to_name(err));
        return err;
    }

    // --------------------------------------------------
    // 3) Set up calibration (curve fitting for ESP32-S3)
    // --------------------------------------------------
    adc->calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };

    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &adc->cali_handle);
    if (err == ESP_OK) {
        adc->calibrated = true;
        ESP_LOGI(TAG, "ADC calibration (curve fitting) enabled");
    } else {
        ESP_LOGW(TAG, "ADC curve-fitting calibration not available: %s", esp_err_to_name(err));
    }

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = unit,
        .chan = channel,
        .atten = atten,
        .bitwidth = ADC_BITWIDTH_12,
    };

    err = adc_cali_create_scheme_line_fitting(&cali_cfg, &adc->cali_handle);
    if (err == ESP_OK) {
        adc->calibrated = true;
        ESP_LOGI(TAG, "ADC calibration (line fitting) enabled");
    } else {
        ESP_LOGW(TAG, "ADC line-fitting calibration not available: %s", esp_err_to_name(err));
    }
#else
    ESP_LOGW(TAG, "No ADC calibration scheme supported on this target");
#endif

    return ESP_OK;
}

esp_err_t simple_adc_read_mv(simple_adc_t *adc,
                             adc_channel_t channel,
                             int *out_mv) {
    if (!adc || !out_mv) return ESP_ERR_INVALID_ARG;

    int raw = 0;
    esp_err_t err = adc_oneshot_read(adc->unit, channel, &raw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_read failed: %s", esp_err_to_name(err));
        return err;
    }

    if (adc->calibrated) {
        err = adc_cali_raw_to_voltage(adc->cali_handle, raw, out_mv);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "adc_cali_raw_to_voltage failed: %s", esp_err_to_name(err));
            // fall back to rough calc
            *out_mv = (int)((raw / 4095.0f) * 3300.0f);
        }
    } else {
        // Rough conversion assuming 0–3.3V range
        *out_mv = (int)((raw / 4095.0f) * 3300.0f);
    }

    return ESP_OK;
}
