#include "CommTask.h"
#include "MesgTask.h"
#include "CtrlTask.h"
#include "KeyTask.h"
#include "MainTask.h"
#include "FlashTask.h"
#include "DigitalTubeTask.h"
#include "app_crc.h"
#include "app_list.h"
#include "string.h"
#include "usart.h"
#include "gpio.h"

#define Mesg_Head 0xAA
#define Mesg_Tail 0x55

static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg);

ListHandle_t ResendList, DealList;
static ListNode_t ResendList_buffer[100];
static ListNode_t DealList_buffer[100];

static Mesg_TypeDef MesgTable[256];
static Tx_HandleTypeDef *MesgPort[256]; // 每个待重发ID保存原串口，不能固定从安卓口重发
static uint8_t rx2_buffer[512];
static Mesg_TypeDef Receive2_mesg;
Tx_HandleTypeDef Tx2;
Rx_HandleTypeDef Rx2;
static ListHandle_t BallDealList;
static ListNode_t BallDealBuffer[100];
uint32_t BallVersion = 0U;
Mesg_TypeDef CtrlSpecialButton;
Mesg_TypeDef CtrlEncoder;
Mesg_TypeDef CtrlJoystick;
static uint16_t ChannelMask = 0U;
static uint8_t FlowCount = 0U;
static uint8_t FlowPosition = 0U;
static uint8_t FlowStarted = 0U;
static uint16_t FlowPeriod = 0U;
static uint32_t FlowTick = 0U;
/* 控台按键06代替原PD6防摇输入；最后一次触发后静默5秒才解除。 */
static uint8_t AntiShakeActive = 0U;
static uint32_t AntiShakeTick = 0U;
static uint8_t rx1_buffer[512];
static uint8_t rx3_buffer[512];

static Mesg_TypeDef Receive1_mesg;
static Mesg_TypeDef Receive3_mesg;

Tx_HandleTypeDef Tx1;
Rx_HandleTypeDef Rx1;
Tx_HandleTypeDef Tx3;
Rx_HandleTypeDef Rx3;

/* 最近一次收到的控台版本号；未收到0x00版本消息前保持为0。 */
uint32_t CtrlVersion = 0U;

extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;
extern Scene_t Scene;
extern Motor_Card Card;
extern Motor_Hoolle Motor_Hoolle1;
extern Motor_Hoolle Motor_Hoolle2;
extern servo_t Servo1;
extern Switch_Valve Lock_Valve;
extern ListHandle_t ResendList, DealList;
extern Setting_TypeDef Setting;

/// 串口1消息验证
static bool USART1_ReceiveMesg_Verify(void *self, void *mesg)
{
    Rx_HandleTypeDef *rx = (Rx_HandleTypeDef *)self;
    Mesg_TypeDef *Rx_mesg = (Mesg_TypeDef *)mesg;
    uint16_t crc16, mesg_crc16;
    if (Rx_mesg->Head != Mesg_Head || Rx_mesg->Tail != Mesg_Tail)
        return false;
    crc16 = CRC16_calculate(rx->Queue.Buf, 11);
    mesg_crc16 = Rx_mesg->CRC16_H << 8 | Rx_mesg->CRC16_L;
    if (crc16 == mesg_crc16)
        return true;
    return false;
}

/* 通道bit0对应J1/LED12，bit11对应J12/LED1；球盘完成物理位序转换。 */
static void Ball_SetChannel(uint16_t mask)
{
    ChannelMask = mask & 0x0FFFU;
    Comm_SendMesg_FillData(&Tx2, Board_to_Ball, BALL_CMD_CHANNEL_LIGHT, ChannelMask, 0U);
}

/* 普通阀命令补充位为0，洞内阀使用side=0左、1右，要求对端应答。 */
void Ball_SendValve(uint8_t command, uint8_t side)
{
    Comm_SendMesg_FillData_withResend(&Tx2, Board_to_Ball, command, side, 0U, &ResendList);
}

/* 补充位1执行原MiniGameBall_EjectIfOccupied：任一洞有球且两阀空闲时成对推出。 */
static void Ball_RequestEjectIfOccupied(void)
{
    Comm_SendMesg_FillData_withResend(&Tx2, Board_to_Ball, BALL_CMD_HOLE_VALVE, 0U, 1U, &ResendList);
}

