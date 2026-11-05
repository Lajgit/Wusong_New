#include "CtrlTask.h"
#include "MesgTask.h"
#include "MainTask.h"
#include "KeyTask.h"
#include "CommTask.h"
#include "tim.h"
#include "port_device.h"
#include "port_event.h"
#include "string.h"

#define STEEL_BALL_OUTPUT_INTERVAL_MS 500U

Motor_Hoolle Motor_Hoolle1, Motor_Hoolle2;
Motor_Card Card;
servo_t Servo1, Servo2, Servo3;
Switch_Valve Lock_Valve;

extern Tx_HandleTypeDef Tx1;
extern Scene_t Scene;
extern Event_Handle_t Mesg_event;
extern Event_Handle_t Event;
static inline uint32_t Get_SysTime(void)
{
    return HAL_GetTick();
}

static void Ctrl_HoolleMotor(Motor_Hoolle *Motor, uint16_t speed, uint8_t dir, uint32_t timeout, uint32_t reverse_time, uint8_t retry_times, void (*Timeout_callbcak)(void))
{
    /*
     * 中文注释：手动常转模式独立于正常出货状态机。
     * 开启后不检查Hoolle_num，不进入超时、反转和重试逻辑，直到收到关闭命令。
     */
    if (Motor->ManualRun != 0U)
    {
        if (Motor->Motor.state != DEVICE_STATE_BUSY)
        {
            Motor->Motor.SetSpeed(&Motor->Motor, speed, dir);
            Motor->Motor.state = DEVICE_STATE_BUSY;
        }
        return;
    }

    // 开机吐珠电机
    if (Motor->Motor.state == DEVICE_STATE_START)
    {
        Motor->Motor.SetSpeed(&Motor->Motor, speed, dir);
        Motor->Motor.state = DEVICE_STATE_BUSY;
        // Motor->RetryCount = 0;
    }
    // 停止吐珠电机
    if (Motor->Motor.state == DEVICE_STATE_STOP)
    {
        Motor->Motor.Stop(&Motor->Motor);
        Motor->Motor.state = DEVICE_STATE_IDLE;
        Motor->Hoolle_num = 0;
        Motor->ClearMode = 0;
    }
    // 吐珠电机超时
    if (Motor->Motor.state == DEVICE_STATE_TIMEOUT)
    {
        // 反转时间到
        if (Motor->Motor.GetRuntime(&Motor->Motor) > HoolleMotorReverse_Time)
        {
            // 翻转次数不够，重新吐出
            if (Motor->RetryCount < retry_times)
            {
                Motor->Motor.state = DEVICE_STATE_START;
                Motor->RetryCount++;
            }
            else
            {
                Motor->Motor.state = DEVICE_STATE_IDLE;
                Motor->Motor.Stop(&Motor->Motor);

                /* 清珠空仓结束，清除0xFFFF剩余计数 */
                if (Motor->ClearMode != 0)
                {
                    Motor->Hoolle_num = 0;
                    Motor->ClearMode = 0;
                }

                // 超时停转后的反应
                if (Timeout_callbcak != NULL)
                    Timeout_callbcak();
            }
        }
    }
    // 吐珠电机暂停
    if (Motor->Motor.state == DEVICE_STATE_PAUSE)
    {
        Motor->Motor.LosePower(&Motor->Motor);
        Motor->Motor.ResetRuntime(&Motor->Motor);
    }
    // 吐珠电机超时
    if (Motor->Motor.GetRuntime(&Motor->Motor) > timeout && Motor->Motor.state != DEVICE_STATE_IDLE)
    {
        Motor->Motor.state = DEVICE_STATE_TIMEOUT;
        Motor->Motor.LosePower(&Motor->Motor);
        HAL_Delay(1);
        // 反转
        Motor->Motor.SetSpeed(&Motor->Motor, speed, !dir);
    }
}

