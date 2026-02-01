/*
 * app_lsm6dstlr.h
 *
 *  Created on: Jan 16, 2026
 *      Author: ghost
 */

#ifndef DRIVERS_APP_LSM6DSLTR_APP_LSM6DSTLR_H_
#define DRIVERS_APP_LSM6DSLTR_APP_LSM6DSTLR_H_


#include "main.h"
#include "i2c.h"
#include <stdint.h>

typedef struct {
    int16_t app_gyro_x;
    int16_t app_gyro_y;
    int16_t app_gyro_z;
    int16_t app_accel_x;
    int16_t app_accel_y;
    int16_t app_accel_z;
} LSM6DSL_Data_t;

HAL_StatusTypeDef LSM6DSL_Init(void);
HAL_StatusTypeDef LSM6DSL_ReadData(LSM6DSL_Data_t *data);


#endif /* DRIVERS_APP_LSM6DSLTR_APP_LSM6DSTLR_H_ */
