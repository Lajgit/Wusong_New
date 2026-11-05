#include "MainTask.h"
#include "CommTask.h"
#include "InterruptTask.h"
#include "KeyTask.h"
#include "LightTask.h"
#include "DigitalTubeTask.h"
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
        /* 中文注释：球盘User_Led实际接PA15，每500ms翻转一次GPIO。 */
        HAL_GPIO_TogglePin(User_Led_GPIO_Port, User_Led_Pin);
        time = HAL_GetTick();
    }
}

void Main_Init(void)
{
    /* 中文注释：球盘只初始化主板通信、7路光眼、5颗RGB灯和两块外接数码管。 */
    CommInit();
    KeyAll_Init();
    Light_Init();
    DigitalTubeTask_Init();
}

void Main_Task(void)
{
    CommTask();
    HAL_IWDG_Refresh(&hiwdg);
    Key_Task();
    HAL_IWDG_Refresh(&hiwdg);
    Light_Task();
    HAL_IWDG_Refresh(&hiwdg);
    DigitalTube_Task();
    HAL_IWDG_Refresh(&hiwdg);
    // SystemLight_Task();
}
