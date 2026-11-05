#include "LightTask.h"
#include "port_light.h"
#include "MainTask.h"
#include "FlashTask.h"
#include "port_event.h"
#include "app_list.h"
#include "tim.h"

#define PINK_NOSE_START 32U
#define PINK_NOSE_END 35U

#define LIGHT1_IDLE_FLOW_COUNT Light1_RGBbufSize
#define LIGHT1_IDLE_FLOW_STEP_TIME 50U
#define LIGHT1_IDLE_FLOW_HOLD_TIME 1000U

RGB_t Light1_RGBbuf[Light1_RGBbufSize];
uint16_t Light1_CRRbuf[Light1_CRRbufSize];
RGB_t Light2_RGBbuf[Light2_RGBbufSize];
uint16_t Light2_CRRbuf[Light2_CRRbufSize];

Semaphore_t Semaphore1;
Semaphore_t Semaphore2;

uint8_t LightBoard_Lightness = 10;
uint8_t LightBelt_Lightness = 5;

extern Scene_t Scene;

Light_t Light1;
Light_t Light2;
Light_Handle_t Light_B1, Light_B2, Light_B3, Light_B4, Light_B5, Light_B6, Light_B7, Light_B8, Light_Y1, Light_Y2, Light_Y3, Light_Y4, Light_Y5, Light_Y6, Light_Y7, Light_Y8, Light_PLeft, Light_PRight;
Light_Handle_t *Light_BLUE[8] = {&Light_B1, &Light_B2, &Light_B3, &Light_B4, &Light_B5, &Light_B6, &Light_B7, &Light_B8};
Light_Handle_t *Light_YELLOW[8] = {&Light_Y1, &Light_Y2, &Light_Y3, &Light_Y4, &Light_Y5, &Light_Y6, &Light_Y7, &Light_Y8};

static BreathLight_t J2 = {&htim5, TIM_CHANNEL_1, 999, 0, GPIOA, GPIO_PIN_0, 0, 0, true};
static BreathLight_t J3 = {&htim5, TIM_CHANNEL_2, 999, 0, GPIOB, GPIO_PIN_1, 0, 0, true};
static BreathLight_t J6 = {&htim10, TIM_CHANNEL_1, 999, 0, GPIOB, GPIO_PIN_8, 0, 0, true};
static BreathLight_t J7 = {&htim11, TIM_CHANNEL_1, 999, 0, GPIOB, GPIO_PIN_9, 0, 0, true};

BreathLight_t *BreathList[] = {&J2, &J3, &J6, &J7};
uint8_t LightCache[8] = {0};

static uint8_t LightBlinkOn = 1U;
static uint32_t LightBlinkTick = 0U;

/* 中文注释：左右眼使用独立闪烁时基，避免一只眼的通信命令影响另一只眼 */
static uint8_t PinkLeftBlinkOn = 1U;
static uint32_t PinkLeftBlinkTick = 0U;
static uint8_t PinkLeftRefreshPending = 0U;
static uint8_t PinkRightBlinkOn = 1U;
static uint32_t PinkRightBlinkTick = 0U;
static uint8_t PinkRightRefreshPending = 0U;

static uint8_t IdleLight1FlowActive = 0U;
static uint8_t IdleLight1FlowHold = 0U;
static uint16_t IdleLight1FlowIndex = 0U;
static uint32_t IdleLight1FlowTick = 0U;
static uint32_t IdleLight1FlowHoldTick = 0U;

