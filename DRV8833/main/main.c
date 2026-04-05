#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "DRV8833.h"

// Definição dos pinos
#define PIN_MOTOR_A_IN1 GPIO_NUM_4
#define PIN_MOTOR_A_IN2 GPIO_NUM_5
#define PIN_MOTOR_B_IN1 GPIO_NUM_6
#define PIN_MOTOR_B_IN2 GPIO_NUM_7

 drv8833_config_t config_motor = {
        .AIN1_pin = PIN_MOTOR_A_IN1,
        .AIN2_pin = PIN_MOTOR_A_IN2,
        .BIN1_pin = PIN_MOTOR_B_IN1,
        .BIN2_pin = PIN_MOTOR_B_IN2,

        // Canais do LEDC
        .AIN1_channel = LEDC_CHANNEL_0,
        .AIN2_channel = LEDC_CHANNEL_1,
        .BIN1_channel = LEDC_CHANNEL_2,
        .BIN2_channel = LEDC_CHANNEL_3,
    
    // Timer e Resolução
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT, // Resolução de 10 bits (0 a 1023)
        .pwm_freq = 1000 // Frequência do PWM em Hz
    };

drv8833_t motor;



void app_main(void)
{
    esp_err_t err = drv8833_init(&motor, &config_motor);

    if (err != ESP_OK){
        ESP_LOGI("erro", "Erro ao inicializar o DRV8833: %d", err);
    }

    for(;;){
        // --- TESTE 1: Acelerar os dois motores para FRENTE (75%) ---
        drv8833_set_motor_a(&motor, -500);
        //drv8833_set_motor_b(&motor, 0);
        vTaskDelay(pdMS_TO_TICKS(5000)); // Aguarda 5 segundos

       
       // --- TESTE 2: Parada de emergência ---
        //drv8833_stop(&motor);
       // vTaskDelay(pdMS_TO_TICKS(5000));

        // --- TESTE 3: Dar RÉ nos dois motores (50%) ---
        drv8833_set_motor_a(&motor, 500);
        //drv8833_set_motor_b(&motor, 50);
        vTaskDelay(pdMS_TO_TICKS(5000)); // Aguarda 5 segundos

        // --- TESTE 4: Girar no próprio eixo (Um motor para frente, outro para trás) ---
        //drv8833_set_motor_a(&motor, 0);  // A para frente
        //drv8833_set_motor_b(&motor, 500); // B para trás
       // vTaskDelay(pdMS_TO_TICKS(5000)); // Aguarda 5 segundos
        
        // Para novamente antes de recomeçar o loop
        drv8833_stop(&motor);
        //drv8833_set_motor_a(&motor, 0);
       vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
