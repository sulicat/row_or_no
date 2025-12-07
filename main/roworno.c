

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "led_strip.h"
#include "adc_utils.h"
#include <math.h>

#define WIFI_CONNECTED_BIT BIT0
#define LED_PIN 48
#define LED_STRIP_LENGTH 1

static const char *TAG = "http_server";
static EventGroupHandle_t s_wifi_event_group;
const char *url = "http://192.168.1.163:5000/api/new_sensor_data";
char json_payload[1028];
app_config_t config;

static led_strip_handle_t led_strip;

// ---- Thermistor setup ----
#define VCC_MV 3300.0f        // Supply to the divider in mV
#define R_FIXED_OHMS 10000.0f // Fixed series resistor (10k)

#define THERM_R0 10000.0f  // Resistance at T0
#define THERM_T0_C 25.0f   // T0 in °C
#define THERM_BETA 3950.0f // Beta value

// Convert thermistor resistance -> temperature (°C) using Beta equation
static float thermistor_resistance_to_celsius(float r_therm) {
    float t0_k = THERM_T0_C + 273.15f;
    float invT = (1.0f / t0_k) + (1.0f / THERM_BETA) * logf(r_therm / THERM_R0);
    float t_k = 1.0f / invT;
    return t_k - 273.15f;
}

// Read thermistor temperature in °C using our simple_adc_* helpers
static esp_err_t thermistor_read_c(simple_adc_t *adc,
                                   adc_channel_t channel,
                                   float *out_temp_c) {
    int mv = 0;
    esp_err_t err = simple_adc_read_mv(adc, channel, &mv);
    if (err != ESP_OK) {
        return err;
    }

    float v_node = mv / 1000.0f; // node voltage in V
    float vcc = VCC_MV / 1000.0f;
    v_node = vcc - v_node;


    // Guard against impossible readings
    if (v_node <= 0.0f || v_node >= vcc) {
        return ESP_ERR_INVALID_STATE;
    }

    // R_therm = R_fixed * Vnode / (Vcc - Vnode)
    float r_therm = R_FIXED_OHMS * (v_node / (vcc - v_node));

    *out_temp_c = thermistor_resistance_to_celsius(r_therm);
    return ESP_OK;
}

static void rgb_led_init(void) {
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,
        .max_leds = LED_STRIP_LENGTH,
        .led_model = LED_MODEL_WS2812,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz for WS2812
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_ERROR_CHECK(led_strip_clear(led_strip)); // turn off
}

static void rgb_blink(uint8_t r, uint8_t g, uint8_t b, int times, int delay_ms) {
    for (int i = 0; i < times; i++) {
        // LED ON
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, r, g, b));
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(pdMS_TO_TICKS(delay_ms));

        // LED OFF
        ESP_ERROR_CHECK(led_strip_clear(led_strip));
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        rgb_blink(80, 5, 5, 6, 50);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(char *ssid, char *pass) {
    ESP_ERROR_CHECK(nvs_flash_init());

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, pass);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // wait for connection
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);

    ESP_LOGI(TAG, "Connected to WiFi");
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        // ESP_LOGI(TAG, "Response chunk: %.*s", evt->data_len, (char *)evt->data);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void http_post(const char *url, const char *payload) {
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "POST status=%d, content_length=%d", status, len);
    } else {
        ESP_LOGE(TAG, "POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

static void trim_right(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || isspace((unsigned char)s[len - 1]))) {
        s[len - 1] = '\0';
        len--;
    }
}

static void serial_config_task(void *arg) {

    enum {
        WAIT_SSID = 0,
        WAIT_PASS,
        WAIT_RATE,
        WAIT_LAKE // NEW
    } state = WAIT_SSID;

    char line[128];
    size_t pos = 0;
    uint8_t ch;

    printf("\r\n=== ESP32 Simple Config ===\r\n");
    printf("Send 4 lines over serial:\r\n");
    printf("  1) SSID\r\n");
    printf("  2) Password\r\n");
    printf("  3) Update rate (ms)\r\n");
    printf("  4) Lake name\r\n"); // NEW
    printf("Then press Enter after each.\r\n\r\n");

    printf("Current config: ssid='%s', rate=%ldms, lake='%s'\r\n\r\n",
           config.wifi_ssid,
           (long)config.update_rate_ms,
           config.lake_name); // NEW

    while (1) {
        int n = uart_read_bytes(UART_NUM_0, &ch, 1, portMAX_DELAY);
        if (n <= 0) continue;

        if (ch == 13) { // Carriage return
            line[pos] = '\0';
            trim_right(line);

            if (state == WAIT_SSID) {

                strncpy(config.wifi_ssid, line, sizeof(config.wifi_ssid) - 1);
                config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = '\0';
                ESP_LOGI("UARTCONFIG", "SSID set to '%s'", config.wifi_ssid);
                state = WAIT_PASS;

            } else if (state == WAIT_PASS) {

                strncpy(config.wifi_pass, line, sizeof(config.wifi_pass) - 1);
                config.wifi_pass[sizeof(config.wifi_pass) - 1] = '\0';
                ESP_LOGI("UARTCONFIG", "Password set (len=%d)", (int)strlen(config.wifi_pass));
                state = WAIT_RATE;

            } else if (state == WAIT_RATE) {

                config.update_rate_ms = atoi(line);
                if (config.update_rate_ms <= 0) config.update_rate_ms = 1000;
                ESP_LOGI("UARTCONFIG", "Update rate set to %ldms", (long)config.update_rate_ms);
                state = WAIT_LAKE; // NEW

            } else if (state == WAIT_LAKE) { // NEW

                strncpy(config.lake_name, line, sizeof(config.lake_name) - 1);
                config.lake_name[sizeof(config.lake_name) - 1] = '\0';
                ESP_LOGI("UARTCONFIG", "Lake name set to '%s'", config.lake_name);

                // After final field, save config
                if (app_config_save(&config) == ESP_OK) {
                    ESP_LOGI("UARTCONFIG", "Config saved. Rebooting in 1s...");
                    rgb_blink(20, 100, 20, 10, 50);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                } else {
                    ESP_LOGI("UARTCONFIG", "ERROR: Failed to save config.");
                }

                state = WAIT_SSID; // Reset in case reboot doesn't occur
            }

            pos = 0;

        } else if (ch != '\r' && pos < sizeof(line) - 1) {
            line[pos++] = (char)ch;
        }
    }
}

void app_main(void) {
    // Init UART0 (usually already console on dev boards)
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_0, &cfg);
    uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0);

    rgb_led_init();

    app_config_init(&config);

    xTaskCreate(serial_config_task, "serial_cfg", 4096, NULL, 5, NULL);

    wifi_init_sta(config.wifi_ssid, config.wifi_pass);

    simple_adc_t adc;

    // Example: ADC_UNIT_1, channel 0 (GPIO1 on many S3 devkits)
    simple_adc_init(&adc,
                    ADC_UNIT_1,
                    ADC_CHANNEL_0,
                    ADC_ATTEN_DB_11); // up to ~3.3V on S3


    while (1) {
        float temp_c = 0.0f;
        esp_err_t err = thermistor_read_c(&adc, ADC_CHANNEL_0, &temp_c);

        if (err == ESP_OK) {
            printf("Temperature: %.2f °C\n", temp_c);
        } else {
            printf("Thermistor read error: %s\n", esp_err_to_name(err));
        }

        sprintf(json_payload, "{\"lake_name\":\"%s\",\"temprature\":%f}", config.lake_name, temp_c);
        http_post(url, json_payload);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
