idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_timer freertos esp_driver_pcnt esp_driver_gptimer)
