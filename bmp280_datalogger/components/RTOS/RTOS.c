/**
 * @file RTOS.c
 * @brief Implementação das tarefas, filas e semáforos do sistema baseado em FreeRTOS.
 * * Este arquivo contém o código-fonte que orquestra as tarefas do sistema: leitura 
 * de sensores, gravação de logs (SPIFFS), monitoramento de status da placa e 
 * interface serial (impressão sob demanda acionada por botão).
 */

#include "RTOS.h"

/* ========================================================================== */
/* VARIÁVEIS GLOBAIS (Instanciação)                                           */
/* ========================================================================== */

bool g_is_system_running = false;
uint32_t g_system_error_count = 0;

/* ========================================================================== */
/* MACROS E CONFIGURAÇÕES                                                     */
/* ========================================================================== */

/* --- Configurações de Hardware --- */
#define SENSOR_READ_INTERVAL_MS 500
#define STATUS_UPDATE_INTERVAL_MS 2000

/* --- Configurações de Software (RTOS) --- */
#define LOG_QUEUE_SIZE          50
#define STATUS_QUEUE_SIZE       5
#define TASK_STACK_SIZE         4096
#define MUTEX_WAIT_TIMEOUT_MS   100

/* --- Buffers e Strings --- */
#define LOG_BUFFER_SIZE         256
#define STATUS_BUFFER_SIZE      1024
#define TAG_INTERNAL            "SYS"
#define TAG_SENSOR              "TASK_SENSOR"

/* --- CONFIGURAÇÕES DO SISTEMA (CONSTANTES MÁGICAS) --- */
#define MUTEX_TIMEOUT_TICKS     pdMS_TO_TICKS(100)
#define SENSOR_INTERVAL_TICKS   pdMS_TO_TICKS(500)
#define STATUS_INTERVAL_TICKS   pdMS_TO_TICKS(2000)

/* --- BUFFERS --- */
#define SMALL_BUF_SIZE          256    // Para logs individuais
#define LARGE_BUF_SIZE          1024   // Para o relatório completo de status

/* ========================================================================== */
/* HANDLES DO FREERTOS E HARDWARE                                             */
/* ========================================================================== */

static bmp280_t bmp280_handle;

QueueHandle_t xLogQueue;
QueueHandle_t xStatusQueue;

TaskHandle_t xTaskSensoresHandle;
TaskHandle_t xTaskLoggerHandle;
TaskHandle_t xTaskStatusHandle;
TaskHandle_t xTaskSerialHandle;

SemaphoreHandle_t xButtonSemaphore;
SemaphoreHandle_t xSpiffMutex;

// Resolução do endereço do barramento I2C via Kconfig
#ifdef CONFIG_BMP280_I2C_ADDR_0x76
    #define I2C_ADDR BMP280_I2C_ADDRESS_0
#else
    #define I2C_ADDR BMP280_I2C_ADDRESS_1
#endif

/* ========================================================================== */
/* FUNÇÕES AUXILIARES                                                         */
/* ========================================================================== */

/**
 * @brief Converte o nível numérico do log para uma string.
 */
const char* nivel_para_string(log_level_t nivel) {
    switch (nivel) {
        case LOG_INFO:   return "INFO";
        case LOG_WARN:   return "WARN";
        case LOG_ERROR:  return "ERROR";
        case LOG_SENSOR: return "SENSOR";
        case LOG_STATUS: return "STATUS";
        default:         return "UNKNOWN";
    }
}

/**
 * @brief Converte o estado nativo do FreeRTOS para uma string em português.
 */
const char* estadoTarefa(eTaskState estado) {
    switch (estado) {
        case eRunning:   return "Executando";
        case eReady:     return "Pronta";
        case eBlocked:   return "Bloqueada";
        case eSuspended: return "Suspensa";
        case eDeleted:   return "Deletada";
        default:         return "Desconhecido";
    }
}

/**
 * @brief Empacota uma mensagem de log e a insere na fila de processamento.
 * * @param level Nível de severidade da mensagem.
 * @param tag Assinatura do módulo gerador do log.
 * @param msg Mensagem textual.
 */
