/**
 * @file bmp280.c
 * @brief I2C driver implementation for the Bosch BMP280 sensor.
 *
 * @note Contains internal register definitions, factory calibration 
 * routines, and the mathematical implementation of the 32-bit and 64-bit 
 * compensation formulas.
 */

#include "bmp280.h"

/* ========================================================================== */
/* INTERNAL REGISTER MAP (Hardware Registers)                                 */
/* ========================================================================== */

/** @brief Raw temperature data registers (20 bits) */
#define BMP280_REG_TEMP_XLSB   0xFC /*!< bits: 7-4 (XLSB) */
#define BMP280_REG_TEMP_LSB    0xFB /*!< bits: 7-0 (LSB) */
#define BMP280_REG_TEMP_MSB    0xFA /*!< bits: 7-0 (MSB) */
#define BMP280_REG_TEMP        (BMP280_REG_TEMP_MSB)

/** @brief Raw pressure data registers (20 bits) */
#define BMP280_REG_PRESS_XLSB  0xF9 /*!< bits: 7-4 (XLSB) */
#define BMP280_REG_PRESS_LSB   0xF8 /*!< bits: 7-0 (LSB) */
#define BMP280_REG_PRESS_MSB   0xF7 /*!< bits: 7-0 (MSB) */
#define BMP280_REG_PRESSURE    (BMP280_REG_PRESS_MSB)

/** @brief Configuration and control registers */
#define BMP280_REG_CONFIG      0xF5 /*!< bits: 7-5 t_sb; 4-2 filter; 0 spi */
#define BMP280_REG_CTRL        0xF4 /*!< bits: 7-5 osrs_t; 4-2 osrs_p; 1-0 mode */
#define BMP280_REG_STATUS      0xF3 /*!< bits: 3 measuring; 0 im_update */
#define BMP280_REG_CTRL_HUM    0xF2 /*!< bits: 2-0 osrs_h (BME280 only) */

/** @brief System and identification registers */
#define BMP280_REG_RESET       0xE0 /*!< Writing 0xB6 triggers a soft reset */
#define BMP280_REG_ID          0xD0 /*!< Contains the Chip ID (0x58) */
#define BMP280_REG_CALIB       0x88 /*!< Start address of NVM calibration data */
#define BMP280_RESET_VALUE     0xB6 /*!< Soft Reset command value */

/* ========================================================================== */
/* PRIVATE GLOBAL VARIABLES (Encapsulated with 'static')                      */
/* ========================================================================== */

/** @brief Tags for ESP-IDF terminal logging identification. */
static const char *TAG = "BMP280_DRV";

/* ========================================================================== */
/* PRIVATE FUNCTIONS                                                          */
/* ========================================================================== */

/**
 * @brief Reads factory trimming parameters from the sensor's NVM.
 *
 * @details Performs an I2C burst read of 24 bytes starting at address 0x88 
 * to reconstruct the 16-bit integers needed for Bosch's compensation formulas.
 *
 * @return 
 * - ESP_OK: Calibration data read and parsed successfully.
 * - Non-zero: I2C communication failure.
 */
static esp_err_t bmp280_read_calibration(bmp280_t *bmp280) {
    uint8_t reg_addr = BMP280_REG_CALIB;
    uint8_t data[24];

    esp_err_t err = i2c_master_transmit_receive(bmp280->dev_handle, &reg_addr, 1, 
                                                data, 24, 100);
    if (err != ESP_OK) 
    {
        ESP_LOGE("READ", "Failed to read calibration data");

        return err;
    }
    ESP_LOGE("READ", "1");

    /* Reconstruct 16-bit integers by combining MSB and LSB */
    bmp280->calibration.dig_T1 = (uint16_t)((data[1] << 8) | data[0]);
    bmp280->calibration.dig_T2 = (int16_t) ((data[3] << 8) | data[2]);
    bmp280->calibration.dig_T3 = (int16_t) ((data[5] << 8) | data[4]);

    bmp280->calibration.dig_P1 = (uint16_t)((data[7] << 8) | data[6]);
    bmp280->calibration.dig_P2 = (int16_t) ((data[9] << 8) | data[8]);
    bmp280->calibration.dig_P3 = (int16_t) ((data[11] << 8) | data[10]);
    bmp280->calibration.dig_P4 = (int16_t) ((data[13] << 8) | data[12]);
    bmp280->calibration.dig_P5 = (int16_t) ((data[15] << 8) | data[14]);
    bmp280->calibration.dig_P6 = (int16_t) ((data[17] << 8) | data[16]);
    bmp280->calibration.dig_P7 = (int16_t) ((data[19] << 8) | data[18]);
    bmp280->calibration.dig_P8 = (int16_t) ((data[21] << 8) | data[20]);
    bmp280->calibration.dig_P9 = (int16_t) ((data[23] << 8) | data[22]);

    ESP_LOGE("READ", "FIM");
    return ESP_OK;
}

