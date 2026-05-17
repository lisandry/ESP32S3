/**
 * @file file_system.c
 * @brief Implementação das funções de gerenciamento do sistema de arquivos SPIFFS.
 * * Este arquivo contém as rotinas para inicializar a partição flash, gerenciar 
 * arquivos de texto (log.txt e status.txt) e realizar operações de leitura/escrita.
 */

#include <stdio.h>
#include <string.h>
#include "file_system.h"

/* ========================================================================== */
/* VARIÁVEIS GLOBAIS (Instanciação)                                           */
/* ========================================================================== */

bool g_spiffs_is_mounted = false;
uint32_t g_total_logs_written = 0;

/* ========================================================================== */
/* CONSTANTES PRIVADAS                                                        */
/* ========================================================================== */

static const char *TAG = "SPIFFS";

/* ========================================================================== */
/* IMPLEMENTAÇÃO DAS FUNÇÕES                                                  */
/* ========================================================================== */

/**
 * @brief Inicializa e monta o sistema de arquivos SPIFFS.
 * * Configura o ponto de montagem, rótulo da partição e formata a flash
 * automaticamente caso a montagem falhe no primeiro uso. Atualiza a variável
 * global g_spiffs_is_mounted em caso de sucesso.
 * * @return esp_err_t Retorna ESP_OK se inicializado com sucesso.
 */
esp_err_t spiffs_system_init(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = SPIFFS_BASE_PATH,
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        g_spiffs_is_mounted = false;
        return ret;
    }

    g_spiffs_is_mounted = true;
    return ESP_OK;
}

/**
 * @brief Desmonta o sistema de arquivos SPIFFS.
 * * Desregistra o VFS e libera a memória alocada pelo driver SPIFFS.
 * Atualiza a variável global g_spiffs_is_mounted para false.
 */
void spiffs_system_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing SPIFFS");
    esp_vfs_spiffs_unregister(NULL);
    g_spiffs_is_mounted = false;
}

/**
 * @brief Escreve dados em um arquivo, sobrescrevendo o conteúdo existente.
 * * @param path Caminho completo do arquivo.
 * @param data String de dados a ser gravada.
 * @return esp_err_t ESP_OK em caso de sucesso, ESP_FAIL se não puder abrir.
 */
esp_err_t spiffs_write_file(const char *path, const char *data) {
    ESP_LOGI(TAG, "Opening file");
    FILE* f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    ESP_LOGI(TAG, "File written");   
    return ESP_OK;
} 

/**
 * @brief Adiciona dados ao final de um arquivo existente.
 * * @param path Caminho completo do arquivo.
 * @param data String de dados a ser anexada.
 * @return esp_err_t ESP_OK em caso de sucesso, ESP_FAIL se não puder abrir.
 */
esp_err_t spiffs_append_file(const char *path, const char *data) {
    FILE *f = fopen(path, "a"); 
    
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para append");
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", data);
    fclose(f);
    return ESP_OK;
}

/**
 * @brief Lê um arquivo linha por linha e imprime no console via ESP_LOGI.
 * * @note Inclui um pequeno atraso (vTaskDelay) para evitar acionamento do Task Watchdog.
 * * @param path Caminho completo do arquivo a ser lido.
 * @return esp_err_t ESP_OK se a leitura for concluída, ESP_FAIL caso não encontre o arquivo.
 */
esp_err_t spiffs_read_file(const char *path){
    FILE *f = fopen(path, "r");
    
    if (f == NULL) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para leitura");
        return ESP_FAIL;
    }
    
    char line[128];
    ESP_LOGI(TAG, "--- Lendo arquivo %s ---", path);
    while (fgets(line, sizeof(line), f) != NULL) {
        // Removendo o \n do final para o ESP_LOGI não pular duas linhas
        line[strcspn(line, "\n")] = 0; 
        ESP_LOGI(TAG, "%s", line);
        vTaskDelay(pdMS_TO_TICKS(1)); // Respiro para o RTOS / Watchdog
    }
    ESP_LOGI(TAG, "-----------------------");

    fclose(f);
    return ESP_OK;
}

/**
 * @brief Prepara a estrutura básica de arquivos do sistema.
 * * Cria `status.txt` e `log.txt` na memória Flash contendo seus cabeçalhos iniciais.
 * * @return esp_err_t ESP_OK se os arquivos forem gerados com sucesso.
 */
esp_err_t spiffs_init_default_files(void) {
    char log_path[64];
    char status_path[64];

    snprintf(log_path, sizeof(log_path), "%s/log.txt", SPIFFS_BASE_PATH);
    snprintf(status_path, sizeof(status_path), "%s/status.txt", SPIFFS_BASE_PATH);

    ESP_LOGI(TAG, "Criando arquivos padrão do sistema...");

    esp_err_t err = spiffs_write_file(status_path, "STATUS: Inicializando sistema...\n");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar status.txt");
        return err;
    }

    err = spiffs_write_file(log_path, "--- LOG DE EVENTOS INICIADO ---\n");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar log.txt");
        return err;
    }

    ESP_LOGI(TAG, "Arquivos log.txt e status.txt gerados com sucesso.");
    return ESP_OK;
}

/**
 * @brief Encapsula a gravação de logs rotineiros no arquivo log.txt.
 * * Incrementa a variável global g_total_logs_written toda vez que um registro 
 * é gravado com sucesso.
 * * @param log_message Linha formatada de log para anexar ao arquivo.
 * @return esp_err_t ESP_OK em caso de sucesso na gravação.
 */
esp_err_t spiffs_add_log(const char *log_message) {
    char log_path[64];
    snprintf(log_path, sizeof(log_path), "%s/log.txt", SPIFFS_BASE_PATH);
    
    esp_err_t err = spiffs_append_file(log_path, log_message);
    if (err == ESP_OK) {
        g_total_logs_written++; // Incrementa o contador global de logs
    }
    return err;
}

/**
 * @brief Sobrescreve o arquivo status.txt com o pacote atualizado de diagnósticos.
 * * @param status_message Relatório completo do estado do sistema.
 * @return esp_err_t ESP_OK em caso de sucesso na gravação.
 */
esp_err_t spiffs_update_status(const char *status_message) {
    char status_path[64];
    snprintf(status_path, sizeof(status_path), "%s/status.txt", SPIFFS_BASE_PATH);
    
    return spiffs_write_file(status_path, status_message);
}