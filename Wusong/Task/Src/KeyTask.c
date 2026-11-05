#include "KeyTask.h"
#include "CommTask.h"
#include "port_key.h"
#include "MesgTask.h"
#include "CtrlTask.h"
extern Event_Handle_t Mesg_event;
extern Switch_Valve Lock_Valve;
static uint8_t SettingEntered = 0U; // K3持续长按期间只发送一次进入后台事件

#define SETTING_BUTTON_COUNT 3U
#define PLAY_BUTTON_COUNT 6U

/*
 * 中文注释：后台设置按键按原理图K1/K2/K3编号：
 * K1=PB6，K2=PB7，K3=PD0，对外协议编号依次为0x01~0x03。
 */
static GPIO_TypeDef *SettingButton_Port[SETTING_BUTTON_COUNT] = {
    KeyBoard3_GPIO_Port,
    KeyBoard2_GPIO_Port,
    KeyBoard1_GPIO_Port,
};
/* 中文注释：数组名避免与main.h中旧的SettingButton_Pin宏重名。 */
static uint32_t SettingButton_Pins[SETTING_BUTTON_COUNT] = {
    KeyBoard3_Pin,
    KeyBoard2_Pin,
    KeyBoard1_Pin,
};

/* 中文注释：外接游玩/拍拍按键1~6分别接PD8~PD13，协议功能码为0x03。 */
static GPIO_TypeDef *PlayButton_Port[PLAY_BUTTON_COUNT] = {
    PlayButton1_GPIO_Port,
    PlayButton2_GPIO_Port,
    PlayButton3_GPIO_Port,
    PlayButton4_GPIO_Port,
    PlayButton5_GPIO_Port,
    PlayButton6_GPIO_Port,
};
static uint32_t PlayButton_Pins[PLAY_BUTTON_COUNT] = {
    PlayButton1_Pin,
    PlayButton2_Pin,
    PlayButton3_Pin,
    PlayButton4_Pin,
    PlayButton5_Pin,
    PlayButton6_Pin,
};

static Key_HandleTypeDef setting_button[SETTING_BUTTON_COUNT];
static Key_HandleTypeDef *setting_button_list[SETTING_BUTTON_COUNT];
static Key_HandleTypeDef play_button[PLAY_BUTTON_COUNT];
static Key_HandleTypeDef *play_button_list[PLAY_BUTTON_COUNT];

extern Tx_HandleTypeDef Tx1;

/*==============================后台设置按键==============================*/
/* K1/K2继续上报设置按键，K3短按开锁；K3长按另行进入后台。 */
static void SettingButton_ShortCallback(uint16_t key_id)
{
    if (key_id < 2U)
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, key_id + 1U, KEY_EVENT_SHORT);
    else
    {
        Lock_Valve.Switch.state = DEVICE_STATE_START;
        EventGroupSetBits(&Mesg_event, MesgEvent_Unlock);
    }
}

static void SettingButton_LongCallback(uint16_t key_id)
{
    if (key_id < 2U)
        Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_SettingButton, key_id + 1U, KEY_EVENT_LONG);
    else if (SettingEntered == 0U)
    {
        EventGroupSetBits(&Mesg_event, MesgEvent_ButtonEnterSetting);
        SettingEntered = 1U;
    }
}

static void SettingButton_ReleaseCallback(uint16_t key_id)
{
    if (key_id == 2U)
        SettingEntered = 0U;
}

static void SettingButton_Init(void)
{
    Key_InitTypeDef init;

    for (uint16_t i = 0; i < SETTING_BUTTON_COUNT; i++)
    {
        init.short_callback = SettingButton_ShortCallback;
        init.long_callback = SettingButton_LongCallback;
        init.release_callback = SettingButton_ReleaseCallback;
        init.debounce_time = KEY_DEBOUNCE_TIME;
        /* 原武松K2长按阈值4秒，其余后台按键为1.5秒。 */
        init.longpress_time = (i == 1U) ? 4000U : 1500U;
        init.trigger_frequnecy = 1;
        init.trigger_level = GPIO_PIN_RESET;
        init.key_id = i;
        init.port = SettingButton_Port[i];
        init.pin = SettingButton_Pins[i];

        Key_Init(&setting_button[i], init);
        setting_button_list[i] = &setting_button[i];
    }
}

/*==============================外接游玩按键==============================*/
static void PlayButton_ShortCallback(uint16_t key_id)
{
    /* 中文注释：0x03=拍拍按键，Data4=编号，ExpandCode=短按。 */
    Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Button, key_id + 1U, KEY_EVENT_SHORT);
}

static void PlayButton_LongCallback(uint16_t key_id)
{
    /* 中文注释：ExpandCode=长按。 */
    Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Button, key_id + 1U, KEY_EVENT_LONG);
}

static void PlayButton_ReleaseCallback(uint16_t key_id)
{
    /* 中文注释：ExpandCode=松开。 */
    Comm_SendMesg_FillData(&Tx1, Board_to_Android, t_Button, key_id + 1U, KEY_EVENT_RELEASE);
}

static void PlayButton_Init(void)
{
    Key_InitTypeDef init;

    for (uint16_t i = 0; i < PLAY_BUTTON_COUNT; i++)
    {
        init.short_callback = PlayButton_ShortCallback;
        init.long_callback = PlayButton_LongCallback;
        init.release_callback = PlayButton_ReleaseCallback;
        init.debounce_time = KEY_DEBOUNCE_TIME;
        init.longpress_time = KEY_LONG_PRESS_TIME;
        init.trigger_frequnecy = 1;
        init.trigger_level = GPIO_PIN_RESET;
        init.key_id = i;
        init.port = PlayButton_Port[i];
        init.pin = PlayButton_Pins[i];

        Key_Init(&play_button[i], init);
        play_button_list[i] = &play_button[i];
    }
}

void KeyAll_Init(void)
{
    SettingButton_Init();
    PlayButton_Init();
}

void Key_Task(void)
{
    Key_Scan(setting_button_list, SETTING_BUTTON_COUNT);
    Key_Scan(play_button_list, PLAY_BUTTON_COUNT);
}
