/**
 * @file bmp280.h
 * @brief I2C Driver for the Bosch BMP280 Temperature and Pressure Sensor.
 *
 * @note This driver is built on top of the ESP-IDF v5 I2C master API. It
 * extracts raw ADC values and applies the factory-programmed trimming 
 * parameters to output human-readable data (Celsius and Pascals).
 */

#ifndef BMP280_H
#define BMP280_H

#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* MACROS & GLOBALS                                                           */
/* ========================================================================== */

/** @brief I2C address when the SDO pin is tied to GND. */
#define BMP280_I2C_ADDRESS_0  0x76 

/** @brief I2C address when the SDO pin is tied to VDD. */
#define BMP280_I2C_ADDRESS_1  0x77 

/* ========================================================================== */
/* ENUMERATIONS                                                               */
/* ========================================================================== */

/**
 * @brief Power operation modes for the sensor.
 * * @details Choosing the right mode is critical for battery-powered applications. 
 * FORCED mode is ideal for low-power operation as the sensor sleeps natively, 
 * waking up only when the host requests a single measurement.
 */
typedef enum {
    BMP280_MODE_SLEEP  = 0x00, /**< Lowest power state, no measurements. */
    BMP280_MODE_FORCED = 0x01, /**< Performs one measurement, then sleeps. */
    BMP280_MODE_NORMAL = 0x03  /**< Continuous background measurements. */
} bmp280_mode_t;

/**
 * @brief Hardware oversampling settings.
 * * @details Defines how many internal reads the ADC performs to generate one output 
 * sample. Higher oversampling reduces noise (higher resolution) but increases 
 * current consumption and measurement time.
 */
typedef enum {
    BMP280_OVERSAMPLING_SKIP            = 0x00, /**< Skips measurement. */
    BMP280_OVERSAMPLING_ULTRA_LOW_POWER = 0x01, /**< 1x (Lowest power). */
    BMP280_OVERSAMPLING_LOW_POWER       = 0x02, /**< 2x. */
    BMP280_OVERSAMPLING_STANDARD        = 0x03, /**< 4x (Recommended). */
    BMP280_OVERSAMPLING_HIGH            = 0x04, /**< 8x. */
    BMP280_OVERSAMPLING_ULTRA_HIGH      = 0x05  /**< 16x (Highest precision). */
} bmp280_oversampling_t;

/**
 * @brief IIR Filter coefficients.
 * * @details The built-in Infinite Impulse Response (IIR) filter minimizes short-term 
 * disturbances in pressure readings (e.g., wind gusts or slamming doors).
 * Essential for indoor navigation or altitude holding in drones.
 */
typedef enum {
    BMP280_FILTER_OFF = 0x00, /**< Filter bypassed, raw data output. */
    BMP280_FILTER_2   = 0x01, /**< Reaches 75% step response in 2 samples. */
    BMP280_FILTER_4   = 0x02, /**< Reaches 75% step response in 5 samples. */
    BMP280_FILTER_8   = 0x03, /**< Reaches 75% step response in 11 samples. */
    BMP280_FILTER_16  = 0x04  /**< Reaches 75% step response in 22 samples. */
} bmp280_filter_t;

/**
 * @brief Standby timing for NORMAL mode.
 * * @details Determines the sleep duration between continuous automated measurements.
 * Used alongside oversampling to calculate the total data output rate.
 */
typedef enum {
    BMP280_STANDBY_05_MS   = 0x00, /**< 0.5 ms standby. */
    BMP280_STANDBY_62_MS   = 0x01, /**< 62.5 ms standby. */
    BMP280_STANDBY_125_MS  = 0x02, /**< 125 ms standby. */
    BMP280_STANDBY_250_MS  = 0x03, /**< 250 ms standby. */
    BMP280_STANDBY_500_MS  = 0x04, /**< 500 ms standby. */
    BMP280_STANDBY_1000_MS = 0x05, /**< 1000 ms standby. */
    BMP280_STANDBY_2000_MS = 0x06, /**< 2000 ms standby. */
    BMP280_STANDBY_4000_MS = 0x07  /**< 4000 ms standby. */
} bmp280_standby_time_t;

/* ========================================================================== */
/* DATA STRUCTURES                                                            */
/* ========================================================================== */

/**
 * @brief Hardware and operational configuration profile.
 * * @details Aggregates physical connection parameters (I2C pins, frequency) and 
 * logical behavior (filters, modes). Passing this single structure simplifies 
 * the API initialization contract.
 */
typedef struct {
    int sda_pin;           /**< GPIO pin mapped to the I2C SDA line. */
    int scl_pin;           /**< GPIO pin mapped to the I2C SCL line. */

    uint8_t i2c_port;      /**< Target I2C peripheral instance. */
    uint8_t i2c_addr;      /**< Target device address on the bus. */
    uint32_t i2c_freq_hz;  /**< SCL clock frequency. */

    bmp280_mode_t mode;                               /**< Initial power mode. */
    bmp280_oversampling_t oversampling_temperature;   /**< Temp resolution. */
    bmp280_filter_t filter;                           /**< IIR step response. */
    bmp280_standby_time_t standby_time;               /**< Sleep between reads. */
    bmp280_oversampling_t oversampling_pressure;      /**< Pressure resolution. */   
} bmp280_config_t;

