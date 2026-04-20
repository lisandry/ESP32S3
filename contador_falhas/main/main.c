/**
 * @file main.c
 * @brief Practical Challenge: Failure Counter using NVS on the ESP32.
 *
 * This code implements a safety system for an industrial machine.
 * It uses the Non-Volatile Storage (NVS) to count how many times the system 
 * was prematurely restarted. If the system restarts 3 times in a row without 
 * reaching the stability time, it enters a safety lockout mode.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

/**
 * @def TAG
 * @brief Tag used for identifying log messages in the terminal.
 */
#define TAG "machine"

/**
 * @brief Main entry point of the application (Main Task).
 *
 * This function performs the following steps:
 * 1. Initializes the NVS partition.
 * 2. Reads the "err_count" key to check the reboot history.
 * 3. Locks the system in an infinite loop if the counter reaches 3 failures.
 * 4. Increments and saves the current failure.
 * 5. Waits 10 seconds for stability to clear the counter.
 * 6. Starts the normal machine operation loop.
 * * @note The Watchdog Timer (WDT) is avoided in infinite loops by using vTaskDelay.
 * @warning The system will remain locked unless the NVS is cleared externally.
 */
void app_main(void)
{
    /// @brief Variable to store error codes returned by ESP-IDF APIs
    esp_err_t err = nvs_flash_init();
    
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    ESP_LOGI(TAG, "\nOpening Non-Volatile Storage (NVS) handle...");
    
    /// @brief Handle to access the "storage" namespace in NVS
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }

    /// @brief Failure counter (early reboots) read from memory
    int32_t err_count = 0;
    
    // Read error count from NVS
    err = nvs_get_i32(my_handle, "err_count", &err_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // If the key does not exist (first time running), initialize with 0
        err_count = 0;
    } else {
        ESP_ERROR_CHECK(err);
    }

    /* -------------------------------------------------------------------------
     * SAFETY CHECK
     * ------------------------------------------------------------------------- */
    if (err_count >= 3) {
        ESP_LOGE(TAG, "SYSTEM LOCKED");
        nvs_close(my_handle);
        
        // Locks in an infinite loop (Safe Mode)
        while(1) {
            // vTaskDelay is important even in a while(1) to avoid triggering the Watchdog Timer (WDT)
            vTaskDelay(pdMS_TO_TICKS(1000)); 
        }
    }

    /* -------------------------------------------------------------------------
     * FAILURE REGISTRATION (CURRENT BOOT)
     * ------------------------------------------------------------------------- */
    // Increments the error counter
    err_count++;
    ESP_LOGI(TAG, "Boot number: %ld. Saving to NVS...", err_count);
    err = nvs_set_i32(my_handle, "err_count", err_count);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(my_handle); // Physically saves to flash
    ESP_ERROR_CHECK(err);

    /* -------------------------------------------------------------------------
     * STABILITY PERIOD
     * ------------------------------------------------------------------------- */
    ESP_LOGI(TAG, "Waiting 10 seconds for stability...");
    vTaskDelay(pdMS_TO_TICKS(10000)); // Delay adjusted to 10 seconds (10000 ms)

    ESP_LOGI(TAG, "System stable for 10s. Clearing the failure counter.");
    err = nvs_set_i32(my_handle, "err_count", 0);
    ESP_ERROR_CHECK(err);

    err = nvs_commit(my_handle);
    ESP_ERROR_CHECK(err);

    // Closes the NVS since boot configuration is complete
    nvs_close(my_handle);

    /* -------------------------------------------------------------------------
     * NORMAL OPERATION
     * ------------------------------------------------------------------------- */
    ESP_LOGI(TAG, "Starting normal operation of the industrial machine...");
    while(1) {
        // Main loop of your application
        ESP_LOGI(TAG, "Machine operating...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}