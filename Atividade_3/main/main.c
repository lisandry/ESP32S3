#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h" 

#define TAMANHO_BUFFER 10
#define NUM_ESCRITAS 5

// --- Tags para os Logs ---
static const char *TAG_PROD = "PRODUTOR";
static const char *TAG_LEIT = "LEITOR";
static const char *TAG_SYS  = "SISTEMA";

// --- Variáveis Globais ---
char buffer[TAMANHO_BUFFER][20];
int index_escrita = 0;
int index_leitura = 0;

// --- Semáforos ---
SemaphoreHandle_t mutex_buffer;
SemaphoreHandle_t sem_vazios;
SemaphoreHandle_t sem_cheios;

// --- Tarefa Produtora ---
void vTaskProdutora(void *pvParameters) {
    char *nome_tarefa = (char *)pvParameters;

    for (int i = 0; i < NUM_ESCRITAS; i++) {
        // Aguarda ter uma posição vazia no buffer
        xSemaphoreTake(sem_vazios, portMAX_DELAY);

        // Aguarda liberar o Mutex para entrar na Região Crítica
        xSemaphoreTake(mutex_buffer, portMAX_DELAY);

        // Escreve na posição e atualiza o índice
        strcpy(buffer[index_escrita], nome_tarefa);
        
        ESP_LOGI(TAG_PROD, "[%s] Escreveu na posicao %d", nome_tarefa, index_escrita);

        index_escrita = (index_escrita + 1) % TAMANHO_BUFFER;

        // Libera o recurso compartilhado (Mutex)
        xSemaphoreGive(mutex_buffer);

        // Sinaliza que há um novo item cheio disponível para leitura
        xSemaphoreGive(sem_cheios);
        
        // Pequeno delay para forçar a troca de contexto e misturar as escritas
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }

    // Escrita finalizada 
    ESP_LOGI(TAG_SYS, "%s: Escrita Finalizada.", nome_tarefa); 
    vTaskDelete(NULL);
}

// --- Tarefa Leitora ---
void vTaskLeitura(void *pvParameters) {
    char *nome_tarefa = (char *)pvParameters;
    char dado_lido[20];

    while (1) {
        if (xSemaphoreTake(sem_cheios, pdMS_TO_TICKS(500)) == pdTRUE) {
            
            // Entra na Região Crítica
            xSemaphoreTake(mutex_buffer, portMAX_DELAY);

            strcpy(dado_lido, buffer[index_leitura]);
            index_leitura = (index_leitura + 1) % TAMANHO_BUFFER;

            // Libera o Mutex e sinaliza que abriu um espaço vazio
            xSemaphoreGive(mutex_buffer);
            xSemaphoreGive(sem_vazios);

            ESP_LOGI(TAG_LEIT, "%s: %s", nome_tarefa, dado_lido);
            
            vTaskDelay(pdMS_TO_TICKS(15));
        } else {
            ESP_LOGI(TAG_SYS, "%s: Leitura finalizada!", nome_tarefa); 
            vTaskDelete(NULL); 
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG_SYS, "Iniciando o sistema...");

    // Criação dos semáforos
    mutex_buffer = xSemaphoreCreateMutex();
    sem_vazios = xSemaphoreCreateCounting(TAMANHO_BUFFER, TAMANHO_BUFFER);
    sem_cheios = xSemaphoreCreateCounting(TAMANHO_BUFFER, 0);

    // Criação das Tarefas produtoras 
    xTaskCreate(vTaskProdutora, "Temperatura", 2048, "Temperatura", 1, NULL);
    xTaskCreate(vTaskProdutora, "Umidade", 2048, "Umidade", 1, NULL);
    xTaskCreate(vTaskProdutora, "Velocidade", 2048, "Velocidade", 1, NULL);
    xTaskCreate(vTaskProdutora, "Peso", 2048, "Peso", 1, NULL);
    xTaskCreate(vTaskProdutora, "Distancia", 2048, "Distancia", 1, NULL);

    // Criação da Tarefas Leitura
    xTaskCreate(vTaskLeitura, "TaskLeitura1", 2048, "TaskLeitura1", 1, NULL);
    xTaskCreate(vTaskLeitura, "TaskLeitura2", 2048, "TaskLeitura2", 1, NULL);
}