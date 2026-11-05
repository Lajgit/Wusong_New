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

/* 兼容旧工程中仍参与编译但已退出运行路径的灯效模块，第二阶段删除对应工程项后再移除。 */
Scene_t Scene = IdleScene;
Event_Handle_t Event;

extern Tx_HandleTypeDef Tx3;

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

    /*
     * 控台上电初始化沿用小丑协议：
     * 0x00请求版本，0x02灯带亮度，0x03场景，0x04控台亮度。
     */
    Comm_SendMesg_FillData(&Tx3, Board_to_Ctrl, CTRL_CMD_GET_VERSION, 0x00, 0x00);

    if (Setting.LightBelt_Lightness <= CTRL_BRIGHTNESS_MAX)
        Comm_SendMesg_FillData(&Tx3,
                               Board_to_Ctrl,
                               CTRL_CMD_LIGHT_BELT_BRIGHTNESS,
                               Setting.LightBelt_Lightness,
                               0x00);

    Comm_SendMesg_FillData(&Tx3,
                           Board_to_Ctrl,
                           CTRL_CMD_LIGHT_SCENE,
                           0x00,
                           (uint8_t)Scene);

    if (Setting.Ctrl_Lightness <= CTRL_BRIGHTNESS_MAX)
        Comm_SendMesg_FillData(&Tx3,
                               Board_to_Ctrl,
                               CTRL_CMD_BRIGHTNESS,
                               Setting.Ctrl_Lightness,
                               0x00);
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
