#ifndef LED_RGB_H
#define LED_RGB_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Estrutura de configuração do LED RGB.
 * Usada para definir os pinos, canais e timers antes da inicialização.
 */
typedef struct {
    gpio_num_t red_pin;    // Pino GPIO para o LED vermelho
    gpio_num_t green_pin;  // Pino GPIO para o LED verde
    gpio_num_t blue_pin;   // Pino GPIO para o LED azul

    ledc_channel_t red_channel;    // Canal LEDC para o LED vermelho
    ledc_channel_t green_channel;  // Canal LEDC para o LED verde
    ledc_channel_t blue_channel;   // Canal LEDC para o LED azul

    ledc_timer_t timer_num;   // Número do timer do LEDC a ser utilizado
    uint32_t duty_resolution; // Resolução do duty cycle do LEDC (ex: 8 bits)

} led_rgb_config_t;

/**
 * @brief Estrutura de controle (Handle) do LED RGB.
 * Armazena a configuração atual e o Mutex para garantir
 * segurança em ambientes multitarefa (FreeRTOS).
 */
typedef struct {
    led_rgb_config_t config; // Configuração atual do LED RGB
    SemaphoreHandle_t mutex; // Mutex para acesso thread-safe ao LED
} led_rgb_t;


// --- Funções da API ---

/**
 * @brief Inicializa o periférico LEDC para controle do LED RGB.
 * * @param led Ponteiro para o handle do LED (estado).
 * @param config Ponteiro para a estrutura de configuração.
 * @return esp_err_t ESP_OK em caso de sucesso, ou código de erro.
 */
esp_err_t led_rgb_init(led_rgb_t *led, const led_rgb_config_t *config);

/**
 * @brief Define a cor do LED RGB alterando o Duty Cycle do PWM.
 * * @param led Ponteiro para o handle do LED.
 * @param red Valor da cor vermelha (0 a 2^duty_resolution - 1).
 * @param green Valor da cor verde (0 a 2^duty_resolution - 1).
 * @param blue Valor da cor azul (0 a 2^duty_resolution - 1).
 * @return esp_err_t ESP_OK em caso de sucesso, ou código de erro.
 */
esp_err_t led_rgb_set_color(led_rgb_t *led, uint32_t red, uint32_t green, uint32_t blue);

/**
 * @brief Desinicializa o LED RGB, parando os canais PWM e liberando o Mutex.
 * * @param led Ponteiro para o handle do LED.
 * @return esp_err_t ESP_OK em caso de sucesso, ou código de erro.
 */
esp_err_t led_deinit(led_rgb_t *led);


#ifdef __cplusplus
}   
#endif

#endif // LED_RGB_H