extern Event_Handle_t Event;
extern Setting_TypeDef Setting;
void LightTask_Init(void)
{
    RGB_Init(&Light1, &htim3, TIM_CHANNEL_1, Light1_RGBbufSize, Light1_RGBbuf, Light1_CRRbuf, &Semaphore1, RGB);
    RGB_Init(&Light2, &htim3, TIM_CHANNEL_2, Light2_RGBbufSize, Light2_RGBbuf, Light2_CRRbuf, &Semaphore2, RGB);
    BreathLight_Init(&J2, &htim5, TIM_CHANNEL_1, GPIOA, GPIO_PIN_0);
    BreathLight_Init(&J3, &htim5, TIM_CHANNEL_2, GPIOB, GPIO_PIN_1);
    BreathLight_Init(&J6, &htim10, TIM_CHANNEL_1, GPIOB, GPIO_PIN_8);
    BreathLight_Init(&J7, &htim11, TIM_CHANNEL_1, GPIOB, GPIO_PIN_9);
    RegisterLight(ColorLight, &Light1);
    RegisterLight(ColorLight, &Light2);
    RegisterLight(BreathLight, &J2);
    RegisterLight(BreathLight, &J3);
    RegisterLight(BreathLight, &J6);
    RegisterLight(BreathLight, &J7);
    LightDerive_Init(&Light_B1, &Light1, 0, 7, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B2, &Light1, 8, 15, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B3, &Light1, 16, 23, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B4, &Light1, 24, 31, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B5, &Light1, 36, 43, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B6, &Light1, 44, 49, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B7, &Light1, 50, 55, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_B8, &Light1, 56, 61, (uint8_t *)&Setting.Board_Lightness);
    /* 中文注释：右眼为62~63，左眼为64~65，鼻子32~35不建立可控灯对象 */
    LightDerive_Init(&Light_PRight, &Light1, 62, 63, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_PLeft, &Light1, 64, 65, (uint8_t *)&Setting.Board_Lightness);

    LightDerive_Init(&Light_Y1, &Light2, 0, 7, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y2, &Light2, 8, 15, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y3, &Light2, 16, 21, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y4, &Light2, 22, 27, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y5, &Light2, 28, 35, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y6, &Light2, 36, 42, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y7, &Light2, 43, 50, (uint8_t *)&Setting.Board_Lightness);
    LightDerive_Init(&Light_Y8, &Light2, 51, 58, (uint8_t *)&Setting.Board_Lightness);
    RGB_SetAllColor(&Light1, NONE, 0, 0);
    RGB_SetAllColor(&Light2, NONE, 0, 0);
    RGB_Flush(&Light1);
    RGB_Flush(&Light2);

    LightBlinkTick = HAL_GetTick();
    PinkLeftBlinkTick = LightBlinkTick;
    PinkRightBlinkTick = LightBlinkTick;
}

void PinkLight_SetState(uint8_t light_id, uint8_t state)
{
    Light_Handle_t *light = NULL;
    uint8_t *blink_on = NULL;
    uint32_t *blink_tick = NULL;
    uint8_t *refresh_pending = NULL;

    /* 中文注释：粉灯唯一运行时控制入口只接受通信定义的左右眼编号 */
    if (light_id == PINK_LIGHT_LEFT)
    {
        light = &Light_PLeft;
        blink_on = &PinkLeftBlinkOn;
        blink_tick = &PinkLeftBlinkTick;
        refresh_pending = &PinkLeftRefreshPending;
    }
    else if (light_id == PINK_LIGHT_RIGHT)
    {
        light = &Light_PRight;
        blink_on = &PinkRightBlinkOn;
        blink_tick = &PinkRightBlinkTick;
        refresh_pending = &PinkRightRefreshPending;
    }
    else
    {
        return;
    }

    /* 中文注释：粉灯不支持流水0x02，只处理关闭、打开和闪烁 */
    if (state != LIGHT_STATE_OFF &&
        state != LIGHT_STATE_ON &&
        state != LIGHT_STATE_BLINK)
    {
        return;
    }

    light->state = state;

    if (state == LIGHT_STATE_BLINK)
    {
        /* 中文注释：每只眼收到闪烁命令后独立从亮250ms开始计时 */
        *blink_on = 1U;
        *blink_tick = HAL_GetTick();
    }

    *refresh_pending = 1U;
}