void send_to_logger(log_level_t level, const char* tag, const char* msg) {
    log_entry_t entry; 
    
    entry.timestamp = esp_timer_get_time();
    entry.level = level;
    
    strlcpy(entry.tag, tag, sizeof(entry.tag)); // Copia a tag real
    strlcpy(entry.message, msg, sizeof(entry.message));

    if (xQueueSend(xLogQueue, &entry, pdMS_TO_TICKS(10)) != pdPASS) {
        // Tabela 2: Evento de transbordo da fila de logs
        ESP_LOGE("TASK", "[TASK] Overflow: Buffer de mensagens cheio"); 
        g_system_error_count++; // Incrementa contador global de erros
    }
}

/**
 * @brief Envia a estrutura de telemetria preenchida para a fila de status.
 * * @param status Ponteiro para a estrutura populada pela task_status.
 */
void send_status_to_queue(status_entry_t *status) {
    // Tenta enviar para a fila correta: xStatusQueue
    if (xQueueSend(xStatusQueue, status, pdMS_TO_TICKS(10)) != pdPASS) {
        g_system_error_count++; // Incrementa erro global
        send_to_logger(LOG_WARN, "Status Queue Overflow! Fila cheia.", "");
    }
}

/**
 * @brief Grava eventos internos críticos direto na memória Flash via SPIFFS.
 * * @param msg Texto descrevendo o evento interno.
 * @param level Nível de severidade do evento.
 */
void logger_internal_log(const char* msg, log_level_t level) {
    char file_msg_buffer[256];
    uint64_t now = esp_timer_get_time();

    // Formatação conforme padrão [LEVEL] [TAG] MSG
    snprintf(file_msg_buffer, sizeof(file_msg_buffer), "%010llu    %-10s %-15s %s\n", 
             now, 
             nivel_para_string(level), 
             "[LOG]", 
             msg);

    if (xSpiffMutex != NULL && xSemaphoreTake(xSpiffMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        spiffs_add_log(file_msg_buffer);
        xSemaphoreGive(xSpiffMutex);
    } else {
        g_system_error_count++;
    }
}

/* ========================================================================== */
/* TAREFAS (TASKS)                                                            */
/* ========================================================================== */

/**
 * @brief Task responsável pela inicialização e leitura contínua dos sensores (BMP280 e ADC).
 * @param pvParameters Parâmetro nulo padrão do FreeRTOS.
 */
void task_sensores(void *pvParameters) {
    float temperature, pressure;
    char buffer[128];
    esp_err_t err;

    ESP_LOGI(TAG_SENSOR, "Iniciando task_sensores...");

    /* --- INICIALIZAÇÃO DO ADC2 --- */
    ESP_LOGI(TAG_SENSOR, "Configurando hardware do ADC...");
    adc_oneshot_unit_handle_t adc2_handle;
    adc_oneshot_unit_init_cfg_t init_config2 = { .unit_id = ADC_UNIT_2, };
    adc_oneshot_new_unit(&init_config2, &adc2_handle);

    adc_oneshot_chan_cfg_t adc_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc2_handle, ADC_CHANNEL_3, &adc_config);

    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_2,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);

    /* --- INICIALIZAÇÃO DO BMP280 --- */
    ESP_LOGI(TAG_SENSOR, "Configurando hardware do BMP280...");

    bmp280_config_t sensor_cfg = {
        .sda_pin = CONFIG_BMP280_I2C_SDA_PIN,
        .scl_pin = CONFIG_BMP280_I2C_SCL_PIN,
        .i2c_port = CONFIG_BMP280_I2C_PORT_NUM,
        .i2c_addr = I2C_ADDR,
        .i2c_freq_hz = CONFIG_BMP280_I2C_FREQ_HZ,
        .mode = BMP280_MODE_NORMAL,
        .oversampling_temperature = BMP280_OVERSAMPLING_STANDARD,
        .oversampling_pressure = BMP280_OVERSAMPLING_STANDARD,
        .filter = BMP280_FILTER_4,
        .standby_time = BMP280_STANDBY_250_MS
    };
    
    err = bmp280_init(&bmp280_handle, &sensor_cfg);
    if (err == ESP_OK) {
        send_to_logger(LOG_INFO, "BMP280", "[BMP] Sensor inicializado com sucesso");
        bmp280_set_config(&bmp280_handle, &sensor_cfg);
    } else {
        send_to_logger(LOG_ERROR, "BMP280", "[BMP] Problema na inicializacao do sensor");
        g_system_error_count++;
        vTaskDelete(NULL); 
    }

    while (1) {
        // Aguarda liberação por parte do Timer (Timer_Sensores)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); 
        
        err = bmp280_read_measurements(&bmp280_handle, &temperature, &pressure);
        
        // --- Leitura do ADC ---
        int valor_raw = 0, voltagem_mv = 0;
        adc_oneshot_read(adc2_handle, ADC_CHANNEL_3, &valor_raw);
        if (cali_handle != NULL) adc_cali_raw_to_voltage(cali_handle, valor_raw, &voltagem_mv);
        float volts = voltagem_mv / 1000.0; 

        // Registro de Logs de Leitura
        if (err == ESP_OK) {
            snprintf(buffer, sizeof(buffer), "[BMP] Temperatura (C): %.2f Pressao (Pa): %.2f", temperature, pressure);
            send_to_logger(LOG_INFO, "BMP280", buffer);
        } else {
            send_to_logger(LOG_ERROR, "BMP280", "[BMP] Falha ao ler o sensor no barramento I2C.");
            g_system_error_count++;
        }

        if (volts > 0.0) {
            snprintf(buffer, sizeof(buffer), "[ADC] Leitura de tensao realizada: %.2f V", volts);
            send_to_logger(LOG_INFO, "ADC", buffer);
        } else {
            send_to_logger(LOG_ERROR, "ADC", "[ADC] Problemas na leitura da tensao");
            g_system_error_count++;
        }
    }
}

