#ifndef __LIGHTTASK_H__
#define __LIGHTTASK_H__

#include "port_lighteffect.h"
#include "port_light.h"

#define Light1_RGBbuffer_SIZE 5U
#define Light1_CRRbuffer_SIZE ((Light1_RGBbuffer_SIZE + 7U) * 24U)

void Light_Init(void);
void Light_Task(void);
void BallLight_SetMode(uint8_t led_id, uint8_t color_id, uint8_t mode);
void BallLight_SetBrightness(uint8_t brightness);

#endif
