#ifndef __MAINTASK_H__
#define __MAINTASK_H__

#include "port_event.h"


#define EncoderCheckTime 100
#define FlashData_SIZE 5

#define Event_SettingButtonPress (1u << 0)
#define Event_EncoderCheck (1u << 1)
#define Event_Encoder_L (1u << 2)
#define Event_Encoder_R (1u << 3)
#define Event_FlashData (1u << 4)
#define Event_ReadFlashData (1u << 5)
#define Event_SceneChange (1u << 6)
#define Event_DoorOpen (1u << 7)
#define Event_SaveSetting (1u << 8)

typedef enum
{
    SettingScene = 0,
    IdleScene = 1,
    PlayingScene = 2,
    LittleGame_1 = 3,
    LittleGame_2 = 4,
    /* 以下为原武松安卓场景编号，不能使用模板PlayingScene=2代替游玩场景6。 */
    SCENE_SETTING = 0,
    SCENE_IDLE = 1,
    SCENE_RANDOM = 2,
    SCENE_HOOLLE_INPUT = 3,
    SCENE_PUBLIC_RATIO = 4,
    SCENE_ADDITIONAL = 5,
    SCENE_PLAYING = 6,
    SCENE_VICTORY = 7,
    SCENE_DEFEAT = 8,
    SCENE_HAPPY1 = 9,
    SCENE_HAPPY2 = 10,
    SCENE_END = 11,
} Scene_t;

void System_Reset(void);
void Main_Init(void);
void Main_Task(void);

#endif