void Ball_SendScene(uint8_t scene)
{
    /* 场景以最新值为准，取消旧场景的待重发帧，避免稍后回退到旧后台。 */
    for (ListNode_t *node = ResendList.Head; node != NULL; )
    {
        ListNode_t *next = node->Next;
        if (MesgPort[node->ID] == &Tx2 && MesgTable[node->ID].Code2 == BALL_CMD_SCENE)
            List_DeleteNode(&ResendList, node->ID);
        node = next;
    }
    Comm_SendMesg_FillData_withResend(&Tx2, Board_to_Ball, BALL_CMD_SCENE, scene, 0U, &ResendList);
}

static void Ball_StopFlow(void)
{
    if (FlowCount != 0U)
    {
        FlowCount = 0U;
        FlowStarted = 0U;
        Ball_SetChannel(0U);
    }
}

/* 中文注释：只在升级请求中写RTC备份标记，普通F0继续沿用原复位行为。 */
static void Board_EnterBootloader(void)
{
    uint32_t timeout = 100000U;
    uint8_t retry;

    __HAL_RCC_PWR_CLK_ENABLE();
    __DSB();
    (void)RCC->APB1ENR;
    HAL_PWR_EnableBkUpAccess();
    while (((PWR->CR & PWR_CR_DBP) == 0U) && timeout > 0U)
        timeout--;
    if (timeout == 0U)
    {
        HAL_PWR_DisableBkUpAccess();
        return;
    }
    __HAL_RCC_RTC_ENABLE();
    __DSB();
    (void)RCC->BDCR;
    for (retry = 0U; retry < 3U; retry++)
    {
        RTC->BKP0R = OTA_REQUEST_MAGIC;
        __DSB();
        __ISB();
        if (RTC->BKP0R == OTA_REQUEST_MAGIC)
            break;
    }
    HAL_PWR_DisableBkUpAccess();
    /* 标记不可写时保持APP运行，避免普通重启被误当作已进入升级。 */
    if (retry == 3U)
        return;

    /* 直接关闭硬件输出，不能只设STOP后等待即将停止运行的任务处理。 */
    SteelBall_MotorSwitch(0U);
    Motor_Hoolle1.Motor.LosePower(&Motor_Hoolle1.Motor);
    Motor_Hoolle2.Motor.LosePower(&Motor_Hoolle2.Motor);
    Motor_Hoolle2.Motor.state = DEVICE_STATE_IDLE;
    Card.Switch.off(&Card.Switch);
    Card.Switch.state = DEVICE_STATE_IDLE;
    Lock_Valve.Switch.off(&Lock_Valve.Switch);
    Lock_Valve.Switch.state = DEVICE_STATE_IDLE;
    Ball_StopFlow();
    Ball_SendValve(BALL_CMD_STOP, 0U);

    /* 应答和停阀帧均为阻塞发送；保留中断并等待从板处理后再系统复位。 */
    HAL_Delay(100U);
    System_Reset();
}

/* 流水灯只在游玩/加注场景推进，先处理本轮微动上报再更新灯组。 */
static void Ball_FlowTask(void)
{
    uint16_t mask = 0U;
    uint32_t now = HAL_GetTick();
    if (FlowCount == 0U || (Scene != SCENE_PLAYING && Scene != SCENE_ADDITIONAL))
        return;
    if (FlowStarted != 0U)
    {
        if ((uint32_t)(now - FlowTick) < FlowPeriod)
            return;
        FlowPosition = (FlowPosition + 1U) % 12U;
    }
    for (uint8_t i = 0U; i < FlowCount; i++)
        mask |= (uint16_t)(1U << ((FlowPosition + i) % 12U));
    Ball_SetChannel(mask);
    FlowTick = now;
    FlowStarted = 1U;
}

