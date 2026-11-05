#include "MainTask.h"
#include "CommTask.h"
#include "CtrlTask.h"
#include "FlashTask.h"
#include "InterruptTask.h"
#include "KeyTask.h"
#include "MesgTask.h"
#include "port_event.h"
#include "DigitalTubeTask.h"
#include "iwdg.h"

#define SYSLIGHT_BLINK_TIME 500

/* 保留模板枚举供原工程编译，实际游戏场景按原武松SCENE_*编号处理。 */
Scene_t Scene = IdleScene;
Event_Handle_t Event;

extern Tx_HandleTypeDef Tx3;
extern Tx_HandleTypeDef Tx2;

void System_Reset(void)
{
    __disable_irq();
    HAL_NVIC_SystemReset();
}

static void SystemLight_Task(void)
{
    static uint32_t time = 0;
    if (HAL_GetTick() - time > SYSLIGHT_BLINK_TIME)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        time = HAL_GetTick();
    }
}

void Main_Init(void)
{
    FlashTask_Init();
    CommInit();
    Device_Init();
    HoolleInput_FilterInit();
    KeyAll_Init();

    /* 中文注释：主板本地数码管继续保留，控台数码管通过USART3同步接收显示数据。 */
    DigitalTubeTask_Init();

    /* 新板无RGB，查询从板版本并同步微动板初始场景。 */
    Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, CTRL_CMD_GET_VERSION, 0U, 0U);
    Comm_SendMesg_FillData(&Tx2, Board_to_Ball, BALL_CMD_VERSION, 0U, 0U);
    Ball_SendScene((uint8_t)Scene);

}

void Main_Task(void)
{
    CommTask();
    HAL_IWDG_Refresh(&hiwdg);
    FlashTask();
    HAL_IWDG_Refresh(&hiwdg);
    Key_Task();
    HAL_IWDG_Refresh(&hiwdg);
    /* 中文注释：旧弹界本地灯效任务已从主循环移除。 */
    CtrlTask();
    HAL_IWDG_Refresh(&hiwdg);
    Mesg_Task();
    HAL_IWDG_Refresh(&hiwdg);

    /* 中文注释：数码管为主板直连模块，保持本地刷新任务。 */
    DigitalTube_Task();
    SystemLight_Task();
}