static void PinkLightBufferFlush(Light_Handle_t *light, uint8_t blink_on)
{
    if (light->state == LIGHT_STATE_ON ||
        (light->state == LIGHT_STATE_BLINK && blink_on != 0U))
    {
        RGB_SetMoreColor(light->light, light->start, light->end, PINK, *(light->Lightness), 255);
    }
    else
    {
        RGB_SetMoreColor(light->light, light->start, light->end, NONE, *(light->Lightness), 0);
    }
}

static void PinkLight_Task(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t need_flush = 0U;

    /* 中文注释：左眼闪烁只更新左眼自己的相位和刷新标志 */
    if (Light_PLeft.state == LIGHT_STATE_BLINK &&
        (uint32_t)(now - PinkLeftBlinkTick) >= LIGHT_BLINK_HALF_TIME)
    {
        PinkLeftBlinkTick = now;
        PinkLeftBlinkOn = (PinkLeftBlinkOn == 0U) ? 1U : 0U;
        PinkLeftRefreshPending = 1U;
    }

    /* 中文注释：右眼闪烁只更新右眼自己的相位和刷新标志 */
    if (Light_PRight.state == LIGHT_STATE_BLINK &&
        (uint32_t)(now - PinkRightBlinkTick) >= LIGHT_BLINK_HALF_TIME)
    {
        PinkRightBlinkTick = now;
        PinkRightBlinkOn = (PinkRightBlinkOn == 0U) ? 1U : 0U;
        PinkRightRefreshPending = 1U;
    }

    if (PinkLeftRefreshPending != 0U)
    {
        PinkLightBufferFlush(&Light_PLeft, PinkLeftBlinkOn);
        PinkLeftRefreshPending = 0U;
        need_flush = 1U;
    }

    if (PinkRightRefreshPending != 0U)
    {
        PinkLightBufferFlush(&Light_PRight, PinkRightBlinkOn);
        PinkRightRefreshPending = 0U;
        need_flush = 1U;
    }

    if (need_flush != 0U)
    {
        /* 中文注释：鼻子32~35始终关闭；左右眼颜色只由各自协议状态决定 */
        RGB_SetMoreColor(&Light1, PINK_NOSE_START, PINK_NOSE_END, NONE, LightBoard_Lightness, 0);
        RGB_Flush(&Light1);
    }
}

static void IdleLight1Flow_Task(void)
{
    uint32_t now = HAL_GetTick();

    if (IdleLight1FlowActive == 0U)
    {
        IdleLight1FlowActive = 1U;
        IdleLight1FlowHold = 0U;
        IdleLight1FlowIndex = 0U;
        IdleLight1FlowTick = now;
        IdleLight1FlowHoldTick = now;

        /* 中文注释：待机流水只清蓝灯区域，跳过鼻子和左右眼 */
        RGB_SetMoreColor(&Light1, 0, 31, NONE, LightBoard_Lightness, 0);
        RGB_SetMoreColor(&Light1, 36, 61, NONE, LightBoard_Lightness, 0);
        RGB_Flush(&Light1);
        return;
    }

    if (IdleLight1FlowHold != 0U)
    {
        if ((uint32_t)(now - IdleLight1FlowHoldTick) >= LIGHT1_IDLE_FLOW_HOLD_TIME)
        {
            IdleLight1FlowHold = 0U;
            IdleLight1FlowIndex = 0U;
            IdleLight1FlowTick = now;
            RGB_SetMoreColor(&Light1, 0, 31, NONE, LightBoard_Lightness, 0);
            RGB_SetMoreColor(&Light1, 36, 61, NONE, LightBoard_Lightness, 0);
            RGB_Flush(&Light1);
        }
        return;
    }

    if ((uint32_t)(now - IdleLight1FlowTick) < LIGHT1_IDLE_FLOW_STEP_TIME)
        return;

    IdleLight1FlowTick = now;

    /* 中文注释：仍按原66个灯位计时，仅在粉灯位置跳过写入，保持原流水节奏 */
    if (IdleLight1FlowIndex <= 31U ||
        (IdleLight1FlowIndex >= 36U && IdleLight1FlowIndex <= 61U))
    {
        RGB_SetMoreColor(&Light1, IdleLight1FlowIndex, IdleLight1FlowIndex, SKYBLUE, LightBoard_Lightness, 255);
        RGB_Flush(&Light1);
    }

    IdleLight1FlowIndex++;
    if (IdleLight1FlowIndex >= LIGHT1_IDLE_FLOW_COUNT)
    {
        IdleLight1FlowHold = 1U;
        IdleLight1FlowHoldTick = now;
    }
}

