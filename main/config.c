#include "config.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "NON VOL STORAGE";
static const char *NVS_NS = "config";

esp_err_t app_config_init(app_config_t *cfg) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // defaults
    memset(cfg, 0, sizeof(*cfg));
    cfg->update_rate_ms = 1000;
    cfg->wifi_ssid[0] = '\0';
    cfg->wifi_pass[0] = '\0';
    cfg->lake_name[0] = '\0'; // default empty lake name

    nvs_handle_t h;
    err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No config in NVS, using defaults");
        return ESP_OK;
    } else if (err != ESP_OK) {
        return err;
    }

    size_t len;

    // SSID
    len = sizeof(cfg->wifi_ssid);
    if (nvs_get_str(h, "ssid", cfg->wifi_ssid, &len) != ESP_OK) {
        cfg->wifi_ssid[0] = '\0';
    }

    // PASS
    len = sizeof(cfg->wifi_pass);
    if (nvs_get_str(h, "pass", cfg->wifi_pass, &len) != ESP_OK) {
        cfg->wifi_pass[0] = '\0';
    }

    // UPDATE RATE
    int32_t tmp;
    if (nvs_get_i32(h, "update_rate_ms", &tmp) == ESP_OK) {
        cfg->update_rate_ms = tmp;
    }

    // LAKE NAME (NEW)
    len = sizeof(cfg->lake_name);
    if (nvs_get_str(h, "lake_name", cfg->lake_name, &len) != ESP_OK) {
        cfg->lake_name[0] = '\0';
    }

    nvs_close(h);

    ESP_LOGI(TAG,
             "Loaded config: ssid='%s' (len=%d), update_rate_ms=%ld, lake='%s'",
             cfg->wifi_ssid,
             (int)strlen(cfg->wifi_ssid),
             (long)cfg->update_rate_ms,
             cfg->lake_name);

    return ESP_OK;
}

esp_err_t app_config_save(const app_config_t *cfg) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    ESP_ERROR_CHECK(nvs_set_str(h, "ssid", cfg->wifi_ssid));
    ESP_ERROR_CHECK(nvs_set_str(h, "pass", cfg->wifi_pass));
    ESP_ERROR_CHECK(nvs_set_i32(h, "update_rate_ms", cfg->update_rate_ms));
    ESP_ERROR_CHECK(nvs_set_str(h, "lake_name", cfg->lake_name)); // NEW

    err = nvs_commit(h);
    nvs_close(h);
    return err;
}
