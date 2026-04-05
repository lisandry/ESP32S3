#ifndef DRV8833_H
#define DRV8833_H

#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

// Strtuct usada para a configuração do DRV8833 para as pontes A e B
typedef struct {
    gpio_num_t  AIN1_pin; //Bridge A input 1
    gpio_num_t  AIN2_pin; //Bridge A input 2
    gpio_num_t  BIN1_pin; //Bridge B input 1
    gpio_num_t  BIN2_pin; //Bridge B input 2

    ledc_channel_t AIN1_channel;    // LEDC channel for Bridge A input 1
    ledc_channel_t AIN2_channel;    // LEDC channel for Bridge A input 2
    ledc_channel_t BIN1_channel;    // LEDC channel for Bridge B input 1
    ledc_channel_t BIN2_channel;    // LEDC channel for Bridge B input 2

    ledc_timer_t timer_num;       // LEDC timer number
    uint32_t duty_resolution; // LEDC duty resolution
    uint32_t pwm_freq; // Frequência PWM

} drv8833_config_t;

typedef struct{
    drv8833_config_t config; 
    SemaphoreHandle_t mutex; 
}drv8833_t;


//Funçoes do API
/**
 * @brief Inicialização do DRV8833
 */

esp_err_t drv8833_init(drv8833_t *motor, const drv8833_config_t *config);


 /**
 * @brief Define a velocidade e direção do Motor A.
 */
esp_err_t drv8833_set_motor_a(drv8833_t *motor, int32_t velocidade_A);

/**
* @brief Define a velocidade e direção do Motor B.
*/

esp_err_t drv8833_set_motor_b(drv8833_t *motor, int32_t velocidade_B);

/**
 * @brief Parada de emergencia.
 */
esp_err_t drv8833_stop(drv8833_t *motor);

#ifdef __cplusplus
}   
#endif

#endif // DRV8833_H