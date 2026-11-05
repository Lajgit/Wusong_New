#include "CommTask.h"
#include "DigitalTubeTask.h"
#include "LightTask.h"
#include "app_crc.h"
#include "string.h"
#include "usart.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55

static uint8_t rx_buffer[256];
static Mesg_TypeDef Receive_mesg;

Tx_HandleTypeDef Tx;
Rx_HandleTypeDef Rx;

static bool USART_ReceiveMesg_Verify(void *self, void *mesg)
{
    Rx_HandleTypeDef *rx = (Rx_HandleTypeDef *)self;
    Mesg_TypeDef *Rx_mesg = (Mesg_TypeDef *)mesg;
    uint16_t crc16 = CRC16_calculate(rx->Queue.Buf, 11);
    uint16_t mesg_crc16 = ((uint16_t)Rx_mesg->CRC16_H << 8) | Rx_mesg->CRC16_L;
    return crc16 == mesg_crc16;
}

static void USART_Deal(void *Rx_mesg)
{
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;

    if (mesg->Code1 != Board_to_Ball)
        return;

    switch (mesg->Code2)
    {
    case BALL_CMD_VERSION:
        /* 中文注释：主板请求球盘版本后立即按0x05/0x00返回四字节版本号。 */
        Comm_SendMesg_FillData(&Tx, Ball_to_Board, BALL_REPORT_VERSION, (uint32_t)VERSION, 0U);
        break;

    case BALL_CMD_DIGITAL_TUBE:
        /* 中文注释：协议Data3=左右位置，Data4=0x00~0x08显示值。 */
        BallDigitalTube_Set(mesg->Data3, mesg->Data4);
        break;

    case BALL_CMD_RGB_MODE:
        BallLight_SetMode(mesg->Data3, mesg->Data4, mesg->ExpandCode);
        break;

    case BALL_CMD_BRIGHTNESS:
        BallLight_SetBrightness(mesg->Data4);
        break;

    default:
        break;
    }
}

static uint8_t USART_SendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    static uint8_t ID = 0;
    uint8_t data[14];
    uint16_t crc;

    ID++;
    mesg->ResendID = 0;
    mesg->ID = ID;
    memcpy(data, mesg, 14);
    crc = CRC16_calculate(data, 11);
    data[11] = (uint8_t)(crc >> 8);
    data[12] = (uint8_t)crc;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return ID;
}

uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode)
{
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)data;
    mesg.ACKbyte = 0x00;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    return USART_SendMesg(Tx, &mesg);
}

void CommInit(void)
{
    Rx_InitTypeDef Rxinit = {0};
    Tx_InitTypeDef Tx_init = {0};

    /* 中文注释：球盘使用USART2/PA2/PA3与主板通信。 */
    Rxinit.huart = &huart2;
    Rxinit.RingBuf = rx_buffer;
    Rxinit.RingBuf_Size = sizeof(rx_buffer);
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Mesg_Len = 14U;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART_ReceiveMesg_Verify;
    Rxinit.Deal = USART_Deal;
    Communicate_Rx_Init(&Rx, Rxinit);

    Tx_init.huart = &huart2;
    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;
    Communicate_Tx_Init(&Tx, Tx_init);
}

void CommTask(void)
{
    Rx.Receive(&Rx, &Receive_mesg, 14);
}