/**
 * @brief Task consumidora que escreve no Sistema de Arquivos (SPIFFS).
 * Consome a fila de logs (append) e de status (sobrescrita), protegidas por Mutex.
 * @param pvParameters Parâmetro nulo padrão do FreeRTOS.
 */
void task_logger(void *pvParameters) {
    log_entry_t received_entry;
    status_entry_t received_status;
    
    // Usamos buffers estáticos para evitar estouro de pilha (Stack Overflow)
    static char file_msg_buffer[SMALL_BUF_SIZE];
    static char status_report_buffer[LARGE_BUF_SIZE];

    logger_internal_log("Task_Logger iniciada e pronta para processamento.", LOG_INFO);

    while (1) {
        /* --- 1. PROCESSAMENTO DE LOGS (log.txt) --- */
        if (xQueueReceive(xLogQueue, &received_entry, pdMS_TO_TICKS(50)) == pdPASS) {
            
            if (xSpiffMutex != NULL && xSemaphoreTake(xSpiffMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                char level_str[16];
                char tag_str[32];
                
                snprintf(level_str, sizeof(level_str), "[%s]", nivel_para_string(received_entry.level));
                snprintf(tag_str, sizeof(tag_str), "[%s]", received_entry.tag);

                snprintf(file_msg_buffer, sizeof(file_msg_buffer), "%010llu    %-10s %-15s %s\n", 
                         received_entry.timestamp, 
                         level_str, 
                         tag_str, 
                         received_entry.message);
                
                if (spiffs_add_log(file_msg_buffer) != ESP_OK) {
                    ESP_LOGE(TAG_INTERNAL, "Erro critico ao escrever no log.txt");
                    g_system_error_count++;
                }
                
                xSemaphoreGive(xSpiffMutex);
            } else {
                g_system_error_count++; // Timeout no mutex
            }
        }

        /* --- 2. PROCESSAMENTO DE STATUS (status.txt) --- */
        if (xQueueReceive(xStatusQueue, &received_status, 0) == pdPASS) {
            
            if (xSpiffMutex != NULL && xSemaphoreTake(xSpiffMutex, MUTEX_TIMEOUT_TICKS) == pdTRUE) {
                int pos = 0;
                memset(status_report_buffer, 0, sizeof(status_report_buffer));

                pos += snprintf(status_report_buffer + pos, LARGE_BUF_SIZE - pos,
                         "\n--- MONITOR DE SISTEMA ---\n"
                         "Uptime: %llu us\n"
                         "Heap Livre: %lu B | Menor Heap: %lu B\n"
                         "Reset Reason: %d\n"
                         "Ocupacao Filas -> Log: %d | Status: %d\n"
                         "Total de Tarefas Ativas: %d\n"
                         "--------------------------\n",
                         received_status.uptime_us, 
                         (unsigned long)received_status.heap_free, 
                         (unsigned long)received_status.heap_min_ever,
                         received_status.reset_reason,
                         received_status.log_queue_usage, 
                         received_status.status_queue_usage,
                         received_status.total_tasks_count);

                for(int i = 0; i < MAX_MONITORED_TASKS; i++) {
                    if(received_status.tasks_info[i].task_name[0] == '\0') continue;

                    pos += snprintf(status_report_buffer + pos, LARGE_BUF_SIZE - pos,
                             "T: %-12s | Est: %-10s | Stack Min: %lu\n",
                             received_status.tasks_info[i].task_name,
                             estadoTarefa(received_status.tasks_info[i].status),
                             received_status.tasks_info[i].stack_high_watermark);
                }
                
                pos += snprintf(status_report_buffer + pos, LARGE_BUF_SIZE - pos, "--------------------------\n");

                if (spiffs_update_status(status_report_buffer) == ESP_OK) {
                    logger_internal_log("Arquivo status.txt atualizado na Flash.", LOG_INFO);
                } else {
                    ESP_LOGE(TAG_INTERNAL, "Erro ao atualizar status.txt");
                    g_system_error_count++;
                }
                
                xSemaphoreGive(xSpiffMutex);
            } else {
                g_system_error_count++; // Timeout no mutex
            }
        }
        
        // Pequeno delay para ceder CPU se as filas estiverem vazias
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief Task de diagnóstico e monitoramento do sistema operacional (RTOS).
 * Coleta métricas de CPU, Heap e filas e dispara um relatório consolidado.
 * @param pvParameters Parâmetro nulo padrão do FreeRTOS.
 */
void task_status(void *pvParameters) {
    status_entry_t status;
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    uint32_t ulTotalRunTime;
    char buffer[128];

    ESP_LOGI(TAG_INTERNAL, "Iniciando task_status...");

    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Aguarda Timer de Status

        status.reset_reason = esp_reset_reason();
        if (status.reset_reason != ESP_RST_UNKNOWN) {
            send_to_logger(LOG_INFO, "STATUS", "[STATUS] Reset lido com sucesso");
        } else {
            send_to_logger(LOG_ERROR, "STATUS", "[STATUS] Falha ao obter motivo do reset");
        }

        status.uptime_us = esp_timer_get_time();
        send_to_logger(LOG_INFO, "STATUS", "[STATUS] Coleta de uptime realizada");

        status.heap_free = esp_get_free_heap_size();
        status.heap_min_ever = esp_get_minimum_free_heap_size();
        
        if (status.heap_free < 65000) { 
            send_to_logger(LOG_WARN, "STATUS", "[STATUS] Alerta: Heap livre abaixo de 20%");
        }

        status.log_queue_usage = (uint8_t)uxQueueMessagesWaiting(xLogQueue);
        status.status_queue_usage = (uint8_t)uxQueueMessagesWaiting(xStatusQueue);
        
        if (status.log_queue_usage >= (LOG_QUEUE_SIZE * 0.8)) {
            send_to_logger(LOG_WARN, "STATUS", "[STATUS] Alerta: Fila de logs 80% cheia");
        }

        status.total_tasks_count = uxTaskGetNumberOfTasks();
        snprintf(buffer, sizeof(buffer), "[STATUS] %d tarefas ativas no momento", status.total_tasks_count);
        send_to_logger(LOG_INFO, "STATUS", buffer);

        uxArraySize = status.total_tasks_count;
        pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
        
        if(pxTaskStatusArray != NULL) {
            uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);
            memset(status.tasks_info, 0, sizeof(status.tasks_info));

            for (x = 0; x < MAX_MONITORED_TASKS && x < uxArraySize; x++) {
                strlcpy(status.tasks_info[x].task_name, pxTaskStatusArray[x].pcTaskName, 16);
                status.tasks_info[x].status = pxTaskStatusArray[x].eCurrentState;
                status.tasks_info[x].stack_high_watermark = pxTaskStatusArray[x].usStackHighWaterMark;

                if (status.tasks_info[x].stack_high_watermark < 200) {
                    send_to_logger(LOG_WARN, "STATUS", "[STATUS] Alerta: Stack overflow iminente");
                    g_system_error_count++;
                }
            }
            vPortFree(pxTaskStatusArray);
        }

        if (xQueueSend(xStatusQueue, &status, pdMS_TO_TICKS(10)) == pdPASS) {
            send_to_logger(LOG_INFO, "STATUS", "[STATUS] Diagnostico enviado com sucesso");
        } else {
            send_to_logger(LOG_ERROR, "STATUS", "[STATUS] Timeout ao enviar dados para fila");
            g_system_error_count++;
        }
    }
}

/**
 * @brief Task que aguarda interrupção de botão para fazer a leitura dos arquivos 
 * gravados na Flash e enviá-los de volta via Serial para visualização do usuário.
 * @param pvParameters Parâmetro nulo padrão do FreeRTOS.
 */
void task_serial(void *pvParameters) {
    char path[64];
    
    while (1) {
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE) {
            
            send_to_logger(LOG_INFO, "SERIAL", "[SERIAL] Interrupcao de botao recebida");
            
            if (xSpiffMutex != NULL && xSemaphoreTake(xSpiffMutex, portMAX_DELAY) == pdTRUE) {
                
                printf("\n--- CONTEÚDO DO STATUS.TXT ---\n");
                snprintf(path, sizeof(path), "%s/status.txt", SPIFFS_BASE_PATH);
                if (spiffs_read_file(path) != ESP_OK) {
                    send_to_logger(LOG_ERROR, "SERIAL", "[SERIAL] Falha ao abrir Status.txt para leitura");
                    g_system_error_count++;
                }

                printf("\n--- CONTEÚDO DO LOG.TXT ---\n");
                snprintf(path, sizeof(path), "%s/log.txt", SPIFFS_BASE_PATH);
                if (spiffs_read_file(path) != ESP_OK) {
                    send_to_logger(LOG_ERROR, "SERIAL", "[SERIAL] Falha ao abrir Log.txt para leitura");
                    g_system_error_count++;
                }
                
                xSemaphoreGive(xSpiffMutex); 
                send_to_logger(LOG_INFO, "SERIAL", "[SERIAL] Impressao dos arquivos concluida");
                
            } else {
                send_to_logger(LOG_ERROR, "SERIAL", "[SERIAL] Timeout ao enviar evento para logger");
                g_system_error_count++;
            }
        }
    }
}

