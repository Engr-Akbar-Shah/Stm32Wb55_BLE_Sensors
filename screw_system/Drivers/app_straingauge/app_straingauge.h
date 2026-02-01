/*
 * app_straingauge.h
 *
 *  Created on: Jan 16, 2026
 *      Author: ghost
 */

#ifndef DRIVERS_APP_STRAINGAUGE_APP_STRAINGAUGE_H_
#define DRIVERS_APP_STRAINGAUGE_APP_STRAINGAUGE_H_

#include "main.h"
#include "adc.h"

HAL_StatusTypeDef APP_StrainGauge_ReadRaw(uint16_t *raw_value);\
HAL_StatusTypeDef APP_StrainGauge_ReadAndPack(uint8_t *buffer, uint8_t offset);

#endif /* DRIVERS_APP_STRAINGAUGE_APP_STRAINGAUGE_H_ */
