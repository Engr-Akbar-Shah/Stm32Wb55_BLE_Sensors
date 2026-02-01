/*
 * app_stts22htr.c
 *
 *  Created on: Jan 16, 2026
 *      Author: ghost
 */


#include "app_stts22htr.h"


#include <stdbool.h>
#include <stdint.h>

#define STTS22H_I2C_ADDR       (0x3F << 1)   // (ADDR to GND)
//#define STTS22H_I2C_ADDR       (0x3C << 1)   //  (ADDR to VCC)
//#define STTS22H_I2C_ADDR       (0x3F << 1)   //  (ADDR to GND)

#define STTS22H_WHO_AM_I       0x01
#define STTS22H_WHO_AM_I_VALUE 0xA0

#define STTS22H_REG_CTRL       0x04
#define STTS22H_REG_TEMP_L     0x06


static HAL_StatusTypeDef stts22h_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return HAL_I2C_Master_Transmit(&hi2c1, STTS22H_I2C_ADDR, buf, 2, 100);
}

static HAL_StatusTypeDef stts22h_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, STTS22H_I2C_ADDR, reg, 1, data, len, 100);
}

HAL_StatusTypeDef STTS22H_Init(void)
{
    uint8_t who_am_i = 0;
    HAL_StatusTypeDef status;

    // Device presence check
    status = stts22h_read_reg(STTS22H_WHO_AM_I, &who_am_i, 1);
    if (status != HAL_OK || who_am_i != STTS22H_WHO_AM_I_VALUE) {
        return HAL_ERROR;
    }

    // Power down (redundant with default, but from your code)
    status = stts22h_write_reg(STTS22H_REG_CTRL, 0x00);
    if (status != HAL_OK) return status;

    // Freerun mode: 50 Hz ODR (AVG=01), IF_ADD_INC=1, FREERUN=1
    status = stts22h_write_reg(STTS22H_REG_CTRL, 0x1C);
    if (status != HAL_OK) return status;

    HAL_Delay(20);  // Short settle time

    return HAL_OK;
}

HAL_StatusTypeDef STTS22H_ReadTemp(float *temp_c)
{
    uint8_t buf[2];
    HAL_StatusTypeDef status;
    int16_t val;

    if (temp_c == NULL) {
        return HAL_ERROR;
    }

    status = stts22h_read_reg(STTS22H_REG_TEMP_L, buf, 2);
    if (status != HAL_OK) {
        return status;
    }

    val = ((int16_t)buf[1] << 8) | buf[0];
    *temp_c = (float)val * 0.01f;

    return HAL_OK;
}
