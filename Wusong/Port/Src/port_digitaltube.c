#include "port_digitaltube.h"

/* 共阳极数码管段码表（0~9）*/
const uint8_t SEGMENT_CODE_CA[] = {
    0xC0, // 0 (共阳极取反：原0x3F -> 0xC0)
    0xF9, // 1
    0xA4, // 2
    0xB0, // 3
    0x99, // 4
    0x92, // 5
    0x82, // 6
    0xF8, // 7
    0x80, // 8
    0x90, // 9
    0xFF, // 10 (空白)
};

const uint8_t DIGITAL3BIT_CODE_CA[] = {
    0x82, // 0
    0xAF, // 1
    0x91, // 2
    0x85, // 3
    0xAC, // 4
    0xC4, // 5
    0xC0, // 6
    0x8F, // 7
    0x80, // 8
    0x84, // 9
    0xFF, // 10 (空白)
};

const uint8_t TOWER_CODE_CA[] = {
    0x82, // 0
    0xAF, // 1
    0x91, // 2
    0x85, // 3
    0xAC, // 4
    0xC4, // 5
    0xC0, // 6
    0x8F, // 7
    0x80, // 8
    0x84, // 9
    0xFF, // 10 (空白)
};

/*
 * @brief  设置数码管的某一位
 * @param  data: 数码管的某一位数据
 * @param  position: 数码管的某一位位置，第0位开始
 * @param  buffer: 数码管的数据缓存
 * @param  size: 数码管的数据缓存大小
 * @retval 位置非法返回false，正确返回true
 */
static bool DigitalTube_Setbit(uint8_t position, uint8_t data, uint8_t *buffer, uint8_t buffersize, const uint8_t *code)
{
    if (data > 10)
        return false;
    if (position > buffersize - 1)
        return false;

    buffer[buffersize - position - 1] = code[data];
    return true;
}

/*
 * @brief  设置数码管的某位数字
 * @param  positon: 数码管的某位位置，第0位开始
 * @param  data: 数码管的某位数字
 * @param  datasize: 数码管的数字位数
 * @param  buffer: 数码管的数据缓存
 * @param  buffersize: 数码管的数据缓存大小
 * @param  code: 码表
 */
static bool DigitalTube_SetNum(uint8_t positon, uint32_t data, uint8_t datasize, uint8_t *buffer, uint8_t buffersize, const uint8_t *code)
{
    if (positon + datasize > buffersize)
        return false;

    for (uint8_t i = 1; i <= datasize; i++)
    {
        uint8_t num = data % 10;
        if (data == 0)
        {
            if (i == 1)
                DigitalTube_Setbit(positon + datasize - i, 0, buffer, buffersize, code);
            else
                DigitalTube_Setbit(positon + datasize - i, 10, buffer, buffersize, code);
        }
        else
        {
            DigitalTube_Setbit(positon + datasize - i, num, buffer, buffersize, code);
        }
        data /= 10;
    }

    return true;
}

/*
 *===============================数码管对象方法实现=============================================
 */

static void Set_Num(void *self, uint8_t position, uint32_t data, uint8_t datasize)
{
    DigitalTube_t *DigitalTube = (DigitalTube_t *)self;
    DigitalTube_SetNum(position, data, datasize, DigitalTube->Buffer, DigitalTube->bit_num, DigitalTube->CODE_CA);
}

/*
 * @brief 刷新数码管缓存到外部移位寄存器
 * @note RCLK1经过74HC14反相：MCU侧保持高电平时，SM16306的LE为低并保持旧数据；
 *       SPI发送完成后给RCLK1一个低脉冲，对应SM16306的LE高脉冲完成锁存。
 */
static void Refresh(void *self)
{
    DigitalTube_t *DigitalTube = (DigitalTube_t *)self;

    if (DigitalTube->hspi == NULL || DigitalTube->Buffer == NULL || DigitalTube->bit_num == 0)
        return;

    if (DigitalTube->LE_GPIO != NULL && DigitalTube->LE_Pin != 0)
        HAL_GPIO_WritePin(DigitalTube->LE_GPIO, DigitalTube->LE_Pin, GPIO_PIN_SET);

    HAL_SPI_Transmit(DigitalTube->hspi, DigitalTube->Buffer, DigitalTube->bit_num, 100);

    if (DigitalTube->LE_GPIO != NULL && DigitalTube->LE_Pin != 0)
    {
        /* 中文注释：RCLK1低脉冲经74HC14反相后形成SM16306的LE高脉冲，锁存32位显示数据。 */
        HAL_GPIO_WritePin(DigitalTube->LE_GPIO, DigitalTube->LE_Pin, GPIO_PIN_RESET);
        __NOP();
        HAL_GPIO_WritePin(DigitalTube->LE_GPIO, DigitalTube->LE_Pin, GPIO_PIN_SET);
    }
}

void DigitalTube_Init(DigitalTube_t *DigitalTube, DigitalTube_Init_t Init)
{
    DigitalTube->hspi = Init.hspi;
    DigitalTube->Buffer = Init.Buffer;
    DigitalTube->bit_num = Init.bit_num;
    DigitalTube->CODE_CA = Init.CODE_CA;
    DigitalTube->LE_GPIO = Init.LE_GPIO;
    DigitalTube->LE_Pin = Init.LE_Pin;

    DigitalTube->Set_Num = Set_Num;
    DigitalTube->Refresh = Refresh;
    memset(DigitalTube->Buffer, DigitalTube->CODE_CA[0], DigitalTube->bit_num);
}
