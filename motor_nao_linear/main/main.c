#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "encoder.h"
#include "driver/ledc.h"
#include "driver/gpio.h" // Faltava essa biblioteca!

static const char *TAG = "MAIN";

#define PCNT_CHAN_GPIO_A 6
#define PCNT_CHAN_GPIO_B 7

#define PINO_MOTOR_PWM   25 // Pino que vai para o ENA/ENB da Ponte H
#define PINO_MOTOR_DIR1  GPIO_NUM_4 // Pino IN1 da Ponte H
#define PINO_MOTOR_DIR2  GPIO_NUM_5 // Pino IN2 da Ponte H

extern float referencia;
extern encoder_t meuencoder;
extern int e0;
extern float erro;
//extern float Kp;//, Kp;


#define Kc 1.776 // Ganho proporcional
#define Kd 0.095 // Ganho derivativo
#define passo_integrador 0.01 // Passo de tempo para o integrador (10 ms)
#define Gdac 255.0
#define Gadc 1.0/1540.0
#define Tfd 0.0480
#define filtro_derivador (Kd/(Tf+passo_integrador))

#define REFERENCIA_MOTOR 0.25

float proporcional = 0.0, derivador = 0.0, integrador = 0.0, sinal_controle = 0.0, ei = 0.0;

void app_main(void)
{

    encoder_config_t config = {
        .channel_a_gpio = PCNT_CHAN_GPIO_A, 
        .channel_b_gpio = PCNT_CHAN_GPIO_B, 
        .high_limit = 32000,   
        .low_limit = -32000,   
        .glitch_ns = 1000,   
    };

    encoder_init(&meuencoder, &config);

    // ---------------------------------------------------
    // 1. CONFIGURA OS PINOS DE DIREÇÃO COMO SAÍDA DIGITAL
    // ---------------------------------------------------
    gpio_set_direction(PINO_MOTOR_DIR1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PINO_MOTOR_DIR2, GPIO_MODE_OUTPUT);
    gpio_set_level(PINO_MOTOR_DIR1, 0); // Garante que começa parado
    gpio_set_level(PINO_MOTOR_DIR2, 0);

    // ---------------------------------------------------
    // 2. CONFIGURA O PINO DE PWM (Apenas o pino 25!)
    // ---------------------------------------------------

    
    ledc_timer_config_t ledc_timer ={
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 4000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer); // Corrigido o erro de digitação

    ledc_channel_config_t ledc_channel_1 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PINO_MOTOR_DIR1, // Agora sim, o LEDC está no pino 25!
        .duty           = 0, 
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_1);

    // Corrigido erro de digitação e garantindo que inicie zerado
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0); 
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_channel_config_t ledc_channel_2 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PINO_MOTOR_DIR2, // Agora sim, o LEDC está no pino 25!
        .duty           = 0, 
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_2);

    // Corrigido erro de digitação e garantindo que inicie zerado
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0); 
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    // ---------------------------------------------------
    // LOOP PRINCIPAL
    // ---------------------------------------------------
    float velocidade_lida = 0.0f;
    int pulses_lidos = 0;
    referencia = REFERENCIA_MOTOR;
    erro = 0.0f;
    int contador_20_segundos = 0;

    while(1){

    encoder_set_speed(&meuencoder, &velocidade_lida);

    erro = referencia + (Gadc * (float)e0);
    proporcional = Kc * erro;
   // derivador = Kd * (-velocidade_lida);


    sinal_controle = proporcional; //+ derivador;
    ei = sinal_controle * Gdac;


    // // 5. Limita o sinal de controle aos limites do PWM de 8 bits (-255 a 255)
    // // Isso é conhecido como "Anti-Windup" e evita que o PWM receba valores absurdos
    // if (sinal_controle > 255.0f) {
    //     sinal_controle = 255.0f;
    //     ei = ei - erro; // Trava a integral se saturar (evita windup)
    // } else if (sinal_controle < -255.0f) {
    //     sinal_controle = -255.0f;
    //     ei = ei - erro; // Trava a integral se saturar
    // }

    if(ei > 255.0f){
        ei = 255.0f;
    } else if (ei < -255.0f){
        ei = -255.0f;
    }

    ei = 127;
        int pwm_value = (int)ei;
        if (pwm_value >= 0) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pwm_value);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        } else {
            pwm_value = -pwm_value; // Deixa o valor positivo para o PWM
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, pwm_value);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
        }

        

        contador_20_segundos++;
        
        // 2000 ciclos de 10ms = 20.000ms = 20 segundos
        if (contador_20_segundos >= 1000) { 
            contador_20_segundos = 0; // Zera o contador

            // Inverte a rotação
            if (referencia >= REFERENCIA_MOTOR) {
                referencia = -REFERENCIA_MOTOR;
                ESP_LOGW(TAG, "Invertendo: Para trás!");
            } else {
                referencia = REFERENCIA_MOTOR;
                ESP_LOGW(TAG, "Invertendo: Para frente!");
            }

            // Printa os dados a cada 20s para não poluir o terminal
            printf("\n--- STATUS DO SISTEMA ---\n");
            printf("Ref Alvo:    %.2f\n", referencia);
            printf("Vel Atual:   %.2f\n", velocidade_lida);
            printf("Erro PID:    %.2f\n", erro);
            printf("Sinal PWM:   %d\n", pwm_value);
            printf("-------------------------\n");
            printf("Pulsos Totais: %d | Variacao (Delta): %d\n", meuencoder.pulses, meuencoder.delta_pulses);
            printf("Sinal Controle (Antes de Limitar): %.2f\n", (Kc * erro));

        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}