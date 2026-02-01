/*
 * app_stts22htr.h
 *
 *  Created on: Jan 16, 2026
 *      Author: ghost
 */

#ifndef DRIVERS_APP_STTS22HTR_APP_STTS22HTR_H_
#define DRIVERS_APP_STTS22HTR_APP_STTS22HTR_H_

#include "main.h"
#include "i2c.h"

HAL_StatusTypeDef STTS22H_Init(void);
HAL_StatusTypeDef STTS22H_ReadTemp(float *temp_c);

#endif /* DRIVERS_APP_STTS22HTR_APP_STTS22HTR_H_ */
