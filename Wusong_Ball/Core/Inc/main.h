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

/* 球盘原理图 User_Led 实际接 PA15，作为普通 GPIO 状态灯使用。 */
#define User_Led_Pin GPIO_PIN_15
#define User_Led_GPIO_Port GPIOA

/* 球盘 5 颗 WS2812 接 PA6，使用 TIM3_CH1 PWM 和 DMA1_Channel6 输出。 */
#define BallLight_Pin GPIO_PIN_6
#define BallLight_GPIO_Port GPIOA

/* 球盘本板 5 组光眼为 PB3~PB7。 */
#define BallEye1_Pin GPIO_PIN_3
#define BallEye1_GPIO_Port GPIOB
#define BallEye2_Pin GPIO_PIN_4
#define BallEye2_GPIO_Port GPIOB
#define BallEye3_Pin GPIO_PIN_5
#define BallEye3_GPIO_Port GPIOB
#define BallEye4_Pin GPIO_PIN_6
#define BallEye4_GPIO_Port GPIOB
#define BallEye5_Pin GPIO_PIN_7
#define BallEye5_GPIO_Port GPIOB

/* 两块外接单数码管模块的光眼反馈分别进入 PA4/PA5。 */
#define BallEyeFB1_Pin GPIO_PIN_4
#define BallEyeFB1_GPIO_Port GPIOA
#define BallEyeFB2_Pin GPIO_PIN_5
#define BallEyeFB2_GPIO_Port GPIOA

/* 两块级联 SM16306 数码管模块使用 PB12~PB15 软件移位。 */
#define Tube_OE_Pin GPIO_PIN_12
#define Tube_OE_GPIO_Port GPIOB
/* 球盘新硬件接线：PB13 为 SRCLK 移位时钟，PB14 为 RCLK 锁存。 */
#define Tube_RCLK_Pin GPIO_PIN_14
#define Tube_RCLK_GPIO_Port GPIOB
#define Tube_SRCLK_Pin GPIO_PIN_13
#define Tube_SRCLK_GPIO_Port GPIOB
#define Tube_SER_Pin GPIO_PIN_15
#define Tube_SER_GPIO_Port GPIOB

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
