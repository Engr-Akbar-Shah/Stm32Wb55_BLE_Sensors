/*
 * app_lsm6dsltr.c
 *
 *  Created on: Jan 16, 2026
 *      Author: ghost
 */

#include "app_lsm6dstlr.h"


#include <stdbool.h>
#include <stdint.h>

#define LSM6DSL_I2C_ADDR       (0x6A << 1)   // 0xD4 when shifted
#define LSM6DSL_WHO_AM_I       0x0F
#define LSM6DSL_WHO_AM_I_VALUE 0x6A

#define LSM6DSL_REG_CTRL1_XL   0x10
#define LSM6DSL_REG_CTRL2_G    0x11
#define LSM6DSL_REG_CTRL3_C    0x12
#define LSM6DSL_REG_OUT_TEMP_L 0x20
#define LSM6DSL_REG_OUTX_L_G   0x22

static HAL_StatusTypeDef lsm6dsl_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return HAL_I2C_Master_Transmit(&hi2c1, LSM6DSL_I2C_ADDR, buf, 2, 100);
}

static HAL_StatusTypeDef lsm6dsl_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, LSM6DSL_I2C_ADDR, reg, 1, data, len, 100);
}

HAL_StatusTypeDef LSM6DSL_Init(void)
{
    uint8_t who_am_i = 0;
    HAL_StatusTypeDef status;

    // Basic device presence check
    status = lsm6dsl_read_reg(LSM6DSL_WHO_AM_I, &who_am_i, 1);
    if (status != HAL_OK || who_am_i != LSM6DSL_WHO_AM_I_VALUE) {
        return HAL_ERROR;
    }

    // Accelerometer: 104 Hz, ±4g, 400 Hz BW
    status = lsm6dsl_write_reg(LSM6DSL_REG_CTRL1_XL, 0x48);   // ODR=104 Hz, FS=±4g
    if (status != HAL_OK) return status;

    // Gyroscope: 104 Hz, ±500 dps
    status = lsm6dsl_write_reg(LSM6DSL_REG_CTRL2_G, 0x50);    // ODR=104 Hz, FS=±500 dps
    if (status != HAL_OK) return status;

    // Block Data Update + auto-increment
    status = lsm6dsl_write_reg(LSM6DSL_REG_CTRL3_C, 0x44);    // BDU=1, IF_INC=1

    HAL_Delay(20);  // Give time for configuration to settle

    return status;
}

HAL_StatusTypeDef LSM6DSL_ReadData(LSM6DSL_Data_t *data)
{
    uint8_t buf[12];
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR;
    }

    status = lsm6dsl_read_reg(LSM6DSL_REG_OUTX_L_G, buf, 12);
    if (status != HAL_OK) {
        return status;
    }

    // Gyroscope (first 6 bytes)
    data->app_gyro_x  = (int16_t)((buf[1] << 8) | buf[0]);
    data->app_gyro_y  = (int16_t)((buf[3] << 8) | buf[2]);
    data->app_gyro_z  = (int16_t)((buf[5] << 8) | buf[4]);

    // Accelerometer (next 6 bytes)
    data->app_accel_x = (int16_t)((buf[7]  << 8) | buf[6]);
    data->app_accel_y = (int16_t)((buf[9]  << 8) | buf[8]);
    data->app_accel_z = (int16_t)((buf[11] << 8) | buf[10]);

    return HAL_OK;
}
