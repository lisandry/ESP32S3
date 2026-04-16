#include <stdio.h>
#include "led_rgb.h"

// Frequência do PWM em Hertz (5 kHz é um valor excelente para LEDs, 
// pois evita que a luz pareça estar "piscando" para o olho humano ou câmeras)
#define LEDC_FREQ 5000

// TODO: INSERIR LOGS PARA DEBUGAR E INFORMAR ERROS E TALS

esp_err_t led_rgb_init(led_rgb_t *led, const led_rgb_config_t *config)
{
    // 1. Validação de Segurança Básica
    // Verifica se os ponteiros passados não são nulos
    if (!led || !config)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // Verifica se os pinos fornecidos são GPIOs válidos para saída no ESP32
    if (!GPIO_IS_VALID_GPIO(config->red_pin) || !GPIO_IS_VALID_GPIO(config->blue_pin) || !GPIO_IS_VALID_GPIO(config->green_pin))
    {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: verificar canais tbm XP

    // 2. Salva a configuração fornecida dentro do "handle" (estado) do LED
    led->config = *config;

    // 3. Criação do Mutex para Thread-Safety
    // Isso garante que duas tarefas do FreeRTOS não tentem mudar a cor do LED ao mesmo tempo,
    // o que poderia causar conflitos de hardware.
    led->mutex = xSemaphoreCreateMutex();
    if (led->mutex == NULL)
    {
        return ESP_FAIL; // Falha se não houver memória RAM suficiente para criar o Mutex
    }

    // 4. Configuração do Timer do LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,            // Modo de baixa velocidade (padrão seguro)
        .duty_resolution = led->config.duty_resolution, // Ex: 8 bits (0-255)
        .timer_num = led->config.timer_num,           // Qual timer interno será usado
        .freq_hz = LEDC_FREQ,                         // Frequência de 5kHz
        .clk_cfg = LEDC_AUTO_CLK,                     // Escolhe o clock automaticamente
    };

    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK)
    {
        vSemaphoreDelete(led->mutex); // Se der erro, limpa o Mutex criado antes de sair
        return err;
    }

    // 5. Configuração dos Canais do LEDC (Um para cada cor)
    // Prepara a estrutura base que será reaproveitada para as 3 cores
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = led->config.timer_num,
        .duty = 0,     // Inicia com o LED apagado (0%)
        .hpoint = 0,   // Ponto alto do sinal PWM (padrão é 0)
    };

    // Configura e inicializa o Canal Vermelho
    ledc_channel.channel = led->config.red_channel;
    ledc_channel.gpio_num = led->config.red_pin;
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK)
    {
        vSemaphoreDelete(led->mutex);  //DELETA O MUTEX EM CASO DE NAO CONSEGUIR CONFIGURAR O CANAL PWM
        return err;
    }

    // Configura e inicializa o Canal Azul
    ledc_channel.channel = led->config.blue_channel;
    ledc_channel.gpio_num = led->config.blue_pin;
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK)
    {
        vSemaphoreDelete(led->mutex);
        return err;
    }

    // Configura e inicializa o Canal Verde
    ledc_channel.channel = led->config.green_channel;
    ledc_channel.gpio_num = led->config.green_pin;
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK)
    {
        vSemaphoreDelete(led->mutex);
        return err;
    }

    return err; // Retorna ESP_OK se tudo deu certo
}

esp_err_t led_rgb_set_color(led_rgb_t *led, uint32_t red, uint32_t green, uint32_t blue)
{
    if (!led)
    {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: verificar os valores RGB (Garantir que não passem de 2^duty_resolution - 1)

    // 1. Bloqueia o acesso ao LED (Mutex)
    // TODO: verificar se pegou de fato o mutex ou se deu erro (definir timeout ao invés de portMAX_DELAY)
    xSemaphoreTake(led->mutex, portMAX_DELAY);

    // --- Atualiza a Cor Vermelha ---
    // Seta o valor do duty cycle na memória
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, led->config.red_channel, red);
    if (err != ESP_OK)
    {
        xSemaphoreGive(led->mutex); // Libera o mutex em caso de erro
        return err;
    }
    // Aplica efetivamente o valor no hardware
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, led->config.red_channel);
    if (err != ESP_OK)
    {
        xSemaphoreGive(led->mutex);
        return err;
    }

    // --- Atualiza a Cor Verde ---
    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, led->config.green_channel, green);
    if (err != ESP_OK)
    {
        xSemaphoreGive(led->mutex);
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, led->config.green_channel);
    if (err != ESP_OK)
    {
        xSemaphoreGive(led->mutex);
        return err;
    }

    // --- Atualiza a Cor Azul ---
    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, led->config.blue_channel, blue);
    if (err != ESP_OK)
    {
        xSemaphoreGive(led->mutex);
        return err;
    }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, led->config.blue_channel);
    if (err != ESP_OK)
    {
        xSemaphoreGive(led->mutex);
        return err;
    }

    // 2. Libera o acesso ao LED para outras tarefas
    xSemaphoreGive(led->mutex);
    return ESP_OK;
}

esp_err_t led_deinit(led_rgb_t *led)
{
    if (!led)
    {
        return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: verificar se foi deletado;
    
    // Deleta o Mutex criado no init, liberando a RAM
    vSemaphoreDelete(led->mutex);

    // Para a geração de PWM no hardware para cada canal, garantindo que o LED apague
    esp_err_t err = ledc_stop(LEDC_LOW_SPEED_MODE, led->config.green_channel, 0);
    if (err != ESP_OK)
    {
        return err;
    }

    err = ledc_stop(LEDC_LOW_SPEED_MODE, led->config.red_channel, 0);
    if (err != ESP_OK)
    {
        return err;
    }

    err = ledc_stop(LEDC_LOW_SPEED_MODE, led->config.blue_channel, 0);
    if (err != ESP_OK)
    {
        return err;
    }
    
    // TODO: desalocar config tbm =O 
    // (Nota: Como a config não foi alocada dinamicamente com malloc, não precisa de free() aqui!)

    return ESP_OK;
}