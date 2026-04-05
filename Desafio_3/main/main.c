#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // Necessário para usar o Mutex
#include "esp_log.h"

static const char *TAG = "APP_COUNTER";

// Variável global compartilhada
int counter = 0;

// Handle do nosso Mutex
SemaphoreHandle_t mutex_contador;

void vTaskIncrement(void *pvParameters)
{
    char *task_name = (char *)pvParameters;

    while (1)
    {
        // Tenta pegar o Mutex (trancar a porta). 
        // portMAX_DELAY faz a task ficar bloqueada esperando sua vez infinitamente.
        if (xSemaphoreTake(mutex_contador, portMAX_DELAY) == pdTRUE)
        {
            // ================= ZONA CRÍTICA =================
            int valor_temporario = counter;
            valor_temporario++;

            // O delay continua aqui para forçar uma pausa.
            // O FreeRTOS vai tentar rodar a outra Task, mas ela vai 
            // travar no xSemaphoreTake ali em cima porque o Mutex está ocupado!
            vTaskDelay(1); 

            counter = valor_temporario;

            ESP_LOGI(TAG, "%s incrementou o contador para: %d", task_name, counter);
            // ================================================

            // Devolve o Mutex (destranca a porta)
            xSemaphoreGive(mutex_contador);
        }

        // Aguarda meio segundo fora da zona crítica para a outra task ter tempo de agir
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    // 1. O Mutex SEMPRE deve ser criado antes das tasks que vão usá-lo
    mutex_contador = xSemaphoreCreateMutex();

    // 2. Só cria as tasks se o Mutex foi alocado na memória com sucesso
    if (mutex_contador != NULL)
    {
        xTaskCreate(vTaskIncrement, "Task_A", 2048, "Task A", 5, NULL);
        xTaskCreate(vTaskIncrement, "Task_B", 2048, "Task B", 5, NULL);
    }
    else
    {
        ESP_LOGE(TAG, "Falha ao criar o Mutex!");
    }
}