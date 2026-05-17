#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

void app_main(void)
{
    // 1. Configuração do Handle da Unidade ADC2 (GPIO 15)
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
    };
    adc_oneshot_new_unit(&init_config2, &adc2_handle);

    // 2. Configuração do Canal
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc2_handle, ADC_CHANNEL_3, &config);

    // 3. Configuração da Calibração CORRIGIDA (Line Fitting)
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);

    int valor_raw;
    int voltagem_mv;

    while (1) {
        // Leitura do valor bruto (0 - 4095)
        adc_oneshot_read(adc2_handle, ADC_CHANNEL_3, &valor_raw);

        // Conversão para milivolts usando a calibração
        adc_cali_raw_to_voltage(cali_handle, valor_raw, &voltagem_mv);

        // Conversão de mV para Volts
        float volts = voltagem_mv / 1000.0;

        printf("Raw: %d | Voltagem no Pino: %.2f V\n", valor_raw, volts);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}