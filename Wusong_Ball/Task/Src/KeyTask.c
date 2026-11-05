#include "KeyTask.h"
#include "CommTask.h"
#include "main.h"
#include "LightTask.h"

#define SWITCH_COUNT 12U
#define SWITCH_SCAN_MS 1U
#define SWITCH_DEBOUNCE_MS 15U
#define HOLE_CONFIRM_MS 200U

/* 数组下标直接对应协议编号00~0B，物理接口依次为J1~J12。 */
static GPIO_TypeDef *SwitchPort[SWITCH_COUNT] = {
    GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA,
    GPIOA, GPIOA, GPIOA, GPIOB, GPIOB, GPIOB
};
static const uint16_t SwitchPin[SWITCH_COUNT] = {
    GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_8,
    GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_15, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5
};
static GPIO_PinState SwitchRaw[SWITCH_COUNT];
static GPIO_PinState SwitchStable[SWITCH_COUNT];
static uint32_t SwitchTick[SWITCH_COUNT];
static uint8_t SwitchWin[SWITCH_COUNT];
static uint32_t HoleTick[2];
static uint8_t HoleTracking[2];
static uint8_t HoleReported[2];
extern Tx_HandleTypeDef Tx;

void KeyAll_Init(void)
{
    for (uint8_t i = 0U; i < SWITCH_COUNT; i++)
    {
        SwitchRaw[i] = HAL_GPIO_ReadPin(SwitchPort[i], SwitchPin[i]);
        SwitchStable[i] = GPIO_PIN_SET;
        SwitchTick[i] = HAL_GetTick();
    }
}

void Key_Task(void)
{
    static uint32_t scan_tick = 0U;
    uint32_t now = HAL_GetTick();
    uint16_t diagnostic_mask = 0U;
    if ((uint32_t)(now - scan_tick) < SWITCH_SCAN_MS)
        return;
    scan_tick = now;
    for (uint8_t i = 0U; i < SWITCH_COUNT; i++)
    {
        GPIO_PinState state = HAL_GPIO_ReadPin(SwitchPort[i], SwitchPin[i]);
        if (state == GPIO_PIN_RESET)
            diagnostic_mask |= (uint16_t)(1U << i);
        if (state != SwitchRaw[i])
        {
            /* 在原始闭合边沿锁存灯态，消抖或串口传输期间推进流水灯不改变结果。 */
            if (state == GPIO_PIN_RESET)
                SwitchWin[i] = (BallLight_GetChannels() & (1U << i)) != 0U ? 1U : 0U;
            SwitchRaw[i] = state;
            SwitchTick[i] = now;
        }
        if (state != SwitchStable[i] && (uint32_t)(now - SwitchTick[i]) >= SWITCH_DEBOUNCE_MS)
        {
            SwitchStable[i] = state;
            /* 闭合只报一次，长时间压住不重复上报；松开消抖后才重新使能。 */
            if (state == GPIO_PIN_RESET)
                Comm_SendMesg_FillData(&Tx, Ball_to_Board, BALL_REPORT_SWITCH, i, SwitchWin[i]);
        }
    }
    /* 后台灯跟随实时触点；游戏上报仍走上方15ms消抖，且不覆盖主控游戏掩码。 */
    if (BallScene == BALL_SCENE_SETTING && BallLight_GetChannels() != diagnostic_mask)
        BallLight_SetChannels(diagnostic_mask);
    for (uint8_t i = 0U; i < 2U; i++)
    {
        if (HAL_GPIO_ReadPin(GPIOB, i == 0U ? Hole_Left_Pin : Hole_Right_Pin) == GPIO_PIN_SET)
        {
            if (HoleTracking[i] == 0U)
            {
                HoleTick[i] = now;
                HoleTracking[i] = 1U;
            }
            /* 洞口需连续有效200ms才上报，持续占用只报一次，解除后才重新使能。 */
            if (HoleReported[i] == 0U && (uint32_t)(now - HoleTick[i]) >= HOLE_CONFIRM_MS)
            {
                Comm_SendMesg_FillData(&Tx, Ball_to_Board, BALL_REPORT_HOLE, i, 0U);
                HoleReported[i] = 1U;
            }
        }
        else
        {
            HoleTracking[i] = 0U;
            HoleReported[i] = 0U;
        }
    }
}
