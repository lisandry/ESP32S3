/**
 * @file file_system.h
 * @brief Gerenciamento do sistema de arquivos SPIFFS no ESP32.
 * * Este arquivo contém as definições, variáveis globais e declarações 
 * das funções utilizadas para inicializar, ler, escrever e anexar dados 
 * no sistema de arquivos embutido na memória Flash (SPIFFS).
 */

#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H  

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdbool.h>

/**
 * @def SPIFFS_BASE_PATH
 * @brief Caminho base (ponto de montagem) para o sistema de arquivos SPIFFS.
 */
#define SPIFFS_BASE_PATH "/spiffs"

/* ========================================================================== */
/* VARIÁVEIS GLOBAIS                                                          */
/* ========================================================================== */

/**
 * @brief Indica se o sistema de arquivos SPIFFS foi inicializado e montado com sucesso.
 * @note Deve ser instanciada no file_system.c como: bool g_spiffs_is_mounted = false;
 */
extern bool g_spiffs_is_mounted;

/**
 * @brief Contador global de logs gravados com sucesso no sistema de arquivos.
 * @note Deve ser instanciada no file_system.c como: uint32_t g_total_logs_written = 0;
 */
extern uint32_t g_total_logs_written;


/* ========================================================================== */
/* PROTÓTIPOS DAS FUNÇÕES                                                     */
/* ========================================================================== */

/**
 * @brief Inicializa e monta o sistema de arquivos SPIFFS.
 * * @return 
 * - ESP_OK: Sucesso na montagem do SPIFFS.
 * - ESP_FAIL: Falha geral na montagem do particionamento.
 * - Outros códigos de erro padrão do ESP-IDF em caso de falha.
 */
esp_err_t spiffs_system_init(void);

/**
 * @brief Desmonta e desinicializa o sistema de arquivos SPIFFS, liberando recursos.
 */
void spiffs_system_deinit(void);

/**
 * @brief Sobrescreve um arquivo com novos dados. Caso o arquivo não exista, ele será criado.
 * * @param path Caminho completo do arquivo (ex: "/spiffs/arquivo.txt").
 * @param data String terminada em nulo '\0' contendo os dados a serem escritos.
 * @return esp_err_t Retorna ESP_OK em caso de sucesso na escrita.
 */
esp_err_t spiffs_write_file(const char *path, const char *data);

/**
 * @brief Anexa novos dados ao final de um arquivo existente. Se o arquivo não existir, será criado.
 * * @param path Caminho completo do arquivo alvo.
 * @param data String terminada em nulo '\0' contendo os dados a serem anexados.
 * @return esp_err_t Retorna ESP_OK em caso de sucesso ao adicionar os dados.
 */
esp_err_t spiffs_append_file(const char *path, const char *data);

/**
 * @brief Lê o conteúdo de um arquivo inteiro e o imprime via porta serial/console.
 * * @param path Caminho completo do arquivo a ser lido.
 * @return esp_err_t Retorna ESP_OK se lido com sucesso, ou erro se não conseguir abrir o arquivo.
 */
esp_err_t spiffs_read_file(const char *path);

/**
 * @brief Cria os arquivos 'log.txt' e 'status.txt' com cabeçalhos ou valores iniciais padrão.
 * * @return esp_err_t Retorna ESP_OK em caso de sucesso na criação de ambos os arquivos.
 */
esp_err_t spiffs_init_default_files(void);

/**
 * @brief Adiciona uma nova mensagem ao arquivo contínuo de logs (log.txt).
 * * @param log_message String contendo a linha de log a ser registrada.
 * @return esp_err_t Retorna ESP_OK se o log foi gravado corretamente.
 */
esp_err_t spiffs_add_log(const char *log_message);

/**
 * @brief Atualiza o arquivo de status (status.txt). Diferente do log, esta função 
 * sobrescreve o arquivo inteiro com os novos dados de diagnóstico.
 * * @param status_message String formatada contendo o painel completo de status do sistema.
 * @return esp_err_t Retorna ESP_OK se o arquivo for atualizado com sucesso.
 */
esp_err_t spiffs_update_status(const char *status_message);

#ifdef __cplusplus
}
#endif 

#endif /* FILE_SYSTEM_H */