/// 串口1消息处理：保留原武松安卓命令含义。
static void USART1_Deal(void *Rx_mesg)
{
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;
    uint32_t data = ((uint32_t)mesg->Data1 << 24) | ((uint32_t)mesg->Data2 << 16) |
                    ((uint32_t)mesg->Data3 << 8) | mesg->Data4;

    if (mesg->Code1 == Board_to_Android)
    {
        /* 原方向回显表示应答，同时核对端口和有效负载，避免同ID误删其他消息。 */
        if (MesgPort[mesg->ID] == &Tx1 &&
            memcmp(&MesgTable[mesg->ID].Code1, &mesg->Code1, 8U) == 0)
            List_DeleteNode(&ResendList, mesg->ID);
        return;
    }
    if (mesg->Code1 != Android_to_Board)
        return;
    if (mesg->ACKbyte == 1U)
        USART_RequestMesg(&Tx1, mesg);
    if (mesg->ResendID != 0U && List_IsExistID(&DealList, mesg->ID))
        return;

    switch (mesg->Code2)
    {
    case 0x00U:
        EventGroupSetBits(&Mesg_event, MesgEvent_VersionRequest);
        break;
    case r_HoolleOutput:
        if (data <= 0xFFFFU)
        {
            Hoolle_Output(&Motor_Hoolle1, (uint16_t)data);
            EventGroupSetBits(&Mesg_event, MesgEvent_RemainingSteelBall);
        }
        break;
    case r_CardOutput:
        if (data <= 0xFFFFU)
        {
            Card_Output(&Card, (uint16_t)data);
            EventGroupSetBits(&Mesg_event, MesgEvent_RemainingSteelBall);
        }
        break;
    case r_ValveOutput:
        Ball_SendValve(BALL_CMD_VALVE, 0U);
        break;
    case r_SceneChange:
        if (mesg->Data4 <= SCENE_END)
        {
            if (mesg->Data4 != SCENE_PLAYING && mesg->Data4 != SCENE_ADDITIONAL)
                Ball_StopFlow();
            Scene = (Scene_t)mesg->Data4;
            Ball_SendScene((uint8_t)Scene);
        }
        break;
    case r_ChannelLight:
        Ball_StopFlow();
        Ball_SetChannel((uint16_t)data);
        break;
    case r_ChannelLightFlow:
        if (mesg->Data1 == 1U && mesg->Data2 == 1U && mesg->ExpandCode == 0U &&
            mesg->Data3 >= 1U && mesg->Data3 <= 3U && mesg->Data4 <= 3U)
        {
            uint16_t period = (uint16_t)(mesg->Data4 + 1U) * 100U;
            if (FlowCount != mesg->Data3 || FlowPeriod != period)
            {
                FlowCount = mesg->Data3;
                FlowPeriod = period;
                FlowPosition = 0U;
                FlowTick = HAL_GetTick();
                FlowStarted = 0U;
                Ball_FlowTask();
            }
        }
        break;
    case r_ClearOutput:
        Motor_Hoolle1.ClearMode = 1U;
        Hoolle_Output(&Motor_Hoolle1, 0xFFFFU - Motor_Hoolle1.Hoolle_num);
        break;
    case r_OutputRemaining:
        Hoolle_Output(&Motor_Hoolle1, 0U);
        Card_Output(&Card, 0U);
        Ball_RequestEjectIfOccupied();
        break;
    case r_SaveSetting:
        EventGroupSetBits(&Event, Event_SaveSetting);
        break;
    case r_ResumeSetting:
        ResumeSetting();
        break;
    case r_HoleValveOutput:
        Ball_SendValve(BALL_CMD_HOLE_VALVE, 0U);
        Ball_SendValve(BALL_CMD_HOLE_VALVE, 1U);
        break;
    case r_Unlock:
        Lock_Valve.Switch.state = DEVICE_STATE_START;
        EventGroupSetBits(&Mesg_event, MesgEvent_Unlock);
        break;
    case r_SystemReset:
        if (data == OTA_REQUEST_MAGIC)
            Board_EnterBootloader();
        else
            System_Reset();
        break;
    case r_DeviceStop:
        Ball_StopFlow();
        /* 停止后不能再补发之前的阀动作。 */
        for (ListNode_t *node = ResendList.Head; node != NULL; )
        {
            ListNode_t *next = node->Next;
            if (MesgPort[node->ID] == &Tx2)
                List_DeleteNode(&ResendList, node->ID);
            node = next;
        }
        Ball_SendValve(BALL_CMD_STOP, 0U);
        SteelBall_MotorSwitch(0U);
        Motor_Hoolle2.Motor.state = DEVICE_STATE_STOP;
        Card.Switch.state = DEVICE_STATE_STOP;
        EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputFinish);
        break;
    case r_DigitalTubeData:
        DigitalTube_SetDisplayNumber(data);
        Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, CTRL_CMD_DIGITAL_TUBE, data, 0U);
        break;
    default:
        /* 新板无RGB，原亮度/灯光命令仅按请求应答。 */
        break;
    }
    if (List_IsExistID(&DealList, mesg->ID))
        List_DeleteNode(&DealList, mesg->ID);
    List_AddNode(&DealList, mesg->ID, HAL_GetTick());
}

