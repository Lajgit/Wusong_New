/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

#if defined(DBGMCU_CR_TRACE_IOEN)
  /* 中文注释：下载调试只使用SWD模式，关闭Trace/SWO输出，避免PB3调试复用影响扩展GPIO。 */
  DBGMCU->CR &= ~DBGMCU_CR_TRACE_IOEN;
#endif

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /* 中文注释：两路12V灯带为低边MOS驱动，预置输出高电平保持默认开启。 */
  HAL_GPIO_WritePin(GPIOB, LedOutput1_Pin | LedOutput2_Pin, GPIO_PIN_SET);

  /* 中文注释：两组卡片机控制输出上电默认关闭。 */
  HAL_GPIO_WritePin(CardOutput_GPIO_Port, CardOutput_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Card2Output_GPIO_Port, Card2Output_Pin, GPIO_PIN_RESET);

  /* 中文注释：新扭蛋机原理图中电子锁控制为 PA0，默认输出低电平保持关闭。 */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);

  /* 中文注释：RCLK1经过74HC14反相，MCU侧默认拉高可使SM16306的LE保持低电平锁存；SEG_OE先拉高关闭输出。 */
  HAL_GPIO_WritePin(Tube_RCLK_GPIO_Port, Tube_RCLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(Tube_OE_GPIO_Port, Tube_OE_Pin, GPIO_PIN_SET);

  /* 进珠光眼由SysTick软件滤波扫描，不再配置无效的EXTI2。 */
  GPIO_InitStruct.Pin = HoolleInput_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(HoolleInput_GPIO_Port, &GPIO_InitStruct);

  /* 中文注释：两组卡片机反馈和投币保持原有上升沿触发方式。 */
  GPIO_InitStruct.Pin = CardFeedback_Pin | CoinInput_Pin | Card2Feedback_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* 中文注释：原两路出货光眼以及新增PE7/PE8独立光眼均检测高低电平变化。 */
  GPIO_InitStruct.Pin = HoolleOutput_1_Pin | HoolleOutput_2_Pin | ExtraEye1_Pin | ExtraEye2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* 系统指示灯 */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* 中文注释：两组卡片机控制输出分别使用PC4、PC5。 */
  GPIO_InitStruct.Pin = CardOutput_Pin | Card2Output_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* 中文注释：两路12V LED输出，硬件每路并接3个灯带接口，GPIO输出高电平时MOS导通。 */
  GPIO_InitStruct.Pin = LedOutput1_Pin | LedOutput2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 中文注释：APM32/STM32初始化为GPIO输出后再次置高，确保PB3/PB4最终默认开启。 */
  HAL_GPIO_WritePin(GPIOB, LedOutput1_Pin | LedOutput2_Pin, GPIO_PIN_SET);

  /* 中文注释：电子锁MOS控制脚，原弹界TIM5灯效占用已取消。 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* 中文注释：PB12为数码管锁存时钟RCLK，PB14为段输出使能SEG_OE。 */
  GPIO_InitStruct.Pin = Tube_RCLK_Pin | Tube_OE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 中文注释：后台设置按键K1/K2使用PB6/PB7，均由板上电阻上拉，按下为低。 */
  GPIO_InitStruct.Pin = KeyBoard3_Pin | KeyBoard2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 中文注释：后台设置按键K3使用PD0；游玩按键1~4使用PD8/PD9/PD10/PD11，均由板上电阻上拉。 */
  GPIO_InitStruct.Pin = KeyBoard1_Pin | PlayButton1_Pin | PlayButton2_Pin | PlayButton3_Pin | PlayButton4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* 中文注释：PE10作为第二卡片机反馈，使用EXTI10并挂在EXTI15_10中断组。 */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
