#include "encoder.h"

static const char *TAG = "ENCODER"; // Faltava definir a TAG
//static int event_count = 0; // Variável para contar os eventos de limite atingido, se necessário

esp_timer_handle_t timer_handler; // Handler global para o timer

int e0 = 0;
int count_anterior = 0;

float b = 0.81f;
float m = -0.00046f;

// float proporcional = 0.0, derivador = 0.0, integrador = 0.0, sinal_controle = 0.0, ei = 0.0;
float referencia = 0.0f, velocidade = 0.0f;
float erro = 0.0;

encoder_t meuencoder;

static void IRAM_ATTR encoder_timer_isr(void *arg) {
    if(arg == NULL) {
        //ESP_LOGE(TAG, "Encoder GPIO ISR: Invalid argument");
        return;
    }   
    int count = 0;

    pcnt_unit_get_count(meuencoder.pcnt_unit, &count);
    e0 = count;

    // meuencoder.delta_pulses = count - meuencoder.last_count;
    meuencoder.last_count = count; 
}


esp_err_t encoder_init(encoder_t *encoder, const encoder_config_t *config) 
{
    esp_err_t err = 0;
    
    if (encoder == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if(!GPIO_IS_VALID_GPIO(config->channel_a_gpio) || !GPIO_IS_VALID_GPIO(config->channel_b_gpio) || config->high_limit <= config->low_limit) {
        return ESP_ERR_INVALID_ARG;
    }

    encoder->config = *config;
    encoder->last_count = 0;

    encoder->mutex = xSemaphoreCreateMutex();
    if (encoder->mutex == NULL) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = config->high_limit,
        .low_limit = config->low_limit,
    };
    err = pcnt_new_unit(&unit_config, &encoder->pcnt_unit);
    if (err != ESP_OK) {
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to create pulse counter unit");
        return err;
    }

    ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = config->glitch_ns,
    };
    err = pcnt_unit_set_glitch_filter(encoder->pcnt_unit, &filter_config);
    if (err != ESP_OK) {
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to set glitch filter");
        return err;
    }
    
    ESP_LOGI(TAG, "Configure the pulse counter channel");
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = config->channel_a_gpio,
        .level_gpio_num = config->channel_b_gpio,
    };

    ESP_LOGI(TAG, "create pcnt channel");
    err = pcnt_new_channel(encoder->pcnt_unit, &chan_config, &encoder->pcnt_chan);
    if (err != ESP_OK) {
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to create pulse counter channel");
        return err;
    }

    ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    if(pcnt_channel_set_edge_action(encoder->pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE) != ESP_OK) {
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to set edge action for pulse counter channel");
        return err;
    }

    if(pcnt_channel_set_level_action(encoder->pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE) != ESP_OK) {
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to set level action for pulse counter channel");
        return err;
    }

    ESP_LOGI(TAG, "enable pcnt unit");
    if(pcnt_unit_enable(encoder->pcnt_unit) != ESP_OK) {
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to enable pulse counter unit");
        return err;
    }
    ESP_LOGI(TAG, "clear pcnt unit");
    if(pcnt_unit_clear_count(encoder->pcnt_unit) != ESP_OK) {
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to clear pulse counter unit");
        return err;
    }
    ESP_LOGI(TAG, "start pcnt unit");
  if(pcnt_unit_start(encoder->pcnt_unit) != ESP_OK) {
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to start pulse counter unit");
        return err;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &encoder_timer_isr,
        .name = "Timer"
    };

    esp_timer_create(&timer_args, &timer_handler);
    esp_timer_start_periodic(timer_handler, 1000);

   gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
    .direction = GPTIMER_COUNT_UP,      // Counting direction is up
    .resolution_hz = 1 * 1000 * 1000,   // Resolution is 1 MHz, i.e., 1 tick equals 1 microsecond
    };
    err = gptimer_new_timer(&timer_config, &encoder->gptimer);
    if(err != ESP_OK) {
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to create GPTimer");
        return err;
    }

    err = gptimer_enable(encoder->gptimer);
    if(err != ESP_OK) {
        gptimer_del_timer(encoder->gptimer);
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);    
        ESP_LOGE(TAG, "Failed to enable GPTimer");
        return err;
    }

    err = gptimer_start(encoder->gptimer);
    if(err != ESP_OK) {
        gptimer_del_timer(encoder->gptimer);
        pcnt_del_channel(encoder->pcnt_chan);
        pcnt_del_unit(encoder->pcnt_unit);
        vSemaphoreDelete(encoder->mutex);
        ESP_LOGE(TAG, "Failed to start GPTimer");
        return err;
    }

    ESP_LOGI(TAG, "Encoder initialized successfully");
    return ESP_OK;
}

esp_err_t encoder_set_speed(encoder_t *encoder, float *speed) {
    if (encoder == NULL || speed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

   if (xSemaphoreTake(encoder->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take mutex in set_speed");
        return ESP_FAIL;
    }

    if(encoder->delta_pulses >= 0){
        velocidade = b - (m * (float)encoder->delta_pulses);
        encoder->speed = velocidade;
    }

    *speed = velocidade;
    //printf("Pulsos (Delta): %d | Velocidade: %.2f\n", encoder->delta_pulses, velocidade);

    //*speed = (float)encoder->delta_pulses;

    // erro = referencia - (Gadc * (float)e0);
    // proporcional = Kc * erro;
   // derivador = Kd * (-velocidade);


    // sinal_controle = proporcional; //+ derivador;
    // ei = sinal_controle * Gdac;

    // if (ei > 255.0f) ei = 255.0f;
    // else if (ei < -255.0f) ei = -255.0f;

    //printf("Pulsos Totais: %d | Variacao (Delta): %d\n", encoder->pulses, encoder->delta_pulses);
    xSemaphoreGive(encoder->mutex);

    return ESP_OK;
}

esp_err_t encoder_deinit(encoder_t *encoder) {
    if (encoder == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Tenta pegar o mutex para garantir que não está em uso
    if (xSemaphoreTake(encoder->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Falha ao pegar o mutex para deinit");
        return ESP_FAIL;    
    }

    esp_timer_stop(timer_handler);
    esp_timer_delete(timer_handler);

    // Para o contador e limpa os recursos
    pcnt_unit_stop(encoder->pcnt_unit);
    pcnt_del_channel(encoder->pcnt_chan);
    pcnt_del_unit(encoder->pcnt_unit);

    xSemaphoreGive(encoder->mutex); // Devolve a chave antes de deletar
    vSemaphoreDelete(encoder->mutex); // Deleta o mutex

    return ESP_OK;
}   