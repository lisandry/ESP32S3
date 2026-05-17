#ifndef ENCODER_H
#define ENCODER_H

#include <stdio.h>
#include <stdint.h>
#include "hal/gpio_types.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define Kc 1.776 // Ganho proporcional
#define Kd 0.095 // Ganho derivativo
#define passo_integrador 0.01 // Passo de tempo para o integrador (10 ms)
#define Gdac 255.0
#define Gadc 1.0/1540.0
#define Tfd 0.0480
#define filtro_derivador (Kd/(Tf+passo_integrador))

#define REFERENCIA_MOTOR 0.25



void func(void);

typedef struct {
    int channel_a_gpio;
    int channel_b_gpio;
    int high_limit;
    int low_limit;
    uint32_t glitch_ns;
} encoder_config_t;

typedef struct {
    encoder_config_t config;
    SemaphoreHandle_t mutex;

    pcnt_unit_handle_t pcnt_unit;
    pcnt_channel_handle_t pcnt_chan;
    gptimer_handle_t gptimer;

    volatile int last_count;
    volatile int delta_pulses;

    float speed;
    int pulses;
} encoder_t;

// Function prototypes
esp_err_t encoder_init(encoder_t *encoder, const encoder_config_t *config);
esp_err_t encoder_set_speed(encoder_t *encoder, float *speed);
esp_err_t encoder_deinit(encoder_t *encoder); 

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ENCODER_H