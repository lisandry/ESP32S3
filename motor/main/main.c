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
#include "driver/ledc.h"

static const char *TAG = "MOTOR";

#define LED GPIO_NUM_8

// VAO AO MOTOR
#define MOTOR_PIN_GPIO_A1 GPIO_NUM_6 /**< Pino GPIO para direção 1 do motor */
#define MOTOR_PIN_GPIO_A2 GPIO_NUM_7 /**< Pino GPIO para direção 2 do motor */


// VEM DO DRV
#define PWM_GPIO_A GPIO_NUM_4 
#define PWM_GPIO_B GPIO_NUM_5
// Configuração PCNT
#define PCNT_HIGH_LIMIT  4000       /**< Limite superior do contador de pulsos */
#define PCNT_LOW_LIMIT  -4000       /**< Limite inferior do contador de pulsos */


// Configurações do PID
#define Kc 1.25 // Ganho proporcional
#define Kd 0.095 // Ganho derivativo
#define passo_integrador 0.01 // Passo de tempo para o integrador (10 ms)
#define Gdac 255.0 
#define Gadc 2.0/770.0
#define Tfd 0.0480
#define filtro_derivador (Kd/(Tfd+passo_integrador))
#define REFERENCIA_MOTOR 0.25


int e0 = 0, pwm_value=0;
int count_anterior = 0;
int pulse_count = 0;
int last_count = 0;
int delta_pulsos = 0;
float proporcional = 0.0, derivador = 0.0, integrador = 0.0;
float sinal_controle = 0.0, ei = 0.0;
float referencia = 0.0f, velocidade = 0.0f;
float erro = 0.0;
int f_led = 0;

// Parametros reta tempo x pulsos
float b = 0.81f;
float m = -0.00046f;

pcnt_channel_handle_t pcnt_chan = NULL;
pcnt_unit_handle_t pcnt_unit = NULL;
gptimer_handle_t gptimer = NULL;

esp_timer_handle_t timer_handler;


 void IRAM_ATTR timer_isr(void *arg) {
    f_led ^= 1;
    gpio_set_level(LED, f_led); 

   pcnt_unit_get_count(pcnt_unit, &pulse_count);
    e0 = pulse_count;
    /*erro = referencia - (Gadc*e0);

    proporcional = Kc * erro;

    delta_pulsos = pulse_count - last_count;
    last_count = pulse_count;

    if (delta_pulsos >= 0){
        velocidade = b - ((float)delta_pulsos * m);
    }
    // derivador = Kd * (-velocidade);

    // Cálculo do termo Integrativo com Anti-Windup simples
    // integrador += Ki * erro * passo_integrador;
    // if(integrador > 0.10f) integrador = 0.10f; 
    // else if(integrador < -0.10f) integrador = -0.10f;
    
    sinal_controle = proporcional; // + integrador + derivador;
    ei = sinal_controle * Gdac;

    if(ei > 255.0f){
        ei = 255.0f;
    } else if (ei < -255.0f){
        ei = -255.0f;
    }

    ei = 127;
    pwm_value = (int)ei;
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
        */
    
}


void app_main(void)
{

    gpio_set_direction(PWM_GPIO_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(PWM_GPIO_B, GPIO_MODE_OUTPUT);
    gpio_set_level(PWM_GPIO_A, 0); 
    gpio_set_level(PWM_GPIO_B, 0);

    // gpio_set_direction(MOTOR_PIN_GPIO_A1, GPIO_MODE_INPUT);
    // gpio_set_direction(MOTOR_PIN_GPIO_A2, GPIO_MODE_INPUT);
    // gpio_set_pull_mode(MOTOR_PIN_GPIO_A1, GPIO_PULLUP_ONLY);
    // gpio_set_pull_mode(MOTOR_PIN_GPIO_A2, GPIO_PULLUP_ONLY);

    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_level(LED, 0);


    ledc_timer_config_t ledc_timer ={
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 4000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer); 

    ledc_channel_config_t ledc_channel_0 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_GPIO_A, 
        .duty           = 0, 
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_0);

    // Corrigido erro de digitação e garantindo que inicie zerado
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0); 
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        ledc_channel_config_t ledc_channel_1 = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_GPIO_B, 
        .duty           = 0, 
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel_1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0); 
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);


    //ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
    };
    

    pcnt_new_unit(&unit_config, &pcnt_unit);

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = MOTOR_PIN_GPIO_A1,
        .level_gpio_num = MOTOR_PIN_GPIO_A2,
    };

    
    pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan);
    pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit); 


    const esp_timer_create_args_t timer_args = {
        .callback = &timer_isr,
        .name = "Timer"
    };

    esp_timer_create(&timer_args, &timer_handler);
    esp_timer_start_periodic(timer_handler, 1000);
    
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
        .direction = GPTIMER_COUNT_UP,      // Counting direction is up
        .resolution_hz = 1 * 1000 * 1000,   // Resolution is 1 MHz, i.e., 1 tick equals 1 microsecond
    };

    gptimer_new_timer(&timer_config, &gptimer);

    gptimer_enable(gptimer);

    gptimer_start(gptimer);

    for(;;){
    if (referencia >= REFERENCIA_MOTOR) {
            referencia = -REFERENCIA_MOTOR;
        } else {
            referencia = REFERENCIA_MOTOR;
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS); 

    printf("\n--- STATUS DO SISTEMA ---\n");
    printf("Pulsos:       %d\n", pulse_count);
    printf("Ref Alvo:    %.2f\n", referencia);
    printf("Vel Atual:   %.2f\n", velocidade);
    printf("e0:    %.2f\n", (float)e0);
    printf("ei:   %.2f\n", (float)ei);
    printf("Erro PID:    %.2f\n", (float)erro);
    printf("Sinal PWM:   %d\n", pwm_value);
    printf("Pulsos Totais:   %d\n", pulse_count);
    printf("Variacao (Delta):   %.2f\n", (float)delta_pulsos);
    printf("Sinal Controle (Antes de Limitar): %.2f\n", (float)(Kc * erro));
    printf("Sinal Controle (Depois de Limitar): %.2f\n", (float)sinal_controle);
    printf("Proporcional: %.2f\n", (float)proporcional);
    // printf("Integrativo: %.2f\n", (float)integrador);
    // printf("Derivativo: %.2f\n", (float)derivador);    
    printf("-------------------------\n");

    } 
}