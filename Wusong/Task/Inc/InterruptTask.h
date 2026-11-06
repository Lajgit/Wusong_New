#ifndef __INTERRUPTTASK_H__
#define __INTERRUPTTASK_H__

/* 必须在Device_Init后调用：原子初始化滤波、清待报计数并开放输入处理。 */
void HoolleInput_FilterInit(void);
void HoolleInput_Scan1ms(void);

#endif