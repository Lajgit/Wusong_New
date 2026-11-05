#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"
#include "app_list.h"

#define VERSION 0x01000400U
/* F0数据为42 4F 54 41时请求串口升级，与Bootloader的RTC备份标记一致。 */
#define OTA_REQUEST_MAGIC 0x424F5441U
/* 功能码1表示方向；三条串口共用同一14字节帧格式。 */
#define Board_to_Android 0x00U
#define Android_to_Board 0x01U
#define Board_to_Ctrl 0x02U
#define Ctrl_to_Board 0x03U
#define Board_to_Ball 0x04U
#define Ball_to_Board 0x05U

#define CTRL_CMD_GET_VERSION 0x00U
#define CTRL_CMD_DIGITAL_TUBE 0x01U
#define CTRL_EVENT_VERSION 0x00U
#define CTRL_EVENT_BUTTON 0x01U
#define CTRL_EVENT_SPECIAL_BUTTON 0x02U
#define CTRL_EVENT_ENCODER 0x03U
#define CTRL_EVENT_JOYSTICK 0x04U
#define KEY_EVENT_SHORT 0x01U
#define KEY_EVENT_LONG 0x02U
#define KEY_EVENT_RELEASE 0x03U

/* 微动板命令：通道掩码用Data3/4，阀位置与场景号用Data4。 */
#define BALL_CMD_VERSION 0x00U
#define BALL_CMD_CHANNEL_LIGHT 0x01U
#define BALL_CMD_VALVE 0x02U
#define BALL_CMD_HOLE_VALVE 0x03U // 补充位0按左右直接推出，补充位1且Data4=0条件成对清洞
#define BALL_CMD_SCENE 0x07U // Data4=原场景0~11，场景0启用后台微动跟灯
/* 将原安卓0xFF停止命令延伸到球盘，立即断开三个阀并取消待重发动作。 */
#define BALL_CMD_STOP 0xFFU
#define BALL_REPORT_VERSION 0x00U
#define BALL_REPORT_SWITCH 0x01U // Data4=00~0B，补充位为闭合时中奖快照0/1
#define BALL_REPORT_HOLE 0x02U

#define ResendTrigger_Time 1000U
#define MesgDeal_Time 4500U
#define Max_Resend_Times 3U

/* 安卓命令编号严格沿用Wusong_New_1.7_fixed。 */
#define r_HoolleOutput 0x01U
#define r_CardOutput 0x02U
#define r_ValveOutput 0x03U
#define r_SceneChange 0x07U
#define r_ChannelLight 0x09U
#define r_ClearOutput 0x0BU
#define r_OutputRemaining 0x0CU
#define r_SaveSetting 0x0EU
#define r_HoleValveOutput 0x0FU
#define r_Unlock 0x10U
#define r_ResumeSetting 0x11U
#define r_DigitalTubeData 0x16U
#define r_ChannelLightFlow 0x21U
#define r_SystemReset 0xF0U
#define r_DeviceStop 0xFFU

#define t_VersionRequest 0x00U
#define t_HoolleInput 0x01U
#define t_CoinInput 0x02U
#define t_Button 0x03U
#define t_SettingButton 0x04U
#define t_RemainingHoolle 0x05U
#define t_HoolleOutputTimeOut 0x07U
#define t_CardOutputTimeOut 0x08U
#define t_AlreadyUnlock 0x0DU
#define t_Switch 0x0EU
#define t_Hole 0x12U
#define t_HoolleFinish 0x13U
#define t_CardOnce 0x14U
#define t_CardFinish 0x15U
#define t_Win 0x16U
#define t_Lose 0x17U
#define t_AntiShake 0x18U

/* 数据四字节与CRC高字节在前；CRC覆盖帧头至补充位共11字节。 */
typedef struct
{
    uint8_t Head;
    uint8_t ResendID;
    uint8_t ID;
    uint8_t Code1;
    uint8_t Code2;
    uint8_t Data1;
    uint8_t Data2;
    uint8_t Data3;
    uint8_t Data4;
    uint8_t ACKbyte;
    uint8_t ExpandCode;
    uint8_t CRC16_H;
    uint8_t CRC16_L;
    uint8_t Tail;
} Mesg_TypeDef;

uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode);
uint8_t Comm_SendMesg_FillData_withResend(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode, ListHandle_t *List);
void CommTransmitFillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode);
void Ball_SendValve(uint8_t command, uint8_t side);
void Ball_SendScene(uint8_t scene);
void Resend_Task(void);
void MesgDeal_Task(void);
void CommInit(void);
void CommTask(void);

extern uint32_t CtrlVersion;
extern uint32_t BallVersion;
/* 原安卓协议未定义这三类输入，先保存原始帧，避免占用吐卡等已有功能码。 */
extern Mesg_TypeDef CtrlSpecialButton;
extern Mesg_TypeDef CtrlEncoder;
extern Mesg_TypeDef CtrlJoystick;
#endif
