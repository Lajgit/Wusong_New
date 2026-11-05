#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"
#include "app_list.h"

//V1.0.4修改数码管为旧三位数码管显示逻辑（旧三位数码管拆掉C5电容）

/// 版本号
#define VERSION_MAJOR  1U
#define VERSION_MINOR  0U
#define VERSION_PATCH  4U
#define VERSION_BUILD  0U

#define VERSION ((VERSION_MAJOR << 24) | \
                 (VERSION_MINOR << 16) | \
                 (VERSION_PATCH << 8)  | \
                  VERSION_BUILD)

/// 通信方向
#define Board_to_Android 0x00 // 主板->安卓
#define Android_to_Board 0x01 // 安卓->主板
#define Board_to_Ctrl    0x02 // 主板->控台
#define Ctrl_to_Board    0x03 // 控台->主板

/* 主板->控台功能码，沿用小丑控台14字节协议。 */
#define CTRL_CMD_GET_VERSION            0x00U // 请求控台版本号
#define CTRL_CMD_DIGITAL_TUBE           0x01U // 数码管，Data1~Data4为4字节数据
#define CTRL_CMD_LIGHT_BELT_BRIGHTNESS  0x02U // 灯带亮度，Data4=0x00~0x0A
#define CTRL_CMD_LIGHT_SCENE            0x03U // 灯光场景，ExpandCode=位置/场景编号
#define CTRL_CMD_BRIGHTNESS             0x04U // 控台亮度，Data4=0x00~0x0A
#define CTRL_CMD_BUTTON_LIGHT           0x05U // 控台按键灯，Data3=位置，Data4=颜色

/* 控台->主板功能码。 */
#define CTRL_EVENT_VERSION              0x00U // 控台版本号
#define CTRL_EVENT_BUTTON               0x01U // 普通按键
#define CTRL_EVENT_SPECIAL_BUTTON       0x02U // 特殊功能按键
#define CTRL_EVENT_ENCODER              0x03U // 旋转编码器

#define CTRL_BRIGHTNESS_MAX             0x0AU
#define CTRL_BUTTON_POSITION_MAX        0x08U
#define CTRL_BUTTON_COLOR_MAX           0x08U

#define CTRL_KEY_SHORT                  0x01U
#define CTRL_KEY_LONG                   0x02U
#define CTRL_KEY_RELEASE                0x03U

#define CTRL_ENCODER_LEFT               0x00U
#define CTRL_ENCODER_RIGHT              0x01U
#define CTRL_ENCODER_PRESS              0x02U

#define CTRL_BUTTON_LIGHT_ON            0x00U
#define CTRL_BUTTON_LIGHT_OFF           0x01U
#define CTRL_BUTTON_LIGHT_BLINK         0x03U

#define ResendTrigger_Time 1000 // 重新发送触发时间ms
#define MesgDeal_Time 250       // 消息处理时间
#define Max_Resend_Times 3      // 最大重新发送次数

/// APP写入RTC备份寄存器，请求Bootloader进入串口升级
#define OTA_REQUEST_MAGIC 0x424F5441U // ASCII "BOTA"

/*
 * 中文注释：出货类命令和0x07出货超时统一使用以下类型值。
 * 0x00=扭蛋（Motor_Hoolle2），0x01=钢珠（Motor_Hoolle1）。
 */
#define HOOLLE_TYPE_EGG 0x00U
#define HOOLLE_TYPE_STEEL_BALL 0x01U

/*
 * 中文注释：0x05剩余待出数量为兼容已有安卓协议保留卡片0x01：
 * 0x00=扭蛋，0x01=卡片，0x02=钢珠。
 */
#define REMAIN_TYPE_EGG 0x00U
#define REMAIN_TYPE_CARD 0x01U
#define REMAIN_TYPE_STEEL_BALL 0x02U

/* 中文注释：0x03/0x04按键消息的ExpandCode定义。 */
#define KEY_EVENT_SHORT 0x01U
#define KEY_EVENT_LONG 0x02U
#define KEY_EVENT_RELEASE 0x03U

/* 中文注释：0x0A灯带控制协议定义，Data4选择第几路，ExpandCode选择开关状态。 */
#define LED_OUTPUT_CHANNEL_1 0x01U
#define LED_OUTPUT_CHANNEL_2 0x02U
#define LED_OUTPUT_OFF 0x00U
#define LED_OUTPUT_ON 0x01U

/* 中文注释：0x21吐珠电机手动开关的ExpandCode定义。 */
#define MOTOR_SWITCH_OFF 0x00U
#define MOTOR_SWITCH_ON 0x01U

