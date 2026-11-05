/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    spi.c
  * @brief   This file provides code for the configuration
  *          of the SPI instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "spi.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle)
{
  (void)spiHandle;
  /* 中文注释：球盘数码管使用GPIO软件移位，硬件SPI2不再初始化。 */
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef* spiHandle)
{
  (void)spiHandle;
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