/**
 * @brief Factory trimming parameters for data compensation.
 * * @details Sensors exhibit minor manufacturing variations. These parameters are 
 * read from the chip's Non-Volatile Memory (NVM) during initialization and 
 * are strictly required by the Bosch mathematical formulas to convert raw 
 * ADC data into accurate Celsius and Pascal values.
 */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    /* BME280 humidity compensation (ignored for BMP280) */
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} bmp280_calib_data;

/**
 * @brief Main device structure for runtime state and synchronization.
 * * @details Contains the user configuration for reference and a Mutex handle to 
 * ensure thread-safe access to the I2C bus and the global t_fine variable 
 * used in compensation calculations.
 */
typedef struct {
    bmp280_config_t config;             /**< Current hardware and logical configuration. */
    SemaphoreHandle_t mutex;            /**< Mutex for thread-safe access to the sensor. */
    int32_t t_fine;                     /**< Calibration bridge between temp and pressure. */
    bmp280_calib_data calibration;      /**< Unique factory trimming parameters. */
    
    i2c_master_bus_handle_t bus_handle; /**< I2C physical bus handle. */
    i2c_master_dev_handle_t dev_handle; /**< I2C device handle specific to this instance. */
} bmp280_t;

/* ========================================================================== */
/* PUBLIC API FUNCTIONS                                                       */
/* ========================================================================== */

/**
 * @brief Provisions the I2C bus and verifies sensor authenticity.
 * * @details Acts as the hardware entry point. It creates the I2C master bus, probes 
 * the designated address, verifies the WHO_AM_I chip ID (0x58), and downloads 
 * the trimming parameters (calibration data) needed for future compensations.
 * * @param bmp280 Pointer to the main device structure.
 * @param config Pointer to the configuration profile to be applied.
 * @return 
 * - ESP_OK: Hardware connected, authenticated, and calibrated.
 * - ESP_ERR_NOT_FOUND: Chip ID mismatch (hardware failure or wrong chip).
 * - ESP_ERR_INVALID_ARG: Null pointer passed as configuration.
 */
esp_err_t bmp280_init(bmp280_t *bmp280, const bmp280_config_t *config);

/**
 * @brief Writes operational parameters to the control registers.
 * * @details Translates the configuration struct enums into bitwise operations and 
 * dispatches them via I2C to the 'ctrl_meas' (0xF4) and 'config' (0xF5) 
 * registers. This dictates how the ADC processes the environment.
 * * @param bmp280 Pointer to the main device structure.
 * @param config Pointer to the structure containing the desired parameters.
 * @return 
 * - ESP_OK: Registers successfully updated.
 * - ESP_FAIL: I2C transmission error.
 */
esp_err_t bmp280_set_config(bmp280_t *bmp280, const bmp280_config_t *config);

/**
 * @brief Triggers a manual read cycle in FORCED mode.
 * * @details When operating in FORCED mode to save battery, the sensor remains asleep.
 * Calling this function overrides the mode register to trigger a single ADC 
 * conversion cycle. The sensor automatically returns to sleep afterward.
 * * @param bmp280 Pointer to the main device structure.
 * @return 
 * - ESP_OK: Measurement successfully triggered.
 */
esp_err_t bmp280_force_measurement(bmp280_t *bmp280);

/**
 * @brief Retrieves and compensates the latest temperature reading.
 * * @details Reads the 20-bit raw ADC temperature data and applies the manufacturer's 
 * compensation algorithm using the NVM trimming parameters. 
 * * @note Temperature must always be read before pressure, as the internal 
 * pressure compensation algorithm relies on the fine temperature factor 
 * (t_fine).
 * * @param bmp280 Pointer to the main device structure.
 * @param temperature Pointer to a float where the Celsius value will be stored.
 * @return 
 * - ESP_OK: Valid data processed and written to the pointer.
 */
esp_err_t bmp280_read_temperature(bmp280_t *bmp280, float *temperature);

/**
 * @brief Performs a burst read for synchronized environmental data.
 * * @details Highly recommended over individual reads. It executes a single I2C burst 
 * transaction to fetch both raw temperature and pressure registers. This 
 * ensures data consistency (prevents reading temperature from cycle N and 
 * pressure from cycle N+1).
 * * @param bmp280 Pointer to the main device structure.
 * @param temperature Pointer to store the compensated Celsius value.
 * @param pressure Pointer to store the compensated Pascals value.
 * @return 
 * - ESP_OK: Burst read and compensation successful.
 */
esp_err_t bmp280_read_measurements(bmp280_t *bmp280, float *temperature, float *pressure);

/**
 * @brief Safely tears down the I2C bus and releases memory.
 * * @details Prevents memory leaks during hot-reloads or deep sleep preparations. 
 * It removes the device instance and then deletes the master bus instance.
 * * @param bmp280 Pointer to the main device structure.
 * @return 
 * - ESP_OK: System resources cleanly released.
 */
esp_err_t bmp280_deinit(bmp280_t *bmp280);

#ifdef __cplusplus
}
#endif

#endif //BMP280_H