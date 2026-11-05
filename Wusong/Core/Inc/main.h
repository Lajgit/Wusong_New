/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with the SOFTWARE, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define HoolleInput_Pin GPIO_PIN_2
#define HoolleInput_GPIO_Port GPIOE
#define HoolleInput_EXTI_IRQn EXTI2_IRQn
#define HoolleOutput_1_Pin GPIO_PIN_3
#define HoolleOutput_1_GPIO_Port GPIOE
#define HoolleOutput_1_EXTI_IRQn EXTI3_IRQn
#define HoolleOutput_2_Pin GPIO_PIN_4
#define HoolleOutput_2_GPIO_Port GPIOE
#define HoolleOutput_2_EXTI_IRQn EXTI4_IRQn
#define CardFeedback_Pin GPIO_PIN_5
#define CardFeedback_GPIO_Port GPIOE
#define CardFeedback_EXTI_IRQn EXTI9_5_IRQn
#define CoinInput_Pin GPIO_PIN_6
#define CoinInput_GPIO_Port GPIOE
#define CoinInput_EXTI_IRQn EXTI9_5_IRQn

/* 中文注释：新增两路独立光眼使用PE7/PE8，预留双边沿状态检测。 */
#define ExtraEye1_Pin GPIO_PIN_7
#define ExtraEye1_GPIO_Port GPIOE
#define ExtraEye1_EXTI_IRQn EXTI9_5_IRQn
#define ExtraEye2_Pin GPIO_PIN_8
#define ExtraEye2_GPIO_Port GPIOE
#define ExtraEye2_EXTI_IRQn EXTI9_5_IRQn

/* 中文注释：第二组卡片机反馈使用PE10，控制输出使用PC5。 */
#define Card2Feedback_Pin GPIO_PIN_10
#define Card2Feedback_GPIO_Port GPIOE
#define Card2Feedback_EXTI_IRQn EXTI15_10_IRQn

#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define SettingButton_Pin GPIO_PIN_0
#define SettingButton_GPIO_Port GPIOC
#define SettingButton_EXTI_IRQn EXTI0_IRQn
#define Button1_Pin GPIO_PIN_1
#define Button1_GPIO_Port GPIOC
#define KeyLED_1_Pin GPIO_PIN_2
#define KeyLED_1_GPIO_Port GPIOC
#define KeyLED_2_Pin GPIO_PIN_3
#define KeyLED_2_GPIO_Port GPIOC

/* 中文注释：两路12V灯带输出，硬件每路对应3个LED接口，MOS高电平导通。 */
#define LedOutput1_Pin GPIO_PIN_3
#define LedOutput1_GPIO_Port GPIOB
#define LedOutput2_Pin GPIO_PIN_4
#define LedOutput2_GPIO_Port GPIOB

/* 中文注释：按新原理图命名三路舵机：舵机1=PA1/CH2、舵机2=PA2/CH3、舵机3=PA3/CH4。 */
#define Servo_1_Pin GPIO_PIN_1
#define Servo_1_GPIO_Port GPIOA
#define Servo_2_Pin GPIO_PIN_2
#define Servo_2_GPIO_Port GPIOA
#define Servo_3_Pin GPIO_PIN_3
#define Servo_3_GPIO_Port GPIOA

#define SD_CS_Pin GPIO_PIN_4
#define SD_CS_GPIO_Port GPIOA
#define SD_CLK_Pin GPIO_PIN_5
#define SD_CLK_GPIO_Port GPIOA
#define SD_MISO_Pin GPIO_PIN_6
#define SD_MISO_GPIO_Port GPIOA
#define SD_MOSI_Pin GPIO_PIN_7
#define SD_MOSI_GPIO_Port GPIOA
#define CardOutput_Pin GPIO_PIN_4
#define CardOutput_GPIO_Port GPIOC
#define Card2Output_Pin GPIO_PIN_5
#define Card2Output_GPIO_Port GPIOC

