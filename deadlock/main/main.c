#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

SemaphoreHandle_t mutexA;
SemaphoreHandle_t mutexB;


void Task1(void *pvParameters)
{
    while (1) {
        xSemaphoreTake(mutexA, portMAX_DELAY);
        ESP_LOGI("Task1", "Acquired mutexA");
        vTaskDelay(100 / portTICK_PERIOD_MS); 

        xSemaphoreTake(mutexB, portMAX_DELAY);
        ESP_LOGI("Task1", "Acquired mutexB");
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexB);
        ESP_LOGI("Task1", "Released mutexA");
        xSemaphoreGive(mutexA);
        ESP_LOGI("Task1", "Released mutexB");
    }
}

void Task2(void *pvParameters)
{
    while (1) {
        xSemaphoreTake(mutexB, portMAX_DELAY);
        ESP_LOGI("Task2", "Acquired mutexB");
        vTaskDelay(100 / portTICK_PERIOD_MS); 

        xSemaphoreTake(mutexA, portMAX_DELAY);
        ESP_LOGI("Task2", "Acquired mutexA");
    
        vTaskDelay(100 / portTICK_PERIOD_MS);
        xSemaphoreGive(mutexA);
        ESP_LOGI("Task2", "Released mutexA");
        xSemaphoreGive(mutexB);
        ESP_LOGI("Task2", "Released mutexB");
    }
}

void app_main(void)
{
    mutexA = xSemaphoreCreateMutex();
    mutexB = xSemaphoreCreateMutex();

    xTaskCreate(Task1, "Task1", 2048, NULL, 1, NULL);
    xTaskCreate(Task2, "Task2", 2048, NULL, 1, NULL);
}