/* 微动板上报只在主循环处理；重传先应答后去重，防止重复计算输赢或清洞。 */
static void USART2_Deal(void *Rx_mesg)
{
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;
    if (mesg->Code1 == Board_to_Ball)
    {
        if (MesgPort[mesg->ID] == &Tx2 &&
            memcmp(&MesgTable[mesg->ID].Code1, &mesg->Code1, 8U) == 0)
            List_DeleteNode(&ResendList, mesg->ID);
        return;
    }
    if (mesg->Code1 != Ball_to_Board)
        return;
    if (mesg->ACKbyte == 1U)
        USART_RequestMesg(&Tx2, mesg);
    if (mesg->ResendID != 0U && List_IsExistID(&BallDealList, mesg->ID))
        return;
    switch (mesg->Code2)
    {
    case BALL_REPORT_VERSION:
        BallVersion = ((uint32_t)mesg->Data1 << 24) | ((uint32_t)mesg->Data2 << 16) |
                      ((uint32_t)mesg->Data3 << 8) | mesg->Data4;
        break;
    case BALL_REPORT_SWITCH:
        if (mesg->Data4 < 12U && mesg->ExpandCode <= 1U)
        {
            if (Scene == SCENE_PLAYING || Scene == SCENE_ADDITIONAL)
            {
                /* 微动板锁存闭合瞬间的灯态，补充位0=未中奖、1=中奖。 */
                CommTransmitFillData(&Tx1, Board_to_Android,
                    mesg->ExpandCode == 1U ? t_Win : t_Lose, 0U, 0U);
                CommTransmitFillData(&Tx1, Board_to_Android, t_Switch, mesg->Data4, 0U);
            }
        }
        break;
    case BALL_REPORT_HOLE:
        if (mesg->Data4 <= 1U)
        {
            CommTransmitFillData(&Tx1, Board_to_Android, t_Hole, mesg->Data4, 0U);
            if (Scene != SCENE_ADDITIONAL && Scene != SCENE_PLAYING &&
                Scene != SCENE_HAPPY1 && Scene != SCENE_HAPPY2 && Scene != SCENE_SETTING)
            {
                Ball_RequestEjectIfOccupied();
            }
        }
        break;
    }
    if (List_IsExistID(&BallDealList, mesg->ID))
        List_DeleteNode(&BallDealList, mesg->ID);
    List_AddNode(&BallDealList, mesg->ID, HAL_GetTick());
}

static void USART3_Deal(void *Rx_mesg)
{
    Mesg_TypeDef *mesg = (Mesg_TypeDef *)Rx_mesg;
    if (mesg->Code1 != Ctrl_to_Board)
        return;
    if (mesg->ACKbyte == 1U)
        USART_RequestMesg(&Tx3, mesg);
    switch (mesg->Code2)
    {
    case CTRL_EVENT_VERSION:
        CtrlVersion = ((uint32_t)mesg->Data1 << 24) | ((uint32_t)mesg->Data2 << 16) |
                      ((uint32_t)mesg->Data3 << 8) | mesg->Data4;
        break;
    case CTRL_EVENT_BUTTON:
        if (mesg->Data4 >= 1U && mesg->Data4 <= 6U &&
            mesg->ExpandCode >= KEY_EVENT_SHORT && mesg->ExpandCode <= KEY_EVENT_RELEASE)
        {
            if (mesg->Data4 == 6U)
            {
                /* 控台按键06专用于防摇。现有小卡可能只发短按/重复长按，不依赖松开帧。 */
                if (mesg->ExpandCode != KEY_EVENT_RELEASE)
                {
                    AntiShakeTick = HAL_GetTick();
                    if (AntiShakeActive == 0U)
                    {
                        AntiShakeActive = 1U;
                        CommTransmitFillData(&Tx1, Board_to_Android, t_AntiShake, 0U, 0U);
                    }
                }
            }
            else
                Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Button, mesg->Data4, mesg->ExpandCode);
        }
        break;
    /* 这三类输入暂未定义安卓功能码，只保留原始帧，不占用旧安卓编号。 */
    case CTRL_EVENT_SPECIAL_BUTTON:
        if (mesg->Data4 >= 1U && mesg->Data4 <= 4U &&
            mesg->ExpandCode >= KEY_EVENT_SHORT && mesg->ExpandCode <= KEY_EVENT_RELEASE)
            CtrlSpecialButton = *mesg;
        break;
    case CTRL_EVENT_ENCODER:
        if (mesg->ExpandCode <= 2U)
            CtrlEncoder = *mesg;
        break;
    case CTRL_EVENT_JOYSTICK:
        if (mesg->Data4 >= 1U && mesg->Data4 <= 8U &&
            mesg->ExpandCode >= KEY_EVENT_SHORT && mesg->ExpandCode <= KEY_EVENT_RELEASE)
            CtrlJoystick = *mesg;
        break;
    }
}

