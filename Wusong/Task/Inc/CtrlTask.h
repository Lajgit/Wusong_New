#ifndef __CTRLTASK_H__
#define __CTRLTASK_H__

#include "port_device.h"

#define HoolleMotorTimeout_time 3000 // 吐珠/扭蛋电机超时时间
#define CardMotorTimeout_time 3000   // 卡片机超时时间
#define HoolleMotorReverse_Time 300  // 吐珠/扭蛋电机反转时间
#define HoolleMotorRetry_Times 3     // 吐珠/扭蛋电机重试次数
#define ValveTimeout_time 800        // 电子锁动作超时时间

#define HoolleMotor_Speed 80 // 吐珠电机速度80%
#define HoolleMotor2_Speed 80 // 第二路电机速度80%
#define HoolleMotor_Dir 0     // 电机方向

typedef struct
{
    motor_t Motor;
    volatile uint16_t Hoolle_num;
    volatile uint8_t RetryCount;
    volatile uint8_t ClearMode; // 清珠标志
    volatile uint8_t ManualRun; // 手动常转模式：1时忽略数量、超时和反转重试逻辑
} Motor_Hoolle;

typedef struct
{
    switch_t Switch;
    volatile uint16_t Card_num;
    volatile uint8_t RetryCount;
} Motor_Card;

typedef struct
{
    switch_t Switch;
    volatile uint8_t TriggerCount;
} Switch_Valve;

void Device_Init(void);
void CtrlTask(void);
void Hoolle_Output(Motor_Hoolle *Motor, uint16_t num);
void SteelBall_MotorSwitch(uint8_t enable);
void SteelBall_OutputEverySecond(void);
void Card_Output(Motor_Card *Switch, uint16_t num);

#endif