/* ========================================================================== */
/* PUBLIC API IMPLEMENTATION                                                  */
/* ========================================================================== */

/**
 * @brief Initializes the I2C bus and registers the BMP280 device.
 *
 * @param config Pointer to the user-defined configuration profile.
 * @return esp_err_t ESP_OK on success, or specific error code.
 */
esp_err_t bmp280_init(bmp280_t *bmp280, const bmp280_config_t *config){
    esp_err_t err;
    if(!bmp280 || !config) {
        ESP_LOGE(TAG, "Invalid argument: NULL pointer provided.");
        return ESP_ERR_INVALID_ARG;
    }

    if(!GPIO_IS_VALID_GPIO(config->sda_pin) || 
       !GPIO_IS_VALID_GPIO(config->scl_pin)) {
        ESP_LOGE(TAG, "Invalid GPIO pins: SDA=%d, SCL=%d", config->sda_pin, config->scl_pin);
        return ESP_ERR_INVALID_ARG;
    }

    bmp280->config = *config; // Store user config in the struct for later use
    bmp280->t_fine = 0; // Reset compensation bridge variable
    bmp280->bus_handle = NULL; // Initialize bus handle to NULL
    bmp280->dev_handle = NULL; // Initialize device handle to NULL  
    
    bmp280->mutex = xSemaphoreCreateMutex();

    if (bmp280->mutex == NULL) {
        ESP_LOGE(TAG, "Error: Failed to create mutex.");
        return ESP_ERR_NO_MEM;
    }

    i2c_master_bus_config_t bus_config = {
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .i2c_port = config->i2c_port,
        .glitch_ignore_cnt = 7,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };

    err = i2c_new_master_bus(&bus_config, &bmp280->bus_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error: Failed to create I2C master bus.");
        return err;
    }
    
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_addr,
        .scl_speed_hz = config->i2c_freq_hz,
    };  

    err = i2c_master_bus_add_device(bmp280->bus_handle, &dev_config, &bmp280->dev_handle);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Error: Failed to add device to I2C bus.");
        return err;
    }

    err = bmp280_read_calibration(bmp280);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Error: Failed to read calibration data.");
        return err;
    }   

    return ESP_OK;
}

/**
 * @brief Applies oversampling, filter, and power mode settings.
 *
 * @param config Pointer to the struct containing user preferences.
 * @return esp_err_t ESP_OK if registers were successfully written.
 */
esp_err_t bmp280_set_config(bmp280_t *bmp280, const bmp280_config_t *config) {
    esp_err_t err;

    if(!bmp280 || !config) {
        ESP_LOGE(TAG, "Invalid argument: NULL pointer provided.");  
        return ESP_ERR_INVALID_ARG;
    }

    if(bmp280->dev_handle == NULL) {
        ESP_LOGE(TAG, "Invalid state: Device handle is NULL. Ensure bmp280_init() was successful.");
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(bmp280->mutex, portMAX_DELAY);

    uint8_t tx_buffer[2];

    /* Assemble CONFIG register byte (0xF5) using bitwise shifts */
    uint8_t reg_config_val = (config->standby_time << 5) | 
                             (config->filter << 2);
    
    tx_buffer[0] = BMP280_REG_CONFIG;
    tx_buffer[1] = reg_config_val;

    err = i2c_master_transmit(bmp280->dev_handle, tx_buffer, 2, -1);
    if (err != ESP_OK){
        xSemaphoreGive(bmp280->mutex);
        ESP_LOGE(TAG, "Error: Failed to write CONFIG register.");
        return err;
    }

    /* Assemble CTRL_MEAS register byte (0xF4) */
    uint8_t reg_ctrl_meas_val = (config->oversampling_temperature << 5) | 
                                (config->oversampling_pressure << 2) | 
                                (config->mode);

    tx_buffer[0] = BMP280_REG_CTRL; 
    tx_buffer[1] = reg_ctrl_meas_val;
    
    err = i2c_master_transmit(bmp280->dev_handle, tx_buffer, 2, -1);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Error: Failed to write CTRL_MEAS register.");
        return err;
    }

    xSemaphoreGive(bmp280->mutex);

    return ESP_OK;
}

