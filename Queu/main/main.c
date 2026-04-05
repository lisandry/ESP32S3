#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

// --- Definições da Fila ---
// A fila terá capacidade para armazenar até 10 itens por vez.
#define TAMANHO_FILA 10
// O tamanho de cada item será o tamanho de um número inteiro (int)
#define TAMANHO_ITEM sizeof(int)

static const char *TAG = "APP_SENSOR";

// Handle (referência) global para a nossa fila
QueueHandle_t fila_sensor;

/**
 * @brief Task 1: Leitura do Sensor
 * Gera um valor (neste caso, um contador) a cada 500ms e o envia para a fila.
 */
void vTaskSensor(void *pvParameters)
{
    int contador = 0;
    
    while (1)
    {
        contador++; // Simula a leitura de um novo dado do sensor
        
        // Tenta enviar o dado para a fila.
        // O último parâmetro (pdMS_TO_TICKS(100)) é o tempo máximo que a task vai
        // esperar caso a fila esteja cheia.
        if (xQueueSend(fila_sensor, &contador, pdMS_TO_TICKS(100)) != pdTRUE)
        {
            // Se a fila estiver cheia e o tempo esgotar, avisa com um Warning
            ESP_LOGW(TAG, "A fila está cheia! Dado perdido: %d", contador);
        }
        
        // Aguarda 500 ms até a próxima "leitura"
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief Task 2: Envio Serial (Log)
 * Fica aguardando dados chegarem na fila e os imprime no terminal.
 */
void vTaskSerial(void *pvParameters)
{
    int valor_recebido;
    
    while (1)
    {
        // Tenta ler um dado da fila.
        // portMAX_DELAY faz com que esta task fique bloqueada (dormindo e sem
        // gastar processamento) infinitamente até que algum dado chegue na fila.
        if (xQueueReceive(fila_sensor, &valor_recebido, portMAX_DELAY) == pdTRUE)
        {
            // Quando o dado chega, ele é impresso usando a biblioteca de logs
            ESP_LOGI(TAG, "Dado do Sensor Recebido e Processado: %d", valor_recebido);
        }
        //vTaskDelay(pdMS_TO_TICKS(1000)); //Permite simular fila cheia

    }
}

void app_main(void)
{
    // 1. Criação da Fila
    // Deve ser criada antes de inicializar as tasks que vão utilizá-la
    fila_sensor = xQueueCreate(TAMANHO_FILA, TAMANHO_ITEM);

    // Verifica se houve memória RAM suficiente para criar a fila
    if (fila_sensor == NULL)
    {
        ESP_LOGE(TAG, "Falha ao criar a fila de comunicação!");
        return; // Encerra a execução caso falhe
    }

    // 2. Criação das Tasks
    // Parâmetros: (Função da Task, Nome, Tamanho da Pilha, Parametros pra função, Prioridade, Handle)
    
    // Criamos a task do sensor com prioridade 5
    if (xTaskCreate(vTaskSensor, "Task_Sensor", 2048, NULL, 5, NULL) != pdPASS)
    {
         ESP_LOGE(TAG, "Falha ao criar Task_Sensor");
    }
    
    // Criamos a task do log também com prioridade 5
    if (xTaskCreate(vTaskSerial, "Task_Serial", 2048, NULL, 5, NULL) != pdPASS)
    {
         ESP_LOGE(TAG, "Falha ao criar Task_Serial");
    }
}