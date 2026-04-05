#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "APP_COUNTER";

// Variável global compartilhada entre as tasks
int counter = 0;

void vTaskIncrement(void *pvParameters)
{
    // Recebe o nome da task passado na criação
    char *task_name = (char *)pvParameters;

    while (1)
    {
        // 1. LÊ o valor atual da variável global
        int valor_temporario = counter;

        // 2. INCREMENTA o valor localmente
        valor_temporario+=2;

        // Simula uma preempção: O FreeRTOS tira a CPU desta task 
        // temporariamente e dá para outra task rodar.
        vTaskDelay(1); 

        // 3. SALVA o novo valor na variável global
        counter = valor_temporario;

        // Imprime o resultado
        ESP_LOGI(TAG, "%s incrementou o contador para: %d", task_name, counter);

        // Aguarda meio segundo antes de repetir
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    // Cria duas instâncias da MESMA task, apenas mudando o nome do parâmetro
    xTaskCreate(vTaskIncrement, "Task_A", 2048, "Task A", 5, NULL);
    xTaskCreate(vTaskIncrement, "Task_B", 2048, "Task B", 5, NULL);
}