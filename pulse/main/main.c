#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"


static const char *TAG = "example";

#define EXAMPLE_PCNT_HIGH_LIMIT 32000
#define EXAMPLE_PCNT_LOW_LIMIT  -32000

#define EXAMPLE_CHAN_GPIO_A 1
#define EXAMPLE_CHAN_GPIO_B 0

void app_main(void)
{
esp_err_t err = 0;

    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = EXAMPLE_PCNT_HIGH_LIMIT,
        .low_limit = EXAMPLE_PCNT_LOW_LIMIT,
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    err = pcnt_new_unit(&unit_config, &pcnt_unit);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create pulse counter unit");
        return;
    }

    ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    err = pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set glitch filter");
        return;
    }

    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = EXAMPLE_CHAN_GPIO_A,
       // .level_gpio_num = EXAMPLE_CHAN_GPIO_B,
       .level_gpio_num = -1 //Desativa virtualmente o segundo canal, para usar apenas o canal A como entrada de pulso
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    err = pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create pulse counter channel");
        return;
    }

    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
   // ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
   // ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
   ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));

    ESP_LOGI(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_LOGI(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_LOGI(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    int pulse_count = 0;
    int event_count = 0;
    

    
    while (1) {
        // 1. Zera o contador para garantir que a contagem comece do zero
        ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));

        // 2. Aguarda 1 segundo exato (janela de tempo de medição)
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 3. Lê o valor acumulado no hardware durante esse 1 segundo
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulse_count));
        ESP_LOGI(TAG, "Pulse count (1s): %d", pulse_count);

        // 4. Checa os limites estabelecidos
        if (pulse_count >= EXAMPLE_PCNT_HIGH_LIMIT) {
            event_count++;
            ESP_LOGI(TAG, "Limite atingido! Event count: %d", event_count);
            // Não precisa dar clear aqui novamente, pois o início do loop já faz isso.
        }

    }
}
