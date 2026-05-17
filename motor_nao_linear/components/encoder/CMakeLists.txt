idf_component_register(SRCS "encoder.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver freertos esp_driver_pcnt esp_timer esp_driver_gptimer)