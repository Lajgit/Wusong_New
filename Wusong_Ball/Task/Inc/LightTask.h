#ifndef __LIGHTTASK_H__
#define __LIGHTTASK_H__
#include "main.h"
void Light_Init(void);
void Light_Task(void);
uint16_t BallLight_GetChannels(void); // 返回当前实际显示的微动位序掩码，供闭合快照读取
void BallLight_SetChannels(uint16_t mask);
#endif
