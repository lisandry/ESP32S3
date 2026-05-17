/**
 * @file timers_rtos.h
 * @brief Declarações para configuração e inicialização dos temporizadores de hardware.
 *
 * Este arquivo define a interface para a inicialização dos timers de propósito geral 
 * (GPTIMER) do ESP32. Esses temporizadores atuarão como a base de tempo estrita para 
 * liberar a execução das tarefas do RTOS (como leitura de sensores e envio de status),
 * além de lidar com interrupções de hardware (como o botão da interface serial).
 */

#ifndef TIMERS_RTOS_H
#define TIMERS_RTOS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" /* Adicionado para o compilador reconhecer o SemaphoreHandle_t */

/* ========================================================================== */
/* PROTÓTIPOS DE FUNÇÕES                                                      */
/* ========================================================================== */

/**
 * @brief Inicializa os GPTIMERs e os vincula às tarefas do RTOS e interrupções.
 * * Esta função configura os temporizadores de hardware para gerar interrupções 
 * periódicas. Dentro das ISRs (Interrupt Service Routines) desses timers, 
 * notificações (via `vTaskNotifyGiveFromISR`) ou semáforos são disparados 
 * para destravar as tarefas principais no tempo correto.
 * * @param task_sensores Handle da tarefa de sensores (receberá notificação periódica).
 * @param task_status   Handle da tarefa de status (receberá notificação periódica).
 * @param btn_semaphore Handle do semáforo binário do botão (para gerenciar a rotina da Serial).
 * * @return esp_err_t 
 * - ESP_OK: Se todos os temporizadores forem criados, configurados e iniciados com sucesso.
 * - ESP_FAIL (ou outro código de erro): Caso falhe a alocação ou inicialização no hardware.
 */
esp_err_t timers_system_init(TaskHandle_t task_sensores, TaskHandle_t task_status, SemaphoreHandle_t btn_semaphore);


#ifdef __cplusplus
}
#endif

#endif /* TIMERS_RTOS_H */