/// 发送消息，无重传
static uint8_t USART_SendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    static uint8_t ID = 0;
    uint8_t data[14];
    uint16_t crc;

    /* 不覆盖尚未应答的帧，安卓、控台、球盘共用ID分配器。 */
    for (uint16_t i = 0U; i < 256U; i++)
    {
        ID++;
        if (!List_IsExistID(&ResendList, ID))
            break;
    }
    mesg->ResendID = 0;    // 重发次数清零
    mesg->ID = ID;         // 赋予新ID号
    MesgTable[ID] = *mesg; // 保存消息包
    MesgPort[ID] = Tx;
    memcpy(data, mesg, 14);
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return ID;
}

/// 填入参数发送消息，无重传
uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode)
{
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.ResendID = 0;
    mesg.ID = 0;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)(data);
    mesg.ACKbyte = 0x00;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    return USART_SendMesg(Tx, &mesg);
}

/// 填充数据发送消息并加入重发列表
uint8_t Comm_SendMesg_FillData_withResend(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode, ListHandle_t *List)
{
    uint8_t ID;
    Mesg_TypeDef mesg = {0};
    mesg.Head = Mesg_Head;
    mesg.ResendID = 0;
    mesg.ID = 0;
    mesg.Code1 = code_1;
    mesg.Code2 = code_2;
    mesg.Data1 = (uint8_t)(data >> 24);
    mesg.Data2 = (uint8_t)(data >> 16);
    mesg.Data3 = (uint8_t)(data >> 8);
    mesg.Data4 = (uint8_t)(data);
    mesg.ACKbyte = 0x01;
    mesg.ExpandCode = expandCode;
    mesg.Tail = Mesg_Tail;
    ID = USART_SendMesg(Tx, &mesg);
    List_AddNode(List, ID, HAL_GetTick());
    return ID;
}

/// 发送重发消息
static uint8_t USART_ReSendMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;
    mesg->ResendID++;
    if (mesg->ResendID > Max_Resend_Times)
        return 1;
    data[0] = Mesg_Head;
    data[1] = mesg->ResendID;
    data[2] = mesg->ID;
    data[3] = mesg->Code1;
    data[4] = mesg->Code2;
    data[5] = mesg->Data1;
    data[6] = mesg->Data2;
    data[7] = mesg->Data3;
    data[8] = mesg->Data4;
    data[9] = mesg->ACKbyte; // 回显或重传均保持原ACK位与方向
    data[10] = mesg->ExpandCode;
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    data[13] = Mesg_Tail;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
    return 0;
}

/// 发送应答消息
static void USART_RequestMesg(Tx_HandleTypeDef *Tx, Mesg_TypeDef *mesg)
{
    uint8_t data[14];
    uint16_t crc;
    data[0] = Mesg_Head;
    data[1] = mesg->ResendID;
    data[2] = mesg->ID;
    data[3] = mesg->Code1;
    data[4] = mesg->Code2;
    data[5] = mesg->Data1;
    data[6] = mesg->Data2;
    data[7] = mesg->Data3;
    data[8] = mesg->Data4;
    data[9] = mesg->ACKbyte; // 回显或重传均保持原ACK位与方向
    data[10] = mesg->ExpandCode;
    crc = CRC16_calculate(data, 11);
    data[11] = crc >> 8;
    data[12] = crc;
    data[13] = Mesg_Tail;
    HAL_UART_Transmit(Tx->huart, data, 14, 100);
}

/* ----------检测重发消息---------- */
void Resend_Task(void)
{
    ListNode_t *current = ResendList.Head;
    uint32_t now = HAL_GetTick();
    while (current != NULL)
    {
        ListNode_t *next = current->Next;
        if ((uint32_t)(now - current->Value) >= ResendTrigger_Time)
        {
            USART_ReSendMesg(MesgPort[current->ID], &MesgTable[current->ID]);
            current->Value = now;
            if (MesgTable[current->ID].ResendID >= Max_Resend_Times)
                List_DeleteNode(&ResendList, current->ID);
        }
        current = next;
    }
}