/// 洞口灯光流水/开关/闪烁任务
static void LightBufferFlush(Light_Handle_t *light, RGB_t color)
{
    if (light->state == LIGHT_STATE_FLOW)
    {
        light->index++;
        if (light->index > light->end)
        {
            light->index = light->start;
            RGB_SetMoreColor(light->light, light->start, light->end, NONE, *(light->Lightness), 255);
        }
        else
            RGB_SetMoreColor(light->light, light->start, light->index, color, *(light->Lightness), 255);
    }
    else if (light->state == LIGHT_STATE_OFF)
        RGB_SetMoreColor(light->light, light->start, light->end, NONE, *(light->Lightness), 0);
    else if (light->state == LIGHT_STATE_ON)
        RGB_SetMoreColor(light->light, light->start, light->end, color, *(light->Lightness), 255);
    else if (light->state == LIGHT_STATE_BLINK)
    {
        /* 中文注释：蓝灯、黄灯0x03为500ms周期闪烁 */
        if (LightBlinkOn != 0U)
            RGB_SetMoreColor(light->light, light->start, light->end, color, *(light->Lightness), 255);
        else
            RGB_SetMoreColor(light->light, light->start, light->end, NONE, *(light->Lightness), 0);
    }
}

static void LightBlink_Task(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t has_blink = 0U;
    uint8_t refresh_light1 = 0U;
    uint8_t refresh_light2 = 0U;

    for (uint8_t i = 0; i < 8; i++)
    {
        if (Light_BLUE[i]->state == LIGHT_STATE_BLINK ||
            Light_YELLOW[i]->state == LIGHT_STATE_BLINK)
        {
            has_blink = 1U;
            break;
        }
    }

    if (has_blink == 0U)
    {
        LightBlinkOn = 1U;
        LightBlinkTick = now;
        return;
    }

    if ((uint32_t)(now - LightBlinkTick) < LIGHT_BLINK_HALF_TIME)
        return;

    LightBlinkTick = now;
    LightBlinkOn = (LightBlinkOn == 0U) ? 1U : 0U;

    for (uint8_t i = 0; i < 8; i++)
    {
        if (Light_BLUE[i]->state == LIGHT_STATE_BLINK)
        {
            LightBufferFlush(Light_BLUE[i], SKYBLUE);
            refresh_light1 = 1U;
        }
        if (Light_YELLOW[i]->state == LIGHT_STATE_BLINK)
        {
            LightBufferFlush(Light_YELLOW[i], GREEN);
            refresh_light2 = 1U;
        }
    }

    if (refresh_light1 != 0U)
        RGB_Flush(&Light1);
    if (refresh_light2 != 0U)
        RGB_Flush(&Light2);
}

/// 洞口灯光刷新任务
static void LightFlush_Task(void)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        LightBufferFlush(Light_BLUE[i], SKYBLUE);
        LightBufferFlush(Light_YELLOW[i], GREEN);
    }
    RGB_Flush(&Light1);
    RGB_Flush(&Light2);
}

