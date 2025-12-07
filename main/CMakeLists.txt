idf_component_register(SRCS "roworno.c" "config.c" "adc_utils.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        freertos
                        esp_wifi
                        esp_event
                        esp_http_client
                        nvs_flash
                        driver
                        esp_adc
                        )