/**
 * @brief Triggers a single measurement cycle in FORCED mode.
 *
 * @details Utilizes a Read-Modify-Write pattern to inject the FORCED mode 
 * bit without overwriting previously configured oversampling settings.
 *
 * @return esp_err_t ESP_OK if the wake-up command was dispatched.
 */
esp_err_t bmp280_force_measurement(bmp280_t *bmp280){
    if(!bmp280){
        ESP_LOGE(TAG, "Invalid argument: NULL pointer provided.");
        return ESP_ERR_INVALID_ARG;
    }

    if (bmp280->dev_handle == NULL) {
        ESP_LOGE(TAG, "Invalid state: Device handle is NULL. Ensure bmp280_init() was successful.");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    uint8_t reg_addr = BMP280_REG_CTRL; 
    uint8_t ctrl_meas_val;

    /* Step 1: Read current state to preserve oversampling bits */
    err = i2c_master_transmit_receive(bmp280->dev_handle, &reg_addr, 1, 
                                      &ctrl_meas_val, 1, -1);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Error: Failed to read CTRL_MEAS register.");
        return err;
    }
    

    /* Step 2: Clear the 2 mode bits and inject FORCED mode */
    ctrl_meas_val = (ctrl_meas_val & ~0x03) | BMP280_MODE_FORCED;

    /* Step 3: Write modified byte back to sensor */
    uint8_t tx_buffer[2] = { BMP280_REG_CTRL, ctrl_meas_val };

    err = i2c_master_transmit(bmp280->dev_handle, tx_buffer, 2, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error: Failed to write CTRL_MEAS register.");
        return err;
    }

    xSemaphoreGive(bmp280->mutex);

    return ESP_OK;
}

/**
 * @brief Reads raw temperature and applies the compensation formula.
 *
 * @param temperature Pointer to store the calculated Celsius value.
 * @return esp_err_t ESP_OK upon successful read and computation.
 */
