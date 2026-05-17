/**
 * @file timers_rtos.c
 * @brief Implementação da inicialização de temporizadores de hardware e interrupções (GPIO).
 *
 * Este arquivo contém a lógica para configurar e iniciar os temporizadores de propósito geral 
 * (GPTIMER) do ESP32, bem como a configuração da interrupção externa de hardware (botão). 
 * As ISRs (Interrupt Service Routines) aqui definidas atuam como "gatilhos" precisos 
 * que acordam as tarefas do RTOS bloqueadas, garantindo um ciclo de execução determinístico.
 */

#include "timers_rtos.h"
#include "driver/gptimer.h"
#include "driver/gpio.h" // Adicionado para o botão
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ========================================================================== */
/* MACROS E VARIÁVEIS LOCAIS                                                  */
/* ========================================================================== */

/**
 * @def PIN_BTN_ISR
 * @brief Define o pino GPIO utilizado para a interrupção externa do botão.
 */
#define PIN_BTN_ISR GPIO_NUM_0 

/**
 * @brief Tag utilizada para a impressão de logs nativos deste módulo.
 */
static const char *TAG = "TIMERS_HW";

/* ========================================================================== */
/* ROTINAS DE SERVIÇO DE INTERRUPÇÃO (ISRs)                                   */
/* ========================================================================== */

/**
 * @brief Rotina de Serviço de Interrupção (ISR) do Botão.
 * * Disparada por hardware na borda de descida do sinal do pino definido por PIN_BTN_ISR.
 * Sua função primária é liberar um semáforo binário que desbloqueia a tarefa serial 
 * encarregada de imprimir os dados na tela.
 * * @param arg Ponteiro de contexto (cast de volta para SemaphoreHandle_t).
 */
static void IRAM_ATTR btn_isr_handler(void* arg) {
    // Recupera o semáforo que foi passado no momento do registro
    SemaphoreHandle_t xBtnSemaphore = (SemaphoreHandle_t) arg; 
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Libera o semáforo de forma segura dentro de uma ISR
    xSemaphoreGiveFromISR(xBtnSemaphore, &xHigherPriorityTaskWoken);
    
    // Força uma troca de contexto imediata se uma tarefa de maior prioridade foi acordada
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Callback de interrupção do GPTimer de Sensores.
 * * Executada quando o temporizador atinge a contagem estipulada. Notifica 
 * a `task_sensores` via TaskNotify para iniciar uma nova varredura no BMP280 e ADC.
 * * @param timer Handle do timer que disparou o evento.
 * @param edata Estrutura com os dados do evento do alarme.
 * @param user_ctx O Handle da tarefa a ser acordada (TaskHandle_t passado no registro).
 * @return true Se a notificação exigiu a troca imediata de contexto pelo RTOS.
 * @return false Se nenhuma tarefa de prioridade maior foi destravada.
 */
static bool IRAM_ATTR sensor_timer_isr_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_awoken = pdFALSE;
    vTaskNotifyGiveFromISR((TaskHandle_t)user_ctx, &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}

/**
 * @brief Callback de interrupção do GPTimer de Status/Telemetria.
 * * Executada quando o temporizador atinge a contagem estipulada. Notifica 
 * a `task_status` via TaskNotify para coletar métricas do sistema e alocar na fila.
 * * @param timer Handle do timer que disparou o evento.
 * @param edata Estrutura com os dados do evento do alarme.
 * @param user_ctx O Handle da tarefa a ser acordada (TaskHandle_t passado no registro).
 * @return true Se a notificação exigiu a troca imediata de contexto pelo RTOS.
 * @return false Se nenhuma tarefa de prioridade maior foi destravada.
 */
static bool IRAM_ATTR status_timer_isr_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    BaseType_t high_task_awoken = pdFALSE;
    vTaskNotifyGiveFromISR((TaskHandle_t)user_ctx, &high_task_awoken);
    return (high_task_awoken == pdTRUE);
}

/* ========================================================================== */
/* INICIALIZAÇÃO DE HARDWARE                                                  */
/* ========================================================================== */

/**
 * @brief Inicializa e vincula as interrupções de GPIO e os GPTIMERs do ESP32.
 * * @param task_sensores Handle da tarefa de coleta (para o timer de sensores).
 * @param task_status Handle da tarefa de diagnóstico (para o timer de status).
 * @param btn_semaphore Semáforo a ser atrelado ao GPIO do botão físico.
 * @return esp_err_t Retorna ESP_OK em caso de sucesso na inicialização geral.
 */
esp_err_t timers_system_init(TaskHandle_t task_sensores, TaskHandle_t task_status, SemaphoreHandle_t btn_semaphore) {
    ESP_LOGI(TAG, "Inicializando Hardware (Timers e GPIO)...");

    /* ========================================================== */
    /* 1. CONFIGURAÇÃO DO BOTÃO (GPIO)                            */
    /* ========================================================== */
    gpio_config_t btn_cfg = {
        .intr_type = GPIO_INTR_NEGEDGE,        // Dispara ao pressionar o botão (borda de descida)
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PIN_BTN_ISR),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE       // Garante nível alto quando botão estiver solto
    };
    gpio_config(&btn_cfg);
    gpio_install_isr_service(0);
    
    // Passamos o btn_semaphore como argumento para o contexto da ISR!
    gpio_isr_handler_add(PIN_BTN_ISR, btn_isr_handler, (void*) btn_semaphore);

    /* ========================================================== */
    /* 2. CONFIGURAÇÃO DOS GPTIMERS                               */
    /* ========================================================== */
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,              // Resolução de 1MHz (1 tick = 1 microssegundo)
    };

    gptimer_handle_t sensor_timer = NULL;
    gptimer_handle_t status_timer = NULL;
    
    // Instancia os novos timers de hardware
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &sensor_timer));
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &status_timer));

    // Configura os alarmes com base no Menuconfig (convertendo milisegundos para microssegundos)
    gptimer_alarm_config_t sensor_alarm_config = {
        .alarm_count = CONFIG_SENSOR_TIMER_PERIOD_MS * 1000ULL, 
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    
    gptimer_alarm_config_t status_alarm_config = {
        .alarm_count = CONFIG_STATUS_TIMER_PERIOD_MS * 1000ULL,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(gptimer_set_alarm_action(sensor_timer, &sensor_alarm_config));
    ESP_ERROR_CHECK(gptimer_set_alarm_action(status_timer, &status_alarm_config));

    // Mapeia os Callbacks e registra os contextos (Task Handles)
    gptimer_event_callbacks_t sensor_cbs = { .on_alarm = sensor_timer_isr_cb };
    gptimer_event_callbacks_t status_cbs = { .on_alarm = status_timer_isr_cb };

    ESP_ERROR_CHECK(gptimer_register_event_callbacks(sensor_timer, &sensor_cbs, task_sensores));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(status_timer, &status_cbs, task_status));

    // Habilita e inicializa a contagem dos temporizadores
    ESP_ERROR_CHECK(gptimer_enable(sensor_timer));
    ESP_ERROR_CHECK(gptimer_start(sensor_timer));
    ESP_ERROR_CHECK(gptimer_enable(status_timer));
    ESP_ERROR_CHECK(gptimer_start(status_timer));

    return ESP_OK;
}