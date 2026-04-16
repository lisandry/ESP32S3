#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bmp280.h"

// Definição dos pinos I2C (Para o ESP32 WROOM de 30 pinos)
#define PIN_I2C_SDA GPIO_NUM_21
#define PIN_I2C_SCL GPIO_NUM_22

// Configuração Global do Sensor (Igual você fez no motor)
bmp280_config_t config_sensor = {
    .sda_pin = PIN_I2C_SDA,
    .scl_pin = PIN_I2C_SCL,
    .i2c_port = I2C_NUM_0,
    .i2c_addr = BMP280_I2C_ADDRESS_0, // 0x76 (Mude para 0x77 se der erro)
    .i2c_freq_hz = 100000,            // Frequência do I2C (100kHz)
    
    // Configurações de Operação
    .mode = BMP280_MODE_NORMAL,       // Medição contínua
    .oversampling_temperature = BMP280_OVERSAMPLING_STANDARD,
    .oversampling_pressure = BMP280_OVERSAMPLING_STANDARD,
    .filter = BMP280_FILTER_4,
    .standby_time = BMP280_STANDBY_1000_MS 
};

void app_main(void)
{
    // REMOVA qualquer código de i2c_new_master_bus que estiver aqui em cima!

    // 1. Apenas defina a configuração
    bmp280_config_t config_sensor = {
        .sda_pin = GPIO_NUM_21,
        .scl_pin = GPIO_NUM_22,
        .i2c_port = I2C_NUM_0,
        .i2c_addr = BMP280_I2C_ADDRESS_0, // 0x76 (confirmado pelo seu log!)
        .i2c_freq_hz = 100000,
        .mode = BMP280_MODE_NORMAL,
        .oversampling_temperature = BMP280_OVERSAMPLING_STANDARD,
        .oversampling_pressure = BMP280_OVERSAMPLING_STANDARD,
        .filter = BMP280_FILTER_4,
        .standby_time = BMP280_STANDBY_1000_MS 
    };

    // 2. Chame a sua função de inicialização. 
    // Ela vai abrir o barramento sozinha.
    esp_err_t err = bmp280_init(&config_sensor);
    if (err != ESP_OK) {
        ESP_LOGE("teste_bmp", "Erro ao inicializar: %d", err);
        return;
    }
    
    // 3. Configure e inicie as leituras
    bmp280_set_config(&config_sensor);

    float temp, press;
    while(1) {
        if (bmp280_read_measurements(&temp, &press) == ESP_OK) {
            printf("Temp: %.2f C | Press: %.2f Pa\n", temp, press);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}