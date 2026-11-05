#ifndef __LIGHTTASK_H__
#define __LIGHTTASK_H__

#include "port_lighteffect.h"
#include "port_light.h"

#define Light1_RGBbufSize 66
#define Light2_RGBbufSize 59
#define Light1_CRRbufSize ((Light1_RGBbufSize + 7) * 24)
#define Light2_CRRbufSize ((Light2_RGBbufSize + 7) * 24)


#define LightPause_Time 500
#define VictoryTime 6000

/* 粉灯通信编号：鼻子不参与通信控制，始终关闭 */
#define PINK_LIGHT_LEFT  0x02U
#define PINK_LIGHT_RIGHT 0x03U

/* 闪烁周期500ms：亮250ms，灭250ms */
#define LIGHT_BLINK_HALF_TIME 250U

typedef enum
{
    LIGHT_STATE_OFF = 0,
    LIGHT_STATE_ON,
    LIGHT_STATE_FLOW,
    LIGHT_STATE_BLINK,
}Lightstate_t;

void LightTask_Init(void);
void LightTask(void);
void PinkLight_SetState(uint8_t light_id, uint8_t state);


#endif