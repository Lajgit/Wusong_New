#include "DigitalTubeTask.h"
#include "softspi.h"
#include "string.h"

#define DigitalTube_Refresh_Time 100U
#define BALL_DIGITAL_BUFFER_SIZE 4U
#define BALL_LEFT_MODULE_OFFSET 2U
#define BALL_RIGHT_MODULE_OFFSET 0U
#define BALL_DIGITAL_ZERO_CODE 0x82U

static uint8_t DigitalBuffer[BALL_DIGITAL_BUFFER_SIZE] = {BALL_DIGITAL_ZERO_CODE, BALL_DIGITAL_ZERO_CODE, BALL_DIGITAL_ZERO_CODE, BALL_DIGITAL_ZERO_CODE};
static SoftwareSPI_HandleTypeDef TubeSPI;
DigitalTube_t DigitalTube;

static void DigitalTube_SPItransmit(uint8_t *pData, uint16_t Size)
{
    SoftwareSPI_Transmit(&TubeSPI, pData, Size);
}

void DigitalTubeTask_Init(void)
{
    DigitalTube_Init_t Init;
    SoftwareSPI_InitTypeDef SpiInit;

    /* 中文注释：按新硬件接线，PB15=数据、PB13=移位时钟、PB14=锁存，继续使用GPIO软件移位。 */
    SpiInit.SDA_Port = Tube_SER_GPIO_Port;
    SpiInit.SDA_Pin = Tube_SER_Pin;
    SpiInit.CLK_Port = Tube_SRCLK_GPIO_Port;
    SpiInit.CLK_Pin = Tube_SRCLK_Pin;
    SpiInit.CS_Port = NULL;
    SpiInit.CS_Pin = 0U;
    SpiInit.DelayTick = 1U;
    SpiInit.CLK_CPOL = 1U;
    SpiInit.CS_Level = GPIO_PIN_RESET;
    SoftwareSPI_Init(&TubeSPI, SpiInit);

    Init.Buffer = DigitalBuffer;
    Init.bit_num = sizeof(DigitalBuffer);
    Init.LE_GPIO = Tube_RCLK_GPIO_Port;
    Init.LE_Pin = Tube_RCLK_Pin;
    /* 中文注释：外接FJ8106BH模块接线为OUT0=F、OUT1=G、OUT2=E、OUT3=D、OUT4=C、OUT5=A、OUT6=B、OUT7=DP，需使用对应段码表。 */
    Init.CODE_CA = DIGITAL3BIT_CODE_CA;
    Init.spi_transmit = DigitalTube_SPItransmit;
    DigitalTube_Init(&DigitalTube, Init);

    /* 中文注释：两块模块各使用16位SM16306，四个字节全部写入实际接线对应的数字0段码，确保上电两块都显示0。 */
    memset(DigitalBuffer, DIGITAL3BIT_CODE_CA[0], sizeof(DigitalBuffer));
    DigitalTube.Refresh(&DigitalTube);
}

void BallDigitalTube_Set(uint8_t side, uint8_t value)
{
    uint8_t offset;
    uint8_t segment;

    if (value > 8U)
        return;

    if (side == 0U)
        offset = BALL_LEFT_MODULE_OFFSET;
    else if (side == 1U)
        offset = BALL_RIGHT_MODULE_OFFSET;
    else
        return;

    segment = DIGITAL3BIT_CODE_CA[value];

    /*
     * 中文注释：每块外接数码管模块使用一颗16位SM16306，但仅OUT0~OUT7接七段数码管。
     * 同一模块的两个字节写入相同段码，可避免16位移位高低字节方向差异影响显示。
     * 当前定义链路第一组16bit为左、第二组16bit为右；若实机线束左右顺序相反，仅交换上方两个OFFSET常量。
     */
    DigitalBuffer[offset] = segment;
    DigitalBuffer[offset + 1U] = segment;
    DigitalTube.Refresh(&DigitalTube);
}

void DigitalTube_Task(void)
{
    static uint32_t time = 0U;

    if ((uint32_t)(HAL_GetTick() - time) > DigitalTube_Refresh_Time)
    {
        DigitalTube.Refresh(&DigitalTube);
        time = HAL_GetTick();
    }
}
