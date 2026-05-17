idf_component_register(SRCS "timers_rtos.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_timer freertos)
