idf_component_register(SRCS "RTOS.c"
                    INCLUDE_DIRS "include"
                    REQUIRES bmp280 freertos esp_timer driver 
                    file_system timers_rtos esp_adc)