/// 灯光任务
void LightTask(void)
{
    if (Scene != IdleScene)
        IdleLight1FlowActive = 0U;

    if (Scene != PlayingScene)
    {
        LightBlinkOn = 1U;
        LightBlinkTick = HAL_GetTick();
    }

    if (Scene == SettingScene)
    {
        /* 中文注释：设置场景只点亮蓝灯区域，跳过鼻子和左右眼 */
        LightEffect_Unblock_SetColor(&Light1, 0, 31, WHITE, LightBoard_Lightness, 255, true);
        LightEffect_Unblock_SetColor(&Light1, 36, 61, WHITE, LightBoard_Lightness, 255, true);
        BreathLight_SetLightKeep(&J2, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J3, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J6, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J7, 0, Setting.LightBelt_Lightness, 255);
        LightEffect_Unblock_SetColor(&Light2, 0, Light2_RGBbufSize, WHITE, LightBoard_Lightness, 255, true);
    }
    if (Scene == IdleScene)
    {
        BreathLight_SetLightKeep(&J2, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J3, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J6, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J7, 0, Setting.LightBelt_Lightness, 255);
        /* 中文注释：Light1待机流水跳过粉灯区域，Light2保持原黄灯流水 */
        IdleLight1Flow_Task();
        LightEffect_Unblock_Flow(&Light2, 0, Light2_RGBbufSize, NONE, GREEN, LightBoard_Lightness, 255, 50, 1000, 0);
    }
    if (Scene == PlayingScene)
    {
        static uint32_t time = 0;
        if (EventGroupCheckBits(&Event, Event_SceneChange) == true)
        {
            /* 中文注释：场景切换只清0~61，禁止改写左右眼62~65 */
            RGB_SetMoreColor(&Light1, 0, 61, NONE, 0, 0);
            RGB_SetAllColor(&Light2, NONE, 0, 0);
            RGB_Flush(&Light1);
            RGB_Flush(&Light2);
            //     // LightResume();
            EventGroupClearBits(&Event, Event_SceneChange);
        }
        LightBlink_Task();
        if (HAL_GetTick() - time > 100)
        {
            LightFlush_Task();
            time = HAL_GetTick();
        }
        BreathLight_SetLightKeep(&J2, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J3, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J6, 0, Setting.LightBelt_Lightness, 255);
        BreathLight_SetLightKeep(&J7, 0, Setting.LightBelt_Lightness, 255);
        // LightEffect_Unblock_SetColor(&Light1, 0, Light1_RGBbufSize, NONE, LightBoard_Lightness, 255, 0);
        // LightEffect_Unblock_SetColor(&Light2, 0, Light2_RGBbufSize, NONE, LightBoard_Lightness, 255, 0);
        // LightEffect_Unblock_Blink(&Light1, 36, 37, PINK, LightBoard_Lightness, 255, 250);
    }
    if (Scene == LittleGame_1)
    {
        if (EventGroupCheckBits(&Event, Event_SceneChange))
        {
            EventGroupClearBits(&Event, Event_SceneChange);
            /* 中文注释：小游戏场景只清0~61，禁止改写左右眼62~65 */
            RGB_SetMoreColor(&Light1, 0, 61, NONE, 0, 0);
            RGB_CleanAll(&Light2);
            RGB_Flush(&Light1);
            RGB_Flush(&Light2);
        }
    }
    if (Scene == LittleGame_2)
    {
        if (EventGroupCheckBits(&Event, Event_SceneChange))
        {
            EventGroupClearBits(&Event, Event_SceneChange);
            /* 中文注释：小游戏场景只清0~61，禁止改写左右眼62~65 */
            RGB_SetMoreColor(&Light1, 0, 61, NONE, 0, 0);
            RGB_CleanAll(&Light2);
            RGB_Flush(&Light1);
            RGB_Flush(&Light2);
        }
    }

    /* 中文注释：左右眼只根据通信设置的状态及其闪烁时基运行 */
    PinkLight_Task();
}