/*
 * app_straingauge.c
 *
 *  Created on: Jan 16, 2026
 *      Author: ghost
 */


#include "app_straingauge.h"


HAL_StatusTypeDef APP_StrainGauge_ReadRaw(uint16_t *raw_value)
{
    HAL_StatusTypeDef status;

    if (raw_value == NULL) {
        return HAL_ERROR;
    }

    status = HAL_ADC_Start(&hadc1);
    if (status != HAL_OK) {
        return status;
    }

    status = HAL_ADC_PollForConversion(&hadc1, 100);
    if (status != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return status;
    }

    *raw_value = (uint16_t)HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return HAL_OK;
}

HAL_StatusTypeDef APP_StrainGauge_ReadAndPack(uint8_t *buffer, uint8_t offset)
{
    uint16_t value;
    HAL_StatusTypeDef status = APP_StrainGauge_ReadRaw(&value);

    if (buffer != NULL) {
        buffer[offset]     = (uint8_t)(value & 0x00FF);
        buffer[offset + 1] = (uint8_t)(value >> 8);
    }

    return status;
}


