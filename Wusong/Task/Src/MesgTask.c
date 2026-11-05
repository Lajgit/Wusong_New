#include "MainTask.h"
#include "MesgTask.h"
#include "CommTask.h"
#include "CtrlTask.h"
#include "port_communicate.h"
#include "port_event.h"

extern Tx_HandleTypeDef Tx1;
extern Rx_HandleTypeDef Rx1;
extern Tx_HandleTypeDef Tx3;
extern Rx_HandleTypeDef Rx3;

extern Motor_Hoolle Motor_Hoolle1;
extern Motor_Hoolle Motor_Hoolle2;
extern Motor_Card Card;
extern Switch_Valve Lock_Valve;
extern ListHandle_t ResendList, DealList;

Event_Handle_t Mesg_event;
// 发送给NFC开锁消息
static uint8_t NFCUnlock_mesg[11] = {0xaa, 0x0f, 0x01, 0x01, 0x01, 0x01, 0x5c, 0x77, 0x08, 0x7f, 0x55};

extern volatile uint16_t HoolleInputPendingCount;
void Mesg_Task(void)
{

    uint8_t HoolleInputPending = 0U;
    uint32_t Primask;
    // 按键进入设置
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_ButtonEnterSetting))
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, 0x03, KEY_EVENT_SHORT);
        EventGroupClearBits(&Mesg_event, MesgEvent_ButtonEnterSetting);
    }
    // 开锁
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_Unlock) == true)
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_AlreadyUnlock, 0x00, 0x00);
        EventGroupClearBits(&Mesg_event, MesgEvent_Unlock);
    }
    // 投珠
    /*
    * 从中断累计值中原子取出一个进珠事件。
    * 临界区只包含计数操作，不包含串口发送。
    */
    Primask = __get_PRIMASK();
    __disable_irq();

    if (HoolleInputPendingCount > 0U)
    {
        HoolleInputPendingCount--;
        HoolleInputPending = 1U;
    }

    __set_PRIMASK(Primask);

    if (HoolleInputPending != 0U)
    {
        Comm_SendMesg_FillData_withResend(
            &Tx1,
            Board_to_Android,
            t_HoolleInput,
            0x00,
            0x00,
            &ResendList);
    }
    // 投币
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CoinInput) == true)
    {
        Comm_SendMesg_FillData_withResend(&Tx1, Board_to_Android, t_CoinInput, 0x00, 0x00, &ResendList);
        EventGroupClearBits(&Mesg_event, MesgEvent_CoinInput);
    }

    /* 原武松：0x05为剩余珠数，ExpandCode=0；吐卡一次/完成分别为0x14/0x15。 */
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_SteelBallOutputTimeout))
    {
        /* 原工程启用BEIJING_NXH_VERSION，不主动上报吐珠超时。 */
        EventGroupClearBits(&Mesg_event, MesgEvent_SteelBallOutputTimeout);
    }
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CardOutputTimeout))
    {
        CommTransmitFillData(&Tx1, Board_to_Android, t_CardOutputTimeOut, Card.Card_num, 0U);
        EventGroupClearBits(&Mesg_event, MesgEvent_CardOutputTimeout);
    }
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_RemainingSteelBall))
    {
        CommTransmitFillData(&Tx1, Board_to_Android, t_RemainingHoolle, Motor_Hoolle1.Hoolle_num, 0U);
        EventGroupClearBits(&Mesg_event, MesgEvent_RemainingSteelBall);
    }
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CardOutputOnce))
    {
        CommTransmitFillData(&Tx1, Board_to_Android, t_CardOnce, 0U, 0U);
        EventGroupClearBits(&Mesg_event, MesgEvent_CardOutputOnce);
    }
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_CardOutputFinish))
    {
        CommTransmitFillData(&Tx1, Board_to_Android, t_CardFinish, 0U, 0U);
        EventGroupClearBits(&Mesg_event, MesgEvent_CardOutputFinish);
    }
    // 版本请求
    if (EventGroupCheckBits(&Mesg_event, MesgEvent_VersionRequest) == true)
    {
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_VersionRequest, (uint32_t)VERSION, 0x00);
        EventGroupClearBits(&Mesg_event, MesgEvent_VersionRequest);
    }
    Resend_Task();
    MesgDeal_Task();
}