/* ========================================================================== */
/* INICIALIZAÇÃO DO SISTEMA                                                   */
/* ========================================================================== */

/**
 * @brief Rotina principal que aloca recursos (Filas, Mutex, Semáforos), inicializa 
 * o subsistema de arquivos (SPIFFS) e cria todas as Tasks. Ao fim, liga os Timers.
 */
void rtos_system_init(void) {

    if (spiffs_system_init() == ESP_OK) {
        spiffs_init_default_files();
    } else {
        ESP_LOGE(TAG_INTERNAL, "Falha Crítica: SPIFFS não inicializado.");
        g_system_error_count++;
    }

    xLogQueue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_entry_t));
    xStatusQueue = xQueueCreate(STATUS_QUEUE_SIZE, sizeof(status_entry_t));

    xButtonSemaphore = xSemaphoreCreateBinary();
    xSpiffMutex = xSemaphoreCreateMutex();

    if (xLogQueue == NULL || xStatusQueue == NULL || xButtonSemaphore == NULL || xSpiffMutex == NULL) {
        ESP_LOGE(TAG_INTERNAL, "Erro crítico ao alocar memória para as filas!");
        g_system_error_count++;
        return; 
    }

    // Cria as tarefas, associando seus respectivos Handles
    xTaskCreate(task_sensores, "task_sensores", TASK_STACK_SIZE, NULL, 5, &xTaskSensoresHandle);
    xTaskCreate(task_logger,   "task_logger",   TASK_STACK_SIZE, NULL, 4, &xTaskLoggerHandle);
    xTaskCreate(task_status,   "task_status",   TASK_STACK_SIZE, NULL, 3, &xTaskStatusHandle);
    xTaskCreate(task_serial,  "task_serial",   TASK_STACK_SIZE, NULL, 2, &xTaskSerialHandle);    

    timers_system_init(xTaskSensoresHandle, xTaskStatusHandle, xButtonSemaphore); 
    
    // Sucesso geral na inicialização
    g_is_system_running = true;
}