/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  */
/* USER CODE END Header */
#include "gpio.h"

void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  /* 中文注释：PB3/PB4作为球盘光眼输入，PA15作为状态灯GPIO，关闭JTAG并保留SWD。 */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();

  /* 中文注释：User_Led由3.3V经电阻接到PA15，低电平点亮。 */
  // HAL_GPIO_WritePin(User_Led_GPIO_Port, User_Led_Pin, GPIO_PIN_RESET);
  /* 球盘 WS2812 的 PA6 输出锁存值预置低电平，复用模式由 TIM3 初始化配置。 */
  HAL_GPIO_WritePin(BallLight_GPIO_Port, BallLight_Pin, GPIO_PIN_RESET);
  /* 中文注释：SEG_OE经两级反相后与OE#同极性，PB12低电平使能显示，高电平关闭显示。 */
  HAL_GPIO_WritePin(Tube_OE_GPIO_Port, Tube_OE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(Tube_RCLK_GPIO_Port, Tube_RCLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(Tube_SRCLK_GPIO_Port, Tube_SRCLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(Tube_SER_GPIO_Port, Tube_SER_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = User_Led_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  // HAL_GPIO_Init(User_Led_GPIO_Port, &GPIO_InitStruct);

  /* 中文注释：PB3~PB7和PA4/PA5均由原理图外围电阻提供电平，MCU不额外上下拉。 */
  GPIO_InitStruct.Pin = BallEye1_Pin | BallEye2_Pin | BallEye3_Pin | BallEye4_Pin | BallEye5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BallEyeFB1_Pin | BallEyeFB2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = Tube_OE_Pin | Tube_RCLK_Pin | Tube_SRCLK_Pin | Tube_SER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
