#include "CommTask.h"
#include "LightTask.h"
#include "app_crc.h"
#include "string.h"
#include "usart.h"
#include "app_list.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55

static uint8_t rx_buffer[256];
static Mesg_TypeDef Receive_mesg;

Tx_HandleTypeDef Tx;
Rx_HandleTypeDef Rx;
uint8_t BallScene = BALL_SCENE_IDLE;
static uint16_t GameChannelMask = 0U; // 保存主控游戏掩码，后台测试灯态不覆盖此值

#define RESEND_MS 1000U
#define DEAL_KEEP_MS 4500U
#define VALVE_PULSE_MS 800U

static Mesg_TypeDef Pending[256];
static ListHandle_t ResendList, DealList;
static ListNode_t ResendPool[32], DealPool[32];
static const uint16_t ValvePin[3] = {Valve_Left_Pin, Valve_Right_Pin, Valve_Block_Pin};
static uint32_t ValveTick[3];
static uint8_t ValveActive[3]; // 下标依次为左、右、挡珠；0空闲、1吸合、2脉冲间隔
static uint8_t ValveRepeat[3];

/* 应答直接回显已校验的原帧，保持原方向、ID、ACK位及CRC。 */
static void USART_Echo(Mesg_TypeDef *mesg)
{
    HAL_UART_Transmit(Tx.huart, (uint8_t *)mesg, sizeof(*mesg), 100U);
}

/* 本板自行关断电磁阀，不依赖主控再次下发关闭命令。 */
void BallValve_Task(void)
{
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0U; i < 3U; i++)
    {
        if (ValveActive[i] == 1U && (uint32_t)(now - ValveTick[i]) >= VALVE_PULSE_MS)
        {
            HAL_GPIO_WritePin(GPIOB, ValvePin[i], GPIO_PIN_RESET);
            ValveActive[i] = ValveRepeat[i] != 0U ? 2U : 0U;
            ValveTick[i] = now;
        }
        else if (ValveActive[i] == 2U && (uint32_t)(now - ValveTick[i]) >= 300U)
        {
            ValveRepeat[i]--;
            ValveTick[i] = now;
            ValveActive[i] = 1U;
            HAL_GPIO_WritePin(GPIOB, ValvePin[i], GPIO_PIN_SET);
        }
    }
}

static void BallValve_Start(uint8_t index)
{
    /* 忙时不延长吸合时间，断线/重传也由球盘本地800ms关断。 */
    if (index < 3U && ValveActive[index] == 0U)
    {
        ValveTick[index] = HAL_GetTick();
        ValveActive[index] = 1U;
        /* 左右洞保留原TriggerCount=1的追加一次脉冲，挡珠阀只执行一次。 */
        ValveRepeat[index] = index < 2U ? 1U : 0U;
        HAL_GPIO_WritePin(GPIOB, ValvePin[index], GPIO_PIN_SET);
    }
}

static void BallValve_EjectIfOccupied(void)
{
    /* 原武松任一洞有球、且两洞阀均空闲时，才成对执行推出。 */
    if ((HAL_GPIO_ReadPin(GPIOB, Hole_Left_Pin) == GPIO_PIN_SET ||
         HAL_GPIO_ReadPin(GPIOB, Hole_Right_Pin) == GPIO_PIN_SET) &&
        ValveActive[0] == 0U && ValveActive[1] == 0U)
    {
        BallValve_Start(0U);
        BallValve_Start(1U);
    }
}

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
    if (mesg->Code1 == Ball_to_Board)
    {
        /* 主控以原方向回显的上报帧为应答，只删除负载匹配的待重发消息。 */
        if (memcmp(&Pending[mesg->ID].Code1, &mesg->Code1, 8U) == 0)
            List_DeleteNode(&ResendList, mesg->ID);
        return;
    }
    if (mesg->Code1 != Board_to_Ball)
        return;
    if (mesg->ACKbyte == 1U)
        USART_Echo(mesg);
    /* 已执行动作的重传只回显确认，不再次启动阀或重置场景。 */
    if (mesg->ResendID != 0U && List_IsExistID(&DealList, mesg->ID))
        return;
    switch (mesg->Code2)
    {
    case BALL_CMD_VERSION:
        Comm_SendMesg_FillData(&Tx, Ball_to_Board, BALL_REPORT_VERSION, (uint32_t)VERSION, 0U);
        break;
    case BALL_CMD_CHANNEL_LIGHT:
        GameChannelMask = (((uint16_t)mesg->Data3 << 8) | mesg->Data4) & 0x0FFFU;
        if (BallScene != BALL_SCENE_SETTING)
            BallLight_SetChannels(GameChannelMask);
        break;
    case BALL_CMD_VALVE:
        BallValve_Start(2U);
        break;
    case BALL_CMD_HOLE_VALVE:
        if (mesg->ExpandCode == 0U && mesg->Data4 <= 1U)
            BallValve_Start(mesg->Data4);
        else if (mesg->ExpandCode == 1U && mesg->Data4 == 0U)
            BallValve_EjectIfOccupied();
        break;
    case BALL_CMD_SCENE:
        if (mesg->Data4 <= BALL_SCENE_END && BallScene != mesg->Data4)
        {
            BallScene = mesg->Data4;
            /* 后台由实时微动控制灯；离开后台恢复游戏掩码，测试灯态不污染中奖通道。 */
            BallLight_SetChannels(BallScene == BALL_SCENE_SETTING ? 0U : GameChannelMask);
        }
        break;
    case BALL_CMD_STOP:
        HAL_GPIO_WritePin(GPIOB, Valve_Left_Pin | Valve_Right_Pin | Valve_Block_Pin, GPIO_PIN_RESET);
        memset(ValveActive, 0, sizeof(ValveActive));
        memset(ValveRepeat, 0, sizeof(ValveRepeat));
        break;
    }
    if (mesg->ACKbyte == 1U)
    {
        if (List_IsExistID(&DealList, mesg->ID))
            List_DeleteNode(&DealList, mesg->ID);
        List_AddNode(&DealList, mesg->ID, HAL_GetTick());
    }
}

