idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES freertos driver esp_timer esp_driver_pcnt esp_driver_gptimer)
