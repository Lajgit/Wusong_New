#include "LightTask.h"

static uint16_t ChannelMask = 0U;

uint16_t BallLight_GetChannels(void)
{
    return ChannelMask;
}

/* SM16306：OE#硬接地，PB15=SDI、PB14=CLK、PB13=LE，无反相器。 */
void BallLight_SetChannels(uint16_t mask)
{
    uint16_t output = 0U;
    ChannelMask = mask & 0x0FFFU;
    /* 通道bit0(J1)对应LED12/OUT11；bit11(J12)对应LED1/OUT0。 */
    for (uint8_t i = 0U; i < 12U; i++)
        if ((mask & (1U << i)) != 0U)
            output |= (uint16_t)(1U << (11U - i));

    HAL_GPIO_WritePin(GPIOB, Channel_LE_Pin, GPIO_PIN_RESET);
    /* 先发送高位，完整移入16位后再脉冲锁存，未使用的高四路始终为0。 */
    for (uint16_t bit = 0x8000U; bit != 0U; bit >>= 1U)
    {
        HAL_GPIO_WritePin(GPIOB, Channel_CLK_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, Channel_SDI_Pin, (output & bit) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, Channel_CLK_Pin, GPIO_PIN_SET);
    }
    HAL_GPIO_WritePin(GPIOB, Channel_CLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, Channel_LE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, Channel_LE_Pin, GPIO_PIN_RESET);
}

void Light_Init(void)
{
    BallLight_SetChannels(0U);
}

void Light_Task(void)
{
    /* 静态通道灯仅在收到新掩码时移位，不需要RGB刷新任务。 */
}
