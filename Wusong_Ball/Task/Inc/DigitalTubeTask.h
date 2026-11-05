#ifndef __DIGITAL_TUBE_TASK_H__
#define __DIGITAL_TUBE_TASK_H__

#include "main.h"
#include "port_digitaltube.h"

void DigitalTubeTask_Init(void);
void DigitalTube_Task(void);
void BallDigitalTube_Set(uint8_t side, uint8_t value);

#endif