/* ----------清除已执行消息任务---------- */
void MesgDeal_Task(void)
{
    ListHandle_t *lists[2] = {&DealList, &BallDealList};
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0U; i < 2U; i++)
    {
        ListNode_t *current = lists[i]->Head;
        while (current != NULL)
        {
            ListNode_t *next = current->Next;
            if ((uint32_t)(now - current->Value) >= MesgDeal_Time)
                List_DeleteNode(lists[i], current->ID);
            current = next;
        }
    }
}

/* ----------通信初始化---------- */
void CommInit(void)
{
    List_Create(&ResendList, ResendList_buffer, 100);
    List_Create(&DealList, DealList_buffer, 100);
    List_Create(&BallDealList, BallDealBuffer, 100);

    /// USART1：主板与安卓
    Rx_InitTypeDef Rxinit = {0};
    Rxinit.Mesg_Len = 14U;
    Rxinit.huart = &huart1;
    Rxinit.RingBuf = rx1_buffer;
    Rxinit.RingBuf_Size = sizeof(rx1_buffer);
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART1_ReceiveMesg_Verify;
    Rxinit.Deal = USART1_Deal;
    Communicate_Rx_Init(&Rx1, Rxinit);

    /// USART2：PD5/PD6连接微动板USART1 PA9/PA10。
    Rxinit.huart = &huart2;
    Rxinit.RingBuf = rx2_buffer;
    Rxinit.RingBuf_Size = sizeof(rx2_buffer);
    Rxinit.Deal = USART2_Deal;
    Communicate_Rx_Init(&Rx2, Rxinit);

    /// USART3：主板与控台，沿用同一套14字节帧和CRC16校验
    Rxinit.huart = &huart3;
    Rxinit.RingBuf = rx3_buffer;
    Rxinit.RingBuf_Size = sizeof(rx3_buffer);
    Rxinit.Frame_Head = Mesg_Head;
    Rxinit.Frame_Tail = Mesg_Tail;
    Rxinit.Receive = Rx_Receive;
    Rxinit.Verify = USART1_ReceiveMesg_Verify;
    Rxinit.Deal = USART3_Deal;
    Communicate_Rx_Init(&Rx3, Rxinit);

    Tx_InitTypeDef Tx_init;
    Tx_init.huart = &huart1;
    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;
    Communicate_Tx_Init(&Tx1, Tx_init);

    Tx_init.huart = &huart2;
    Communicate_Tx_Init(&Tx2, Tx_init);

    Tx_init.huart = &huart3;
    Tx_init.hdma = NULL;
    Tx_init.TxBuf = NULL;
    Tx_init.TxBuf_Size = 0;
    Communicate_Tx_Init(&Tx3, Tx_init);
}

void CommTask(void)
{
    Rx1.Receive(&Rx1, &Receive1_mesg, 14U);
    Rx2.Receive(&Rx2, &Receive2_mesg, 14U);
    Rx3.Receive(&Rx3, &Receive3_mesg, 14U);
    if (AntiShakeActive != 0U && (uint32_t)(HAL_GetTick() - AntiShakeTick) >= 5000U)
    {
        AntiShakeActive = 0U;
        CommTransmitFillData(&Tx1, Board_to_Android, t_AntiShake, 1U, 0U);
    }
    /* 先处理本轮微动上报，再推进流水灯。 */
    if (Scene == SCENE_PLAYING || Scene == SCENE_ADDITIONAL)
        Ball_FlowTask();
}
/* 兼容原武松上报帧的ACK位，功能码和Data/Expand布局保持不变。 */
void CommTransmitFillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode)
{
    switch (code_2)
    {
    case t_HoolleInput: case t_CoinInput: case t_HoolleOutputTimeOut: case t_CardOutputTimeOut:
    case t_Switch: case t_Hole: case t_HoolleFinish: case t_CardOnce: case t_CardFinish:
    case t_Win: case t_Lose: case t_AntiShake:
        Comm_SendMesg_FillData_withResend(Tx, code_1, code_2, data, expandCode, &ResendList);
        break;
    default:
        Comm_SendMesg_FillData(Tx, code_1, code_2, data, expandCode);
        break;
    }
}