/* 中文注释：J6数码管接口，PB13/PB15复用SPI2，PB12/PB14作为普通GPIO。 */
#define Tube_RCLK_Pin GPIO_PIN_12
#define Tube_RCLK_GPIO_Port GPIOB
#define Tube_SRCLK_Pin GPIO_PIN_13
#define Tube_SRCLK_GPIO_Port GPIOB
#define Tube_OE_Pin GPIO_PIN_14
#define Tube_OE_GPIO_Port GPIOB
#define Tube_SER_Pin GPIO_PIN_15
#define Tube_SER_GPIO_Port GPIOB

/* 中文注释：四路外接游玩/拍拍按键，协议编号依次为1、2、3、4；按键4正式使用PD11。 */
#define PlayButton1_Pin GPIO_PIN_8
#define PlayButton1_GPIO_Port GPIOD
#define PlayButton2_Pin GPIO_PIN_9
#define PlayButton2_GPIO_Port GPIOD
#define PlayButton3_Pin GPIO_PIN_10
#define PlayButton3_GPIO_Port GPIOD
#define PlayButton4_Pin GPIO_PIN_11
#define PlayButton4_GPIO_Port GPIOD

/* 中文注释：PD5/PD6按新原理图保留给串口2，通信参数后续确定后再初始化USART2。 */
#define Serial2_TX_Pin GPIO_PIN_5
#define Serial2_TX_GPIO_Port GPIOD
#define Serial2_RX_Pin GPIO_PIN_6
#define Serial2_RX_GPIO_Port GPIOD

/* 中文注释：旧Hole_B1宏仍指向PD11，仅为兼容旧代码；PD11当前实际功能为拍拍按键4，不得同时启用旧孔洞逻辑。 */
#define Hole_B1_Pin GPIO_PIN_11
#define Hole_B1_GPIO_Port GPIOD
#define Hole_B2_Pin GPIO_PIN_12
#define Hole_B2_GPIO_Port GPIOD
#define Hole_Y1_Pin GPIO_PIN_13
#define Hole_Y1_GPIO_Port GPIOD
#define Hole_P1_Pin GPIO_PIN_14
#define Hole_P1_GPIO_Port GPIOD
#define Hole_P2_Pin GPIO_PIN_15
#define Hole_P2_GPIO_Port GPIOD
#define Hole_Y2_Pin GPIO_PIN_6
#define Hole_Y2_GPIO_Port GPIOC
#define Hole_B3_Pin GPIO_PIN_7
#define Hole_B3_GPIO_Port GPIOC
#define Hole_Y3_Pin GPIO_PIN_8
#define Hole_Y3_GPIO_Port GPIOC
#define Hole_B5_Pin GPIO_PIN_9
#define Hole_B5_GPIO_Port GPIOC
#define Hole_P3_Pin GPIO_PIN_8
#define Hole_P3_GPIO_Port GPIOA
#define Hole_Y4_Pin GPIO_PIN_11
#define Hole_Y4_GPIO_Port GPIOA
#define Hole_B7_Pin GPIO_PIN_12
#define Hole_B7_GPIO_Port GPIOA

/* 中文注释：后台设置按键。原理图K1=PB6、K2=PB7、K3=PD0；旧宏名暂保留以缩小修改范围。 */
#define KeyBoard1_Pin GPIO_PIN_0
#define KeyBoard1_GPIO_Port GPIOD
#define Q7_Pin GPIO_PIN_3
#define Q7_GPIO_Port GPIOD
#define Q8_Pin GPIO_PIN_4
#define Q8_GPIO_Port GPIOD
#define Q11_Pin GPIO_PIN_7
#define Q11_GPIO_Port GPIOD
#define KeyBoard3_Pin GPIO_PIN_6
#define KeyBoard3_GPIO_Port GPIOB
#define KeyBoard2_Pin GPIO_PIN_7
#define KeyBoard2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
