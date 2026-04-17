/**
 * @file main.c
 * @brief Main application entry point for BMP280 sensor telemetry.
 *
 * @note This file demonstrates how to integrate the custom bmp280 component,
 * map Kconfig parameters, initialize the I2C bus, and spawn a FreeRTOS task 
 * for continuous environmental monitoring.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

// Inclui o seu componente recém-criado!
#include "bmp280.h"

/** @brief Global handle for the BMP280 instance used across tasks. */
bmp280_t bmp280_handle; 

/** @brief Global tag for system logs. */
static const char *TAG = "APP_MAIN";

/* ========================================================================== */
/* RESOLUÇÃO DO ENDEREÇO I2C VIA KCONFIG                                      */
/* ========================================================================== */
// O Kconfig gera macros booleanas no sdkconfig.h. Usamos diretivas de 
// pré-processador para definir o endereço final que vai para a struct.
#ifdef CONFIG_BMP280_I2C_ADDR_0x76
    #define I2C_ADDR BMP280_I2C_ADDRESS_0
#else
    #define I2C_ADDR BMP280_I2C_ADDRESS_1
#endif

/* ========================================================================== */
/* TAREFA DE TEMPO REAL (FREERTOS)                                            */
/* ========================================================================== */

/**
 * @brief FreeRTOS task dedicated to polling the BMP280 sensor.
 *
 * @details This task runs continuously in an infinite loop, querying the 
 * sensor for synchronized temperature and pressure data using burst reads,
 * printing the results, and yielding the CPU to other tasks.
 *
 * @param pvParameters Pointer to parameters passed during task creation (unused).
 */
void bmp280_monitoring_task(void *pvParameters) {
    float temperature = 0.0f;
    float pressure = 0.0f;
    esp_err_t err;

    ESP_LOGI(TAG, "Iniciando monitoramento em tempo real...\n");

    // Loop infinito da tarefa (o famoso "tempo real")
    while (1) {
        // Usa a função de burst read que criamos para pegar os dados sincronizados
        err = bmp280_read_measurements(&bmp280_handle, &temperature, &pressure);
        
        if (err == ESP_OK) {
            // Imprime no terminal de forma amigável
            printf("Temp: %.2f °C  |  Pressão: %.2f Pa\n", temperature, pressure);
        } else {
            ESP_LOGE(TAG, "Falha ao ler o sensor no barramento I2C.");
        }

        // Aguarda 1000 milissegundos (1 segundo) e libera o processador
        // para outras tarefas antes de ler novamente.
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ========================================================================== */
/* FUNÇÃO PRINCIPAL DO SISTEMA                                                */
/* ========================================================================== */

/**
 * @brief Main execution entry point.
 *
 * @details Initializes system logging, maps configuration parameters provided 
 * by the ESP-IDF Menuconfig to the sensor structure, initializes the I2C bus,
 * configures the sensor's hardware registers, and finally spawns the 
 * FreeRTOS monitoring task.
 */
void app_main(void) {
    // Opcional: Você pode usar a tag existente ou criar uma só para esse teste
    const char *CFG_TAG = "TESTE_KCONFIG";

    ESP_LOGW(CFG_TAG, "=========================================");
    ESP_LOGW(CFG_TAG, "    CONFIGURAÇÕES DO MENUCONFIG    ");
    ESP_LOGW(CFG_TAG, "=========================================");
    
    // Pinos e Porta I2C
    ESP_LOGW(CFG_TAG, "-> Pino SDA: GPIO %d", CONFIG_BMP280_I2C_SDA_PIN);
    ESP_LOGW(CFG_TAG, "-> Pino SCL: GPIO %d", CONFIG_BMP280_I2C_SCL_PIN);
    ESP_LOGW(CFG_TAG, "-> Porta I2C Hardware: I2C_NUM_%d", CONFIG_BMP280_I2C_PORT_NUM);
    
    // Frequência (Usando a nossa variável oculta!)
    ESP_LOGW(CFG_TAG, "-> Frequência I2C: %d Hz", CONFIG_BMP280_I2C_FREQ_HZ);
    
    // Endereço I2C
    // Como o endereço vem daquele bloco #ifdef que criamos no topo do main.c,
    // imprimimos a variável final I2C_ADDR usando %02X para formatar como Hexadecimal (ex: 0x76)
    ESP_LOGW(CFG_TAG, "-> Endereço I2C Resolvido: 0x%02X", I2C_ADDR);
    
    ESP_LOGW(CFG_TAG, "=========================================\n");
    
    
    ESP_LOGI(TAG, "Inicializando o sistema de telemetria...");

    // 1. Preenchemos a estrutura do sensor
    // Repare que todos os dados de hardware vêm do menuconfig (CONFIG_BMP280_...)
    bmp280_config_t sensor_cfg = {
        .sda_pin = CONFIG_BMP280_I2C_SDA_PIN,
        .scl_pin = CONFIG_BMP280_I2C_SCL_PIN,
        .i2c_port = CONFIG_BMP280_I2C_PORT_NUM,
        .i2c_addr = I2C_ADDR,
        .i2c_freq_hz = CONFIG_BMP280_I2C_FREQ_HZ,
        
        // 2. Parâmetros lógicos do sensor
        // Escolhemos NORMAL mode para que o sensor fique medindo continuamente
        .mode = BMP280_MODE_NORMAL, 
        .oversampling_temperature = BMP280_OVERSAMPLING_STANDARD, // Resolução padrão
        .oversampling_pressure = BMP280_OVERSAMPLING_STANDARD,    // Resolução padrão
        .filter = BMP280_FILTER_4,                                // Suaviza o ruído do ar
        .standby_time = BMP280_STANDBY_250_MS                     // Ritmo interno do chip
    };

    // 3. Monta o barramento I2C e verifica se o sensor responde (Lê calibração)
    // O ESP_ERROR_CHECK vai abortar e reiniciar o chip caso o sensor não seja detectado.
    ESP_LOGI(TAG, "Provisionando o barramento I2C e lendo calibração de fábrica...");
    ESP_ERROR_CHECK(bmp280_init(&bmp280_handle, &sensor_cfg));

    // 4. Injeta os parâmetros de filtro e resolução nos registradores do chip
    ESP_LOGI(TAG, "Gravando registradores de configuração (Oversampling, Filter, Mode)...");
    ESP_ERROR_CHECK(bmp280_set_config(&bmp280_handle, &sensor_cfg));

    ESP_LOGI(TAG, "BMP280 inicializado com sucesso!");

    // 5. Cria e despacha a tarefa do FreeRTOS
    // Argumentos: Função, Nome de debug, Tamanho da Pilha (Memória), Parâmetros extras, Prioridade, Handle
    xTaskCreate(bmp280_monitoring_task, "bmp280_task", 4096, NULL, 5, NULL);
}