static void Ctrl_CardMotor(Motor_Card *Card, uint32_t timeout, void (*Timeout_callbcak)(void))
{

    /*==============卡片机控制===============*/
    // 开机吐卡
    if (Card->Switch.state == DEVICE_STATE_START)
    {
        Card->Switch.on(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_BUSY;
    }
    // 停止吐卡
    if (Card->Switch.state == DEVICE_STATE_STOP)
    {
        Card->Switch.off(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_IDLE;
        Card->Card_num = 0;
    }
    // 吐卡超时
    if (Card->Switch.state == DEVICE_STATE_TIMEOUT)
    {
        Card->Switch.off(&Card->Switch);
        Card->Switch.state = DEVICE_STATE_IDLE;
        // 吐卡超时反应
        if (Timeout_callbcak != NULL)
            Timeout_callbcak();
    }
    // 吐卡超时判断
    if (Card->Switch.GetRuntime(&Card->Switch) > CardMotorTimeout_time && Card->Switch.state != DEVICE_STATE_IDLE)
    {
        Card->Switch.state = DEVICE_STATE_TIMEOUT;
    }
}

/*==============电磁阀控制===============*/
static void Ctrl_Valve(Switch_Valve *Valve, uint32_t timeout, void (*Timeout_callbcak)(void))
{
    // 电磁阀启动
    if (Valve->Switch.state == DEVICE_STATE_START)
    {
        Valve->Switch.on(&Valve->Switch);
        Valve->Switch.state = DEVICE_STATE_BUSY;
    }
    // 电磁阀停止
    if (Valve->Switch.state == DEVICE_STATE_STOP)
    {
        Valve->Switch.off(&Valve->Switch);
        Valve->Switch.state = DEVICE_STATE_IDLE;
    }
    // 电磁阀超时
    if (Valve->Switch.state == DEVICE_STATE_TIMEOUT)
    {
        Valve->Switch.state = DEVICE_STATE_IDLE;
        Valve->Switch.off(&Valve->Switch);
        if (Timeout_callbcak != NULL)
            Timeout_callbcak();
    }
    // 电磁阀超时判断
    if (Valve->Switch.GetRuntime(&Valve->Switch) > timeout && Valve->Switch.state != DEVICE_STATE_IDLE)
    {
        Valve->Switch.state = DEVICE_STATE_TIMEOUT;
    }
}

static void SteelBallMotorTimeout_callback(void)
{
    /* 中文注释：Motor_Hoolle1固定为钢珠通道，超时事件单独上报。 */
    EventGroupSetBits(&Mesg_event, MesgEvent_SteelBallOutputTimeout);
}

static void EggMotorTimeout_callback(void)
{
    /* 中文注释：Motor_Hoolle2固定为扭蛋通道，超时事件单独上报。 */
    EventGroupSetBits(&Mesg_event, MesgEvent_EggOutputTimeout);
}

static void CardMotorTimeout_callback(void)
{
    EventGroupSetBits(&Mesg_event, MesgEvent_CardOutputTimeout);
}

void Hoolle_Output(Motor_Hoolle *Motor, uint16_t num)
{
    /* 中文注释：手动常转期间忽略所有按数量出货命令，并保持自动出货状态清零。 */
    if (Motor->ManualRun != 0U)
    {
        Motor->Hoolle_num = 0U;
        Motor->RetryCount = 0U;
        Motor->ClearMode = 0U;
        return;
    }

    Motor->Hoolle_num += num;
    if (Motor->Hoolle_num != 0)
    {
        Motor->Motor.state = DEVICE_STATE_START;
        Motor->Motor.runtick = Get_SysTime();
        Motor->RetryCount = 0;
    }
}

/**
 * @brief 控制钢珠/吐珠电机手动常转
 * @param enable 0=立即关闭，1=开启并持续正转
 *
 * 中文注释：该模式专用于0x21电机开关协议。
 * 开启时清除原有数量、清空模式和重试状态，不受光眼计数和超时状态机影响；
 * 关闭时立即停止电机，并恢复为正常按数量出货模式。
 */
void SteelBall_MotorSwitch(uint8_t enable)
{
    if (enable != 0U)
    {
        Motor_Hoolle1.Hoolle_num = 0U;
        Motor_Hoolle1.RetryCount = 0U;
        Motor_Hoolle1.ClearMode = 0U;
        Motor_Hoolle1.ManualRun = 1U;

        /* 清除自动出货模式可能尚未处理的剩余数量和超时消息。 */
        EventGroupClearBits(
            &Mesg_event,
            MesgEvent_RemainingSteelBall | MesgEvent_SteelBallOutputTimeout);

        Motor_Hoolle1.Motor.SetSpeed(
            &Motor_Hoolle1.Motor,
            HoolleMotor_Speed,
            HoolleMotor_Dir);
        Motor_Hoolle1.Motor.state = DEVICE_STATE_BUSY;
        return;
    }

    Motor_Hoolle1.ManualRun = 0U;
    Motor_Hoolle1.Hoolle_num = 0U;
    Motor_Hoolle1.RetryCount = 0U;
    Motor_Hoolle1.ClearMode = 0U;
    Motor_Hoolle1.Motor.Stop(&Motor_Hoolle1.Motor);
    Motor_Hoolle1.Motor.state = DEVICE_STATE_IDLE;
}

void Card_Output(Motor_Card *Switch, uint16_t num)
{
    Switch->Card_num += num;
    if (Switch->Card_num != 0)
    {
        Switch->Switch.state = DEVICE_STATE_START;
        Switch->Switch.runtick = Get_SysTime();
    }
}

/**
 * @brief 非阻塞地按每秒一颗调度钢珠吐出
 *
 * 需要在主循环任务中持续调用。函数不使用HAL_Delay，
 * 上一颗钢珠尚未完成时不会累计新的吐珠数量。
 */
void SteelBall_OutputEverySecond(void)
{
    static uint32_t LastOutputTick = 0U;
    uint32_t CurrentTick = Get_SysTime();

    if ((uint32_t)(CurrentTick - LastOutputTick) <
        STEEL_BALL_OUTPUT_INTERVAL_MS)
    {
        return;
    }

    if (Motor_Hoolle1.ManualRun != 0U ||
        Motor_Hoolle1.Motor.state != DEVICE_STATE_IDLE ||
        Motor_Hoolle1.Hoolle_num != 0U)
    {
        return;
    }

    LastOutputTick = CurrentTick;
    Hoolle_Output(&Motor_Hoolle1, 1U);
}

/// 设备初始化
void Device_Init(void)
{
    Device_Motor_Init(&Motor_Hoolle1.Motor, &htim1, TIM_CHANNEL_1, &htim1, TIM_CHANNEL_2);
    Device_Motor_Init(&Motor_Hoolle2.Motor, &htim1, TIM_CHANNEL_3, &htim1, TIM_CHANNEL_4);
    Device_Switch_Init(&Card.Switch, CardOutput_GPIO_Port, CardOutput_Pin, GPIO_PIN_SET);

    /* 中文注释：新扭蛋机原理图电子锁控制脚为PA0，旧弹界PB1锁控已删除。 */
    Device_Switch_Init(&Lock_Valve.Switch, GPIOA, GPIO_PIN_0, GPIO_PIN_SET);

    /* 中文注释：按原理图编号：舵机1=PA1/TIM2_CH2，舵机2=PA2/TIM2_CH3，舵机3=PA3/TIM2_CH4。 */
    Device_Servo_Init(&Servo1, &htim2, TIM_CHANNEL_2, 45, 135, 90);
    Device_Servo_Init(&Servo2, &htim2, TIM_CHANNEL_3, 45, 135, 90);
    Device_Servo_Init(&Servo3, &htim2, TIM_CHANNEL_4, 45, 135, 90);
    HAL_TIM_Base_Start(&htim7);

    Motor_Hoolle1.Hoolle_num = 0;
    Motor_Hoolle2.Hoolle_num = 0;
    Motor_Hoolle1.RetryCount = 0;
    Motor_Hoolle2.RetryCount = 0;
    Motor_Hoolle1.ClearMode = 0;
    Motor_Hoolle2.ClearMode = 0;
    Motor_Hoolle1.ManualRun = 0;
    Motor_Hoolle2.ManualRun = 0;
    Card.Card_num = 0;
}

void Servo_AutoRun(servo_t *Servo, uint32_t time)
{
    static uint32_t Time = 0;
    static uint8_t dir = 0;
    uint32_t time_now = HAL_GetTick();

    if (time_now - Time >= time)
    {
        Time = time_now;

        if (dir == 0)
        {
            /* 向最大角度移动 */
            if (Servo->angle + 2 >= Servo->max_angle)
            {
                Servo->angle = Servo->max_angle;
                dir = 1;
            }
            else
            {
                Servo->angle += 2;
            }
        }
        else
        {
            /* 向最小角度移动 */
            if (Servo->angle <= Servo->min_angle + 2)
            {
                Servo->angle = Servo->min_angle;
                dir = 0;
            }
            else
            {
                Servo->angle -= 2;
            }
        }

        Servo->SetAngle(Servo, Servo->angle);
    }
}

void CtrlTask(void)
{
    // SteelBall_OutputEverySecond();//每秒吐一颗钢珠
    /*==============钢珠电机控制（Motor_Hoolle1）===============*/
    Ctrl_HoolleMotor(&Motor_Hoolle1, HoolleMotor_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times, SteelBallMotorTimeout_callback);
    /*==============扭蛋电机控制（Motor_Hoolle2）===============*/
    Ctrl_HoolleMotor(&Motor_Hoolle2, HoolleMotor2_Speed, HoolleMotor_Dir, HoolleMotorTimeout_time, HoolleMotorReverse_Time, HoolleMotorRetry_Times, EggMotorTimeout_callback);
    /*==============卡片机控制===============*/
    Ctrl_CardMotor(&Card, CardMotorTimeout_time, CardMotorTimeout_callback);
    /*==============电子锁控制===============*/
    Ctrl_Valve(&Lock_Valve, ValveTimeout_time, NULL);
    // Servo_AutoRun(&Servo1, 75);//舵机1自动摆动
}
