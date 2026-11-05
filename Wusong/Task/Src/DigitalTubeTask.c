#include "DigitalTubeTask.h"

#define DigitalTube_Refresh_Time 25

static uint8_t DigitalBuffer[4] = {0x82};
DigitalTube_t DigitalTube;

/*
 * 三位数码管兼容标记：
 * 1 = 开启左移一位显示，例如安卓下发123时按1230写入；
 * 0 = 保持原四位显示逻辑。
 */
uint8_t DigitalTubeShiftEnable = 1U;

void DigitalTubeTask_Init(void)
{
    DigitalTube_Init_t Init;

    Init.hspi = &hspi2;
    Init.Buffer = DigitalBuffer;
    Init.bit_num = sizeof(DigitalBuffer);
    Init.LE_GPIO = Tube_RCLK_GPIO_Port;
    Init.LE_Pin = Tube_RCLK_Pin;
    Init.CODE_CA = DIGITAL3BIT_CODE_CA;

    DigitalTube_Init(&DigitalTube, Init);
    DigitalTube_SetDisplayNumber(0U);

    /* 中文注释：首帧数据完成锁存后再拉低SEG_OE，使能数码管输出。 */
    HAL_GPIO_WritePin(Tube_OE_GPIO_Port, Tube_OE_Pin, GPIO_PIN_RESET);
}

/*
 * @brief 设置数码管显示值
 * @note 三位兼容开启时，将0~999左移一位写入四位缓存。
 *       例如123按1230写入，使实际不存在的个位落在最后一位，
 *       三个可见数码管显示123。0需要单独写到原十位位置。
 *       大于999的值保持原四位处理，避免改变旧协议的超范围行为。
 */
void DigitalTube_SetDisplayNumber(uint32_t data)
{
    uint32_t display_data = data;

    if (DigitalTubeShiftEnable != 0U && data <= 999U)
    {
        if (data == 0U)
        {
            /*
             * 0乘10仍为0，原Set_Num会把0放在个位。
             * 三位硬件没有个位，因此直接按3位数据写入，
             * 让0显示在原十位对应的最右可见位置。
             */
            DigitalTube.Set_Num(&DigitalTube, 0, 0U, 3);
            DigitalTube.Refresh(&DigitalTube);
            return;
        }

        display_data = data * 10U;
    }

    DigitalTube.Set_Num(&DigitalTube, 0, display_data, 4);
    DigitalTube.Refresh(&DigitalTube);
}

void DigitalTube_Task(void)
{
    static uint32_t time = 0;
    if (HAL_GetTick() - time > DigitalTube_Refresh_Time)
    {
        DigitalTube.Refresh(&DigitalTube);
        time = HAL_GetTick();
    }
}