/// 主板发送给安卓的消息功能码
#define t_VersionRequest 0x00      // 版本请求应答
#define t_HoolleInput 0x01         // 投入弹珠
#define t_CoinInput 0x02           // 投入硬币
#define t_Button 0x03              // 主板直连游玩/拍拍按键
#define t_SettingButton 0x04       // 主板直连后台设置按键
#define t_RemainingHoolle 0x05     // 剩余待出数量，ExpandCode区分扭蛋/卡片/钢珠
#define t_WinOrLoss 0x06           // 游戏结果（旧协议保留，当前未主动发送）
#define t_HoolleOutputTimeOut 0x07 // 出货超时，ExpandCode区分扭蛋/钢珠
#define t_CardOutputTimeOut 0x08   // 卡片输出超时
#define t_NFCEnterSetting 0x09     // NFC进入后台（旧协议保留，当前未主动发送）
#define t_UnlockCardStatus 0x0A    // 解锁卡片状态（旧协议保留，当前未主动发送）
#define t_BackStageCardStatus 0x0B // 后台卡片状态（旧协议保留，当前未主动发送）
#define t_CardID 0x0C              // 绑定卡片ID（旧协议保留，当前未主动发送）
#define t_AlreadyUnlock 0x0D       // 已开锁
#define t_LightEye 0x0E            // 光眼（旧协议保留，当前未主动发送）
#define t_Encoder 0x0F             // 控台编码器转发
#define t_ChannelRequest 0x10      // 击中通道位置反馈（旧协议保留，当前未主动发送）
#define t_ClearRemainMesg 0x11     // 清除剩余卡片提示
#define t_IntoHigherStage 0x12     // 进入高级后台（旧协议保留，当前未主动发送）
#define t_CtrlSpecialButton 0x15   // 控台特殊功能按键转发

/// 主板接收到安卓的消息功能码
#define r_GetVersion 0x00             // 获取版本信息
#define r_HoolleOutput 0x01           // 扭蛋/钢珠输出
#define r_CardOutput 0x02             // 卡片输出
#define r_ValveTrigger 0x03           // 触发电磁阀（旧协议保留，当前未处理）
#define r_BoardLightness 0x04         // 球盘亮度（旧协议保留，当前未处理）
#define r_LightBoardLightness 0x05    // 灯板亮度（旧协议保留，当前未处理）
#define r_LightBeltLightness 0x06     // 灯带亮度（旧协议保留，当前未处理）
#define r_SceneChange 0x07            // 场景编号（旧协议保留，当前未处理）
#define r_WinChannel 0x08             // 中奖通道（旧协议保留，当前未处理）
#define r_LittleGameResult 0x09       // 小游戏输赢结果（旧协议保留，当前未处理）
#define r_ButtonLight 0x0A            // 12V灯带控制，Data4=1/2选择两路三组LED，ExpandCode=0关/1开
#define r_OutputAllHoolle 0x0B        // 清空扭蛋/钢珠
#define r_OutputRemainingItem 0x0C    // 继续扭蛋和卡片剩余输出
#define r_ResumeDefultSetting 0x0D    // 恢复默认设置
#define r_SaveSetting 0x0E            // 保存设置
#define r_HoleValveTrigger 0x0F       // 洞内电磁阀触发（旧协议保留，当前未处理）
#define r_Unlock 0x10                 // 开锁
#define r_ResumeBoundCard 0x11        // 重新绑卡（旧协议保留，当前未处理）
#define r_WirelessMasterSetting 0x12  // 无线通信主从设置（旧协议保留，当前未处理）
#define r_WirelessChannelSetting 0x13 // 无线通信信道设置（旧协议保留，当前未处理）
#define r_ServoControl 0x14           // 舵机控制（旧协议保留，当前未处理）
#define r_LightControl 0x15           // 灯控制（旧协议保留，当前未处理）
#define r_DigitalTubeData 0x16        // 数码管显示数据，本地显示并同步转发控台
#define r_CtrlButtonLight 0x17         // 控台按键灯，Data3=位置、Data4=颜色、ExpandCode=状态
#define r_CtrlLightness 0x18           // 控台亮度
#define r_ServoReset 0x20             // 舵机1归零
#define r_SteelBallMotorSwitch 0x21   // 吐珠/钢珠电机手动开关，0=关闭，1=常转
#define r_StopAllDevice 0xFF          // 停止所有输出
#define r_SystemReset 0xF0            // 系统复位/进入Bootloader

/// 接收消息结构体
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

void Resend_Task(void);
void MesgDeal_Task(void);

void CommInit(void);
void CommTask(void);

/* 最近一次收到的控台版本号；未收到前为0。 */
extern uint32_t CtrlVersion;

#endif
