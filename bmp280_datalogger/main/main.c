/**
 * @file main.c
 * @brief Ponto de entrada (Entry Point) do projeto ESP-IDF.
 *
 * Este arquivo contém a função principal da aplicação. No ambiente de desenvolvimento 
 * ESP-IDF, o escalonador (scheduler) do FreeRTOS já é inicializado automaticamente 
 * em background antes mesmo da `app_main` ser chamada. Portanto, a responsabilidade 
 * deste arquivo é puramente delegar o fluxo para a nossa rotina customizada de 
 * inicialização do sistema.
 */

#include "RTOS.h"

/* ========================================================================== */
/* FUNÇÃO PRINCIPAL                                                           */
/* ========================================================================== */

/**
 * @brief Ponto de partida do firmware.
 * * Chama a função genérica `rtos_system_init()`, que é a verdadeira responsável por 
 * orquestrar toda a alocação de filas, semáforos, tarefas, inicialização do SPIFFS 
 * e configuração das interrupções de hardware (Timers e GPIO) do projeto.
 */
void app_main(void) {
    rtos_system_init();
}