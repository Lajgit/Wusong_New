#include "MainTask.h"
#include "CommTask.h"
#include "InterruptTask.h"
#include "KeyTask.h"
#include "LightTask.h"
#include "port_event.h"
#include "iwdg.h"

#define SYSLIGHT_BLINK_TIME 500U

Event_Handle_t Event;

void System_Reset(void)
{
    __disable_irq();
    HAL_NVIC_SystemReset();
}

static void SystemLight_Task(void)
{
    static uint32_t time = 0U;

    if ((uint32_t)(HAL_GetTick() - time) > SYSLIGHT_BLINK_TIME)
    {
        /* 中文注释：微动板User_Led实际接PA1，每500ms翻转一次GPIO。 */
        HAL_GPIO_TogglePin(User_Led_GPIO_Port, User_Led_Pin);
        time = HAL_GetTick();
    }
}

void Main_Init(void)
{
    /* 微动板只运行串口、12路微动、2路洞口、通道灯和3路定时阀。 */
    CommInit();
    KeyAll_Init();
    Light_Init();

}

void Main_Task(void)
{
    CommTask();
    HAL_IWDG_Refresh(&hiwdg);
    Key_Task();
    HAL_IWDG_Refresh(&hiwdg);
    Light_Task();
    HAL_IWDG_Refresh(&hiwdg);
    BallValve_Task();
    HAL_IWDG_Refresh(&hiwdg);
    SystemLight_Task();
}
