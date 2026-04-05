#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include "main.h"

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c3;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim5;
extern TIM_HandleTypeDef htim7;
extern UART_HandleTypeDef huart3;

void MX_StationPeripherals_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

#endif