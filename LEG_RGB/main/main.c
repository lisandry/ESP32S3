#include <stdio.h>
#include "driver/gpio.h"
#include "driver/ledc.h" // Biblioteca para controle de PWM no ESP32 (usada para o LED)
#include "led_rgb.h"     // Sua biblioteca customizada para abstrair o uso do LED RGB
#include "esp_log.h"     // Biblioteca para exibir mensagens no monitor serial

// --- DEFINIÇÕES DE HARDWARE ---
// Mapeamento dos pinos digitais escolhidos (D27, D26, D25)
// Escolhemos esses pinos pois são saídas seguras e não interferem no boot do ESP32
#define GREEN_PIN_GPIO GPIO_NUM_27
#define BLUE_PIN_GPIO GPIO_NUM_26
#define RED_PIN_GPIO GPIO_NUM_25

// --- DEFINIÇÕES DO PWM (LEDC) ---
// Cada cor do LED precisa de um canal PWM independente para controlar o brilho
#define RED_LEDC_CHANNEL LEDC_CHANNEL_0
#define GREEN_LEDC_CHANNEL LEDC_CHANNEL_1
#define BLUE_LEDC_CHANNEL LEDC_CHANNEL_2

// Resolução do PWM em bits. 
// 8 bits significa que os valores das cores vão de 0 a 255 (2^8 - 1).
#define LEDC_DUTY_RESOLUTION 8 

// O timer responsável por gerar o sinal de clock para os canais PWM acima
#define LEDC_TIMER LEDC_TIMER_0

// --- CONFIGURAÇÃO DO LED ---
// Preenchendo a estrutura com as definições criadas acima.
// Essa estrutura será passada para a função de inicialização.
led_rgb_config_t led_config = {
    .red_pin = RED_PIN_GPIO,
    .blue_pin = BLUE_PIN_GPIO,
    .green_pin = GREEN_PIN_GPIO,

    .red_channel = RED_LEDC_CHANNEL,
    .green_channel = GREEN_LEDC_CHANNEL,
    .blue_channel = BLUE_LEDC_CHANNEL,

    .timer_num = LEDC_TIMER,
    .duty_resolution = LEDC_DUTY_RESOLUTION,
};

// Variável que guarda o "estado/referência" (handle) do nosso LED em funcionamento
led_rgb_t led_rgb_handle;

void app_main(void)
{
    // 1. Inicializa o LED RGB passando o endereço das variáveis de configuração
    esp_err_t err = led_rgb_init(&led_rgb_handle, &led_config);

    // Verifica se a inicialização ocorreu com sucesso
    if (err != ESP_OK)
    {
        ESP_LOGE("[LED_RGB]", "NÃO INICIALIZADO!");
    }

    // 2. Loop infinito (o programa principal roda aqui dentro)
    while (1)
    {
        // --- 1. Vermelho ---
        // Passa a referência do LED e os valores RGB (Red: 255, Green: 0, Blue: 0)
        err = led_rgb_set_color(&led_rgb_handle, 255, 0, 0);
        if (err != ESP_OK) break; // Se der erro ao setar a cor, quebra o loop
        vTaskDelay(pdMS_TO_TICKS(500)); // Aguarda 500 milissegundos

        // --- 2. Laranja ---
        err = led_rgb_set_color(&led_rgb_handle, 255, 127, 0);
        if (err != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(500));

        // --- 3. Amarelo ---
        err = led_rgb_set_color(&led_rgb_handle, 255, 255, 0);
        if (err != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(500));

        // --- 4. Verde ---
        err = led_rgb_set_color(&led_rgb_handle, 0, 255, 0);
        if (err != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(500));

        // --- 5. Azul ---
        err = led_rgb_set_color(&led_rgb_handle, 0, 0, 255);
        if (err != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(500));

        // --- 6. Anil (Indigo) ---
        err = led_rgb_set_color(&led_rgb_handle, 75, 0, 130);
        if (err != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(500));

        // --- 7. Violeta ---
        err = led_rgb_set_color(&led_rgb_handle, 148, 0, 211);
        if (err != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 3. Tratamento de Encerramento
    // O código só chega aqui se acontecer algum erro no loop (quando o 'break' é acionado)
    ESP_LOGE("[LED_RGB]", "Erro detectado, saindo do loop!");
    
    // Desliga e libera os recursos do LEDC (Destrói o Mutex e para o PWM)
    led_deinit(&led_rgb_handle);
    
    // Deleta a tarefa principal do FreeRTOS para liberar a memória
    vTaskDelete(NULL);
    return;
}