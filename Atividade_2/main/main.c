#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

#define NUM_MEDICOES 10000

SemaphoreHandle_t sem_tarefa1;
SemaphoreHandle_t sem_tarefa2;

uint64_t tempo_fim_1, tempo_inicio_1;
double tempo_medio_troca_us, percentual_kernel;

double periodo_tick_us = 10e6/configTICK_RATE_HZ; 

// Tarefa 1
void tarefa_1(void *pvParameters) {
    for (int i = 0; i < NUM_MEDICOES; i++) {
        
        xSemaphoreTake(sem_tarefa2, portMAX_DELAY); 
        uint64_t tempo_inicio_1 = esp_timer_get_time();
        xSemaphoreGive(sem_tarefa1);
        uint64_t tempo_fim_1 = esp_timer_get_time();
        ESP_LOGI("TAREFA 1", "Tempo de troca: %lld us", tempo_fim_1 - tempo_inicio_1);
        double kernel_percentual = ((tempo_fim_1 - tempo_inicio_1)/periodo_tick_us) * 100;
        ESP_LOGI("TAREFA 1", "Percentual de tempo gasto no kernel: %4f %%", kernel_percentual);
    }

    vTaskDelete(NULL);
}

// Tarefa 2 (Pong)
void tarefa_2(void *pvParameters) {
    for(int i =0; i<NUM_MEDICOES; i++)
    {
        xSemaphoreGive(sem_tarefa2);
        xSemaphoreTake(sem_tarefa1, portMAX_DELAY);
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    // Cria os semáforos binários
    sem_tarefa1 = xSemaphoreCreateBinary();
    sem_tarefa2 = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(tarefa_2, "Tarefa 2", 2048, NULL, configMAX_PRIORITIES - 1, NULL, 1); 
    xTaskCreatePinnedToCore(tarefa_1, "Tarefa 1", 2048, NULL, configMAX_PRIORITIES - 1, NULL, 1);
}
