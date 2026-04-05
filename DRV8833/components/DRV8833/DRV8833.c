#include <stdio.h>
#include "DRV8833.h"
#include "esp_log.h"


// INSERIR LOGS PARA DEBUGAR E INFORMAR ERROS
/**
    Inicialização do DRV8833
 */

esp_err_t drv8833_init(drv8833_t *motor, const drv8833_config_t *config){

    // Verificar se a informação que o usuário colocou está correta
     if (!motor || !config)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if(!GPIO_IS_VALID_GPIO (config->AIN1_pin) || !GPIO_IS_VALID_GPIO (config->AIN2_pin) || !GPIO_IS_VALID_GPIO (config->BIN1_pin) || !GPIO_IS_VALID_GPIO (config->BIN2_pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    motor->config = *config;
    motor->mutex = xSemaphoreCreateMutex();

    if (motor->mutex == NULL)
    {
        return ESP_FAIL;
    }

    // Configuração dos parametros do timer do LEDC
    ledc_timer_config_t ledc_timer ={
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = motor->config.duty_resolution,
        .timer_num = motor->config.timer_num,
        .freq_hz = motor->config.pwm_freq,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_sel  = motor->config.timer_num,
            .intr_type  = LEDC_INTR_DISABLE,
            .duty       = 0, // Inicia parado (0%)
            .hpoint     = 0,
    };

    ledc_channel.channel = motor->config.AIN1_channel;
    ledc_channel.gpio_num = motor->config.AIN1_pin;
    err = ledc_channel_config(&ledc_channel);
    if(err != ESP_OK){
        vSemaphoreDelete(motor->mutex);
        return err;
    }

    ledc_channel.channel = motor->config.AIN2_channel;
    ledc_channel.gpio_num = motor->config.AIN2_pin;
    err = ledc_channel_config(&ledc_channel);
    if(err != ESP_OK){
        vSemaphoreDelete(motor->mutex);
        return err;
    }

    ledc_channel.channel = motor->config.BIN1_channel;
    ledc_channel.gpio_num = motor->config.BIN1_pin;
    err = ledc_channel_config(&ledc_channel);
    if(err != ESP_OK){
        vSemaphoreDelete(motor->mutex);
        return err;
    }

    ledc_channel.channel = motor->config.BIN2_channel;
    ledc_channel.gpio_num = motor->config.BIN2_pin;
    err = ledc_channel_config(&ledc_channel);

    if(err != ESP_OK){
        vSemaphoreDelete(motor->mutex);
        return err;
    }


    return ESP_OK;
}


 /**
    Define a velocidade e direção do Motor A.
 */
esp_err_t drv8833_set_motor_a(drv8833_t *motor, int32_t  velocidade_A)
{
    int32_t duty_1, duty_2;

    if(!motor){
        return ESP_ERR_INVALID_ARG;
    }
    
     if(velocidade_A > 0){
        duty_1 = velocidade_A;
        duty_2 = 0;
    }else if (velocidade_A < 0)
    {
        duty_1 = 0;
        duty_2 = velocidade_A*(-1);
    }else{
        duty_1 = 0;
        duty_2 = 0;
    }
     
    BaseType_t mutex_take = xSemaphoreTake(motor->mutex, portMAX_DELAY);
    if(mutex_take != pdTRUE ){
        return ESP_ERR_TIMEOUT;
    }    
    
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, motor->config.AIN1_channel, duty_1);
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, motor->config.AIN1_channel);
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, motor->config.AIN2_channel, duty_2);
    
    
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, motor->config.AIN2_channel);
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }

    xSemaphoreGive(motor->mutex);
    return ESP_OK;

} 

/**
    Define a velocidade e direção do Motor B.
*/

esp_err_t drv8833_set_motor_b(drv8833_t *motor, int32_t velocidade_B)
{
    int32_t duty_1, duty_2;

 if(!motor){
        return ESP_ERR_INVALID_ARG;
    }

    if(velocidade_B > 0){
        duty_1 = velocidade_B;
        duty_2 = 0;
    }else if (velocidade_B < 0)
    {
        duty_1 = 0;
        duty_2 = velocidade_B*(-1);
    }else{
        duty_1 = 0;
        duty_2 = 0;
    }

    BaseType_t mutex_take = xSemaphoreTake(motor->mutex, portMAX_DELAY);
    if(mutex_take != pdTRUE ){
        return ESP_ERR_TIMEOUT;
    }    
    
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, motor->config.BIN1_channel, duty_1);
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, motor->config.BIN1_channel);
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, motor->config.BIN2_channel, duty_2);
    
    
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, motor->config.BIN2_channel);
    if (err != ESP_OK)
    {
        xSemaphoreGive(motor->mutex);
        return err;
    }

    xSemaphoreGive(motor->mutex);
    return ESP_OK;
}

/**
    Parada de emergencia.
 */
esp_err_t drv8833_stop(drv8833_t *motor){
    ESP_LOGI("stop", "entrou");
    if(!motor){
        ESP_LOGI("stop", "falha");
        return ESP_ERR_INVALID_ARG;
    }

   // BaseType_t mutex_take = xSemaphoreTake(motor->mutex, portMAX_DELAY);
    
   /* if(mutex_take != pdTRUE ){
        ESP_LOGI("stop", "mutex");
        return ESP_ERR_TIMEOUT;
    }    */
    ESP_LOGI("stop", "preso");
    drv8833_set_motor_a(motor, 0);
    ESP_LOGI("stop", "motor A parado");
    drv8833_set_motor_b(motor, 0);
    ESP_LOGI("stop", "motor B parado");

    xSemaphoreGive(motor->mutex);

    return ESP_OK;
}
