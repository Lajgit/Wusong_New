/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

void Error_Handler(void);

/* 微动板User_Led接PA1，低电平点亮。 */
#define User_Led_Pin GPIO_PIN_1
#define User_Led_GPIO_Port GPIOA

/* J1~J12：PA3~PA8、PA11、PA12、PA15、PB3~PB5；低电平闭合。 */
#define Channel_SDI_Pin GPIO_PIN_15
#define Channel_CLK_Pin GPIO_PIN_14
#define Channel_LE_Pin GPIO_PIN_13
#define Valve_Left_Pin GPIO_PIN_10
#define Valve_Right_Pin GPIO_PIN_11
#define Valve_Block_Pin GPIO_PIN_0
#define Hole_Left_Pin GPIO_PIN_1
#define Hole_Right_Pin GPIO_PIN_2

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