esp_err_t bmp280_read_temperature(bmp280_t *bmp280, float *temperature){
    if(!bmp280 || !temperature){
        return ESP_ERR_INVALID_ARG;
    }

    if(bmp280->dev_handle == NULL){
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err;
    uint8_t reg_addr = BMP280_REG_TEMP_MSB; 
    uint8_t data[3];

    xSemaphoreTake(bmp280->mutex, portMAX_DELAY);

    err = i2c_master_transmit_receive(bmp280->dev_handle, &reg_addr, 1, data, 3, -1);
    if (err != ESP_OK) {
        xSemaphoreGive(bmp280->mutex);
        return err;
    }

    /* Concatenate 3 bytes into a raw 20-bit integer */
    int32_t adc_T = (int32_t)(((uint32_t)data[0] << 12) | 
                              ((uint32_t)data[1] << 4)  | 
                              ((uint32_t)data[2] >> 4));

    int32_t var1_t, var2_t, T; 
    
    /* Official Bosch Sensortec thermal compensation formula (< 80 columns) */
    var1_t = ((((adc_T >> 3) - ((int32_t)bmp280->calibration.dig_T1 << 1))) *
              ((int32_t)bmp280->calibration.dig_T2)) >> 11;
             
    var2_t = (((((adc_T >> 4) - ((int32_t)bmp280->calibration.dig_T1)) *
                ((adc_T >> 4) - ((int32_t)bmp280->calibration.dig_T1))) >> 12) *
              ((int32_t)bmp280->calibration.dig_T3)) >> 14;
    
    /* Update global t_fine, vital for subsequent pressure calculations */
    bmp280->t_fine = var1_t + var2_t;
    T = (bmp280->t_fine * 5 + 128) >> 8;
    
    /* Convert fractionary output to standard float (e.g., 2512 -> 25.12 C) */
    *temperature = (float)T / 100.0f;

    xSemaphoreGive(bmp280->mutex);

    return ESP_OK;
}

/**
 * @brief Performs a burst read of both temperature and pressure.
 *
 * @details Thread-safe polling method. Uses a Mutex to prevent concurrent
 * access to the I2C bus and the global t_fine variable.
 *
 * @param temperature Pointer to store the compensated Celsius value.
 * @param pressure Pointer to store the compensated Pascals value.
 * @return esp_err_t ESP_OK on success, or ESP_ERR_TIMEOUT if bus is busy.
 */
esp_err_t bmp280_read_measurements(bmp280_t *bmp280, float *temperature, float *pressure) {
    if(!bmp280 || !temperature || !pressure){
        return ESP_ERR_INVALID_ARG;
    }

    if (bmp280->dev_handle == NULL){
         return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(bmp280->mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    /* 1. Declare all variables at the beginning of the scope */
    esp_err_t err;
    uint8_t reg_addr = BMP280_REG_PRESS_MSB; 
    uint8_t data[6];
    int32_t adc_P, adc_T;
    int32_t var1_t, var2_t, T;
    int64_t var1_p, var2_p, p;

    /* 3. Perform I2C burst read */
    err = i2c_master_transmit_receive(bmp280->dev_handle, &reg_addr, 1, data, 6, -1);
    if (err != ESP_OK) {
        xSemaphoreGive(bmp280->mutex);
        return err;
    }

    /* 4. Extract raw 20-bit data */
    adc_P = (int32_t)(((uint32_t)data[0] << 12) | 
                      ((uint32_t)data[1] << 4)  | 
                      ((uint32_t)data[2] >> 4));
                              
    adc_T = (int32_t)(((uint32_t)data[3] << 12) | 
                      ((uint32_t)data[4] << 4)  | 
                      ((uint32_t)data[5] >> 4));

    /* --- Temperature Compensation (< 80 columns) --- */
    var1_t = ((((adc_T >> 3) - ((int32_t)bmp280->calibration.dig_T1 << 1))) *
              ((int32_t)bmp280->calibration.dig_T2)) >> 11;
             
    var2_t = (((((adc_T >> 4) - ((int32_t)bmp280->calibration.dig_T1)) *
                ((adc_T >> 4) - ((int32_t)bmp280->calibration.dig_T1))) >> 12) *
              ((int32_t)bmp280->calibration.dig_T3)) >> 14;
    
    bmp280->t_fine = var1_t + var2_t; 
    T = (bmp280->t_fine * 5 + 128) >> 8;
    *temperature = (float)T / 100.0f; 

    /* --- Pressure Compensation (64-bit) (< 80 columns) --- */
    var1_p = ((int64_t) bmp280->t_fine - 128000);
    var2_p = var1_p * var1_p * (int64_t)bmp280->calibration.dig_P6;
    var2_p = var2_p + ((var1_p * (int64_t)bmp280->calibration.dig_P5) << 17);
    var2_p = var2_p + (((int64_t)bmp280->calibration.dig_P4) << 35);
    var1_p = ((var1_p * var1_p * (int64_t)bmp280->calibration.dig_P3) >> 8) + 
             ((var1_p * (int64_t)bmp280->calibration.dig_P2) << 12);
    
    var1_p = (((((int64_t)1) << 47) + var1_p)) * ((int64_t)bmp280->calibration.dig_P1) >> 33;

    if (var1_p == 0) {
        xSemaphoreGive(bmp280->mutex);
        return ESP_FAIL; 
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2_p) * 3125) / var1_p;
    var1_p = (((int64_t)bmp280->calibration.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2_p = (((int64_t)bmp280->calibration.dig_P8) * p) >> 19;
    p = ((p + var1_p + var2_p) >> 8) + (((int64_t)bmp280->calibration.dig_P7) << 4);
    *pressure = (float)p / 256.0f;

    xSemaphoreGive(bmp280->mutex);

    return ESP_OK;
}

/**
 * @brief Safely releases I2C bus resources and device handles.
 *
 * @details Must be called to prevent memory leaks if the component is 
 * dynamically reinitialized or if the host shuts down.
 *
 * @return esp_err_t ESP_OK upon safe destruction of handles.
 */
esp_err_t bmp280_deinit(bmp280_t *bmp280) {
    if(!bmp280){
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err;

    xSemaphoreTake(bmp280->mutex, portMAX_DELAY);

    /* 1. Remove logical device from the bus */
    if (bmp280->dev_handle != NULL) {
        err = i2c_master_bus_rm_device(bmp280->dev_handle);
        if (err != ESP_OK) {
            xSemaphoreGive(bmp280->mutex);
            return err;
        }
        bmp280->dev_handle = NULL; /* Prevent dangling pointers */
    }

    /* 2. Physically destroy the main I2C bus, freeing FreeRTOS memory */
    if (bmp280->bus_handle != NULL) {
        err = i2c_del_master_bus(bmp280->bus_handle);
        if (err != ESP_OK){
            xSemaphoreGive(bmp280->mutex);
            return err;
        }
        bmp280->bus_handle = NULL; 
    }

    xSemaphoreGive(bmp280->mutex);
    vSemaphoreDelete(bmp280->mutex);
    bmp280->mutex = NULL;

    return ESP_OK;
}