static uint8_t USART_SendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    static uint8_t ID = 0;
    uint8_t data[14];
    uint16_t crc;

    for (uint16_t i = 0U; i < 256U; i++)
    {
        ID++;
        if (!List_IsExistID(&ResendList, ID))
            break;
    }
    mesg->ResendID = 0;
    mesg->ID = ID;
    if (mesg->ACKbyte == 1U)
    {
        Pending[ID] = *mesg;
        List_AddNode(&ResendList, ID, HAL_GetTick());
    }
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
    /* 微动和洞口属于关键事件，需要主控应答；版本响应不进入重发队列。 */
    mesg.ACKbyte = (code_2 == BALL_REPORT_SWITCH || code_2 == BALL_REPORT_HOLE) ? 1U : 0U;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    return USART_SendMesg(Tx, &mesg);
}

void CommInit(void)
{
    Rx_InitTypeDef Rxinit = {0};
    Tx_InitTypeDef Tx_init = {0};

    List_Create(&ResendList, ResendPool, 32U);
    List_Create(&DealList, DealPool, 32U);
    /* 微动板USART1/PA9/PA10连接主控USART2/PD5/PD6。 */
    Rxinit.huart = &huart1;
    Rxinit.RingBuf = rx_buffer;
    Rxinit.RingBuf_Size = sizeof(rx_buffer);
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Mesg_Len = 14U;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART_ReceiveMesg_Verify;
    Rxinit.Deal = USART_Deal;
    Communicate_Rx_Init(&Rx, Rxinit);

    Tx_init.huart = &huart1;
    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;
    Communicate_Tx_Init(&Tx, Tx_init);
}

void CommTask(void)
{
    ListNode_t *node;
    uint32_t now = HAL_GetTick();
    Rx.Receive(&Rx, &Receive_mesg, 14U);
    /* 保持原ID重发，递增重传次数并重算CRC，最多补发三次。 */
    node = ResendList.Head;
    while (node != NULL)
    {
        ListNode_t *next = node->Next;
        if ((uint32_t)(now - node->Value) >= RESEND_MS)
        {
            uint8_t frame[14];
            uint16_t crc;
            Pending[node->ID].ResendID++;
            memcpy(frame, &Pending[node->ID], sizeof(frame));
            crc = CRC16_calculate(frame, 11U);
            frame[11] = (uint8_t)(crc >> 8);
            frame[12] = (uint8_t)crc;
            HAL_UART_Transmit(Tx.huart, frame, sizeof(frame), 100U);
            node->Value = now;
            if (Pending[node->ID].ResendID >= 3U)
                List_DeleteNode(&ResendList, node->ID);
        }
        node = next;
    }
    node = DealList.Head;
    while (node != NULL)
    {
        ListNode_t *next = node->Next;
        if ((uint32_t)(now - node->Value) >= DEAL_KEEP_MS)
            List_DeleteNode(&DealList, node->ID);
        node = next;
    }
}
