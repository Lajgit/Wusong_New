#ifndef __DIGITAL_TUBE_TASK_H__
#define __DIGITAL_TUBE_TASK_H__

#include "main.h"
#include "spi.h"
#include "port_digitaltube.h"

extern DigitalTube_t DigitalTube;

/*
 * 数码管三位兼容标记：
 * 0 = 保持原四位显示逻辑；
 * 1 = 将0~999的显示值左移一位，例如123按1230写入，使原个位显示到十位位置。
 */
extern uint8_t DigitalTubeShiftEnable;

void DigitalTubeTask_Init(void);
void DigitalTube_SetDisplayNumber(uint32_t data);
void DigitalTube_Task(void);

#endif
