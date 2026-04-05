idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES LED_RGB driver freertos)