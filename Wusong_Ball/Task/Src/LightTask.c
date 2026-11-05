#include "LightTask.h"
#include "main.h"
#include "tim.h"
#include "string.h"

#define BALL_LED_COUNT 5U
#define BALL_COLOR_COUNT 9U

#define BALL_MODE_ON 0U
#define BALL_MODE_OFF 1U
#define BALL_MODE_BLINK 2U
#define BALL_MODE_BREATH 3U

#define BALL_LIGHT_REFRESH_INTERVAL 20U
#define BALL_LIGHT_BLINK_INTERVAL 500U
#define BALL_LIGHT_BREATH_PERIOD 2000U
/* PA6 对应 TIM3_CH1，使用 DMA1_Channel6；不恢复旧 PA7/TIM3_CH2 的无效 DMA 配置。 */
#define BALL_WS2812_CODE_0 24U
#define BALL_WS2812_CODE_1 63U
#define BALL_WS2812_RESET_SLOTS 256U
#define BALL_WS2812_PWM_SIZE (1U + BALL_LED_COUNT * 24U + BALL_WS2812_RESET_SLOTS)

/* 72MHz/90=800kHz，0/1 高电平分别约 0.333us/0.875us；尾部 256 个零占空比周期约 320us。 */
static uint16_t BallLightPwmBuffer[BALL_WS2812_PWM_SIZE];
static volatile uint8_t BallLightDmaBusy = 0U;
static volatile uint8_t BallLightDmaRetry = 0U;

static uint8_t BallLedColor[BALL_LED_COUNT] = {0U};
static uint8_t BallLedMode[BALL_LED_COUNT] = {
    BALL_MODE_OFF,
    BALL_MODE_OFF,
    BALL_MODE_OFF,
    BALL_MODE_OFF,
    BALL_MODE_OFF,
};
static uint8_t BallLightness = 5U;
static uint8_t BallLightDirty = 1U;

void Light_Init(void)
{
    memset(BallLightPwmBuffer, 0, sizeof(BallLightPwmBuffer));
    /* 上电先用零占空比保持低电平 1ms，再由任务发送五颗灯的完整全灭帧。 */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();
    HAL_Delay(1U);
    BallLightDirty = 1U;
}

void BallLight_SetMode(uint8_t led_id, uint8_t color, uint8_t mode)
{
    if (led_id == 0U || led_id > BALL_LED_COUNT)
    {
        return;
    }

    if (color >= BALL_COLOR_COUNT)
    {
        return;
    }

    if (mode > BALL_MODE_BREATH)
    {
        return;
    }

    BallLedColor[led_id - 1U] = color;
    BallLedMode[led_id - 1U] = mode;
    BallLightDirty = 1U;
}

void BallLight_SetBrightness(uint8_t brightness)
{
    if (brightness > 10U)
    {
        brightness = 10U;
    }

    BallLightness = brightness;
    BallLightDirty = 1U;
}

static uint8_t BallLight_GetRelativeLightness(uint8_t mode, uint32_t tick)
{
    uint32_t phase;

    switch (mode)
    {
        case BALL_MODE_ON:
            return 255U;

        case BALL_MODE_OFF:
            return 0U;

        case BALL_MODE_BLINK:
            return ((tick / BALL_LIGHT_BLINK_INTERVAL) % 2U) == 0U ? 255U : 0U;

        case BALL_MODE_BREATH:
            phase = tick % BALL_LIGHT_BREATH_PERIOD;
            if (phase < (BALL_LIGHT_BREATH_PERIOD / 2U))
            {
                return (uint8_t)((phase * 255U) / (BALL_LIGHT_BREATH_PERIOD / 2U));
            }
            return (uint8_t)(((BALL_LIGHT_BREATH_PERIOD - phase) * 255U) / (BALL_LIGHT_BREATH_PERIOD / 2U));

        default:
            return 0U;
    }
}

static uint8_t BallLight_ScaleColor(uint8_t value, uint8_t relative)
{
    uint32_t scaled;

    scaled = (uint32_t)value * relative * BallLightness;
    scaled /= (255U * 10U);
    if (scaled > 255U)
    {
        scaled = 255U;
    }

    return (uint8_t)scaled;
}

static uint16_t BallLight_DmaEncodeByte(uint16_t offset, uint8_t data)
{
    uint8_t mask;

    for (mask = 0x80U; mask != 0U; mask >>= 1U)
    {
        BallLightPwmBuffer[offset++] = (data & mask) != 0U ? BALL_WS2812_CODE_1 : BALL_WS2812_CODE_0;
    }
    return offset;
}

static void BallLight_DmaIdle(void)
{
    HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    htim3.Instance->EGR = TIM_EGR_UG;
    /* 完成/错误后恢复零占空比输出，空闲期间 PA6 保持低电平。 */
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
        Error_Handler();
    BallLightDmaBusy = 0U;
}

static bool BallLight_DmaFlush(uint32_t tick)
{
    uint8_t i;
    uint8_t relative;
    RGB_t color;
    uint16_t offset = 1U;

    /* DMA 空闲时才编码完整 GRB 帧，传输中不改缓冲区，串口中断始终保持开启。 */
    for (i = 0U; i < BALL_LED_COUNT; i++)
    {
        relative = BallLight_GetRelativeLightness(BallLedMode[i], tick);
        color = Color_table[BallLedColor[i]];

        offset = BallLight_DmaEncodeByte(offset, BallLight_ScaleColor(color.G, relative));
        offset = BallLight_DmaEncodeByte(offset, BallLight_ScaleColor(color.R, relative));
        offset = BallLight_DmaEncodeByte(offset, BallLight_ScaleColor(color.B, relative));
    }

    /* 首位及尾部保持零占空比；重新装载预装寄存器，避免上一帧 CCR 残值影响起始位。 */
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    htim3.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC1 | TIM_FLAG_UPDATE);
    BallLightDmaBusy = 1U;
    if (HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1,
                              (uint32_t *)BallLightPwmBuffer, BALL_WS2812_PWM_SIZE) != HAL_OK)
    {
        BallLight_DmaIdle();
        return false;
    }
    return true;
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim3 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        /* HAL 已确认 DMA 完成，尾部复位低电平也已发送，直接释放发送状态供下一帧使用。 */
        BallLight_DmaIdle();
    }
}

void HAL_TIM_ErrorCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim3 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        /* DMA 错误后解除忙状态，并让主循环重新发送，避免灯珠刷新永久停住。 */
        BallLightDmaRetry = 1U;
        BallLight_DmaIdle();
    }
}

void Light_Task(void)
{
    static uint32_t LastRefreshTick = 0U;
    uint32_t CurrentTick;
    uint8_t DynamicMode = 0U;
    uint8_t i;

    CurrentTick = HAL_GetTick();

    if (BallLightDmaBusy != 0U)
        return;
    if (BallLightDmaRetry != 0U)
    {
        BallLightDmaRetry = 0U;
        BallLightDirty = 1U;
    }

    for (i = 0U; i < BALL_LED_COUNT; i++)
    {
        if (BallLedMode[i] == BALL_MODE_BLINK || BallLedMode[i] == BALL_MODE_BREATH)
        {
            DynamicMode = 1U;
            break;
        }
    }

    if (BallLightDirty == 0U && DynamicMode == 0U)
    {
        return;
    }

    if (BallLightDirty == 0U && (CurrentTick - LastRefreshTick) < BALL_LIGHT_REFRESH_INTERVAL)
    {
        return;
    }

    if (BallLight_DmaFlush(CurrentTick) == false)
        return;
    BallLightDirty = 0U;
    LastRefreshTick = CurrentTick;
}
