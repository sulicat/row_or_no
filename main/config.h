#pragma once
#include "nvs_flash.h"
#include "nvs.h"

typedef struct {
    char wifi_ssid[64];
    char wifi_pass[64];
    long update_rate_ms;
    char lake_name[64];  // <-- add this
} app_config_t;

esp_err_t app_config_init(app_config_t *cfg);
esp_err_t app_config_save(const app_config_t *cfg);
