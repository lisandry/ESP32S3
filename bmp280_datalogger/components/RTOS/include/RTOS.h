/**
 * @file RTOS.h
 * @brief Declarações e estruturas do sistema operacional em tempo real (RTOS).
 *
 * Este arquivo define as estruturas de dados para logs e telemetria, enumerações, 
 * variáveis globais e os protótipos de todas as tarefas e funções de controle 
 * baseadas no FreeRTOS para a aplicação de datalogger.
 */

#ifndef RTOS_H
#define RTOS_H  

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bmp280.h"
#include "file_system.h"
#include "timers_rtos.h"
#include "esp_spiffs.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

/* ========================================================================== */
/* VARIÁVEIS GLOBAIS                                                          */
/* ========================================================================== */

/**
 * @brief Flag global que indica se o RTOS foi inicializado e as tarefas estão ativas.
 * @note Deve ser instanciada no arquivo RTOS.c como: bool g_is_system_running = false;
 */
extern bool g_is_system_running;

/**
 * @brief Contador global para registrar a ocorrência de erros no sistema 
 * (como timeouts de fila, falhas de I2C, etc).
 * @note Deve ser instanciada no arquivo RTOS.c como: uint32_t g_system_error_count = 0;
 */
extern uint32_t g_system_error_count;

/* ========================================================================== */
/* DEFINIÇÕES E MACROS                                                        */
/* ========================================================================== */

/**
 * @def MAX_MONITORED_TASKS
 * @brief Número máximo de tarefas que serão monitoradas pela rotina de diagnóstico.
 */
#define MAX_MONITORED_TASKS     10  

/* ========================================================================== */
/* ENUMERAÇÕES                                                                */
/* ========================================================================== */

/**
 * @brief Níveis de severidade para classificação das mensagens de log.
 */
typedef enum {
    LOG_INFO,   /**< Informação geral sobre o fluxo do sistema. */
    LOG_WARN,   /**< Aviso de condição atípica, mas não impeditiva. */
    LOG_ERROR,  /**< Falha crítica ou erro na execução de uma rotina. */
    LOG_SENSOR, /**< Dado bruto ou convertido de medição de sensor. */
    LOG_STATUS  /**< Dados de diagnóstico, telemetria e estado das tarefas. */
} log_level_t;


/* ========================================================================== */
/* ESTRUTURAS DE DADOS                                                        */
/* ========================================================================== */

/**
 * @brief Estrutura que representa um pacote individual de mensagem de log.
 * * É utilizada para trafegar dados entre as tarefas geradoras (sensores, status)
 * e a tarefa consumidora (logger) através da fila xLogQueue.
 */
typedef struct {
    uint64_t timestamp; /**< Tempo em microssegundos desde a inicialização do ESP32. */
    log_level_t level;  /**< Nível de severidade da mensagem. */
    char tag[16];       /**< Identificador da fonte do log (Ex: "BMP280", "SYS"). */
    char message[128];  /**< Conteúdo textual detalhado do evento. */
} log_entry_t;

/**
 * @brief Estrutura para armazenar as métricas de saúde de uma única tarefa.
 */
typedef struct {
    char task_name[16];            /**< Nome registrado da tarefa. */
    eTaskState status;             /**< Estado atual (Running, Ready, Blocked, Suspended, Deleted). */
    uint32_t stack_high_watermark; /**< Quantidade mínima de memória de pilha livre já atingida. */
} task_monitor_t;

/**
 * @brief Estrutura que engloba toda a telemetria do sistema em um dado instante.
 * * Enviada periodicamente via fila (xStatusQueue) para que o sistema de arquivos 
 * registre uma foto instantânea (snapshot) do estado geral do microcontrolador.
 */
typedef struct {
    uint64_t uptime_us;          /**< Tempo contínuo de operação do sistema em microssegundos. */
    uint32_t heap_free;          /**< Memória RAM livre atual disponível no ESP32. */
    uint32_t heap_min_ever;      /**< Menor valor histórico de RAM livre registrada. */
    uint8_t  reset_reason;       /**< Motivo pelo qual a placa reiniciou na última vez. */
    uint8_t  log_queue_usage;    /**< Quantidade de mensagens aguardando processamento na fila de log. */
    uint8_t  status_queue_usage; /**< Quantidade de mensagens aguardando processamento na fila de status. */
    uint16_t total_tasks_count;  /**< Total de tarefas do FreeRTOS alocadas no momento. */
    task_monitor_t tasks_info[MAX_MONITORED_TASKS]; /**< Tabela com dados das tarefas individuais monitoradas. */
} status_entry_t;


/* ========================================================================== */
/* PROTÓTIPOS DE FUNÇÕES (AUXILIARES E DE CONVERSÃO)                          */
/* ========================================================================== */

/**
 * @brief Retorna uma string descritiva em português para o estado de uma tarefa.
 * @param estado Enum nativo do FreeRTOS representando a situação atual da tarefa.
 * @return String constante formatada (ex: "Executando", "Bloqueada").
 */
const char* estadoTarefa(eTaskState estado);

/**
 * @brief Converte o nível numérico do log em uma string curta.
 * @param nivel Valor vindo da enumeração log_level_t.
 * @return String constante formatada (ex: "INFO", "ERROR").
 */
const char* nivel_para_string(log_level_t nivel);


/* ========================================================================== */
/* PROTÓTIPOS DE FUNÇÕES (TASKS E SETUP DO RTOS)                              */
/* ========================================================================== */

/**
 * @brief Configura e inicia todos os recursos nativos do RTOS (Filas, Semáforos e Tarefas).
 */
void rtos_system_init(void);

/**
 * @brief Tarefa responsável por fazer varredura nos sensores de hardware (BMP280, ADC).
 * @param pvParameters Ponteiro de configuração (padrão FreeRTOS).
 */
void task_sensores(void *pvParameters);

/**
 * @brief Tarefa consumidora responsável por gravar definitivamente mensagens de logs 
 * e relatórios de status no sistema de arquivos da memória Flash.
 * @param pvParameters Ponteiro de configuração (padrão FreeRTOS).
 */
void task_logger(void *pvParameters);

/**
 * @brief Tarefa responsável por coletar dados de telemetria do sistema (memória, processamento).
 * @param pvParameters Ponteiro de configuração (padrão FreeRTOS).
 */
void task_status(void *pvParameters);

/**
 * @brief Utilitário genérico que empacota e envia mensagens formatadas para a fila de logs.
 * @param level A severidade que a mensagem vai carregar.
 * @param tag A assinatura/etiqueta do módulo responsável (ex: "I2C").
 * @param msg Texto descrevendo o evento ou leitura ocorrida.
 */
void send_to_logger(log_level_t level, const char* tag, const char* msg);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_H */