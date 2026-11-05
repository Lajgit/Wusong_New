#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"

#define VERSION 20260430U

/* 中文注释：球盘只与主板通信。 */
#define Board_to_Ball 0x04U
#define Ball_to_Board 0x05U

#define BALL_CMD_VERSION 0x00U
#define BALL_CMD_CHANNEL_LIGHT 0x01U
#define BALL_CMD_VALVE 0x02U
#define BALL_CMD_HOLE_VALVE 0x03U // 补充位0按左右直接推出，补充位1且Data4=0条件成对清洞
#define BALL_CMD_SCENE 0x07U // Data4=0为后台，退出后台恢复游戏掩码
#define BALL_CMD_STOP 0xFFU
#define BALL_REPORT_VERSION 0x00U
#define BALL_REPORT_SWITCH 0x01U // Data4=J1~J12编号00~0B，补充位0/1为未中奖/中奖
#define BALL_REPORT_HOLE 0x02U

#define BALL_SCENE_SETTING 0x00U
#define BALL_SCENE_IDLE 0x01U
#define BALL_SCENE_END 0x0BU

typedef struct
{
    uint8_t Head;
    uint8_t ResendID;
    uint8_t ID;
    uint8_t Code1;
    uint8_t Code2;
    uint8_t Data1;
    uint8_t Data2;
    uint8_t Data3;
    uint8_t Data4;
    uint8_t ACKbyte;
    uint8_t ExpandCode;
    uint8_t CRC16_H;
    uint8_t CRC16_L;
    uint8_t Tail;
} Mesg_TypeDef;

uint8_t Comm_SendMesg_FillData(Tx_HandleTypeDef *Tx, uint8_t code_1, uint8_t code_2, uint32_t data, uint8_t expandCode);
void BallValve_Task(void);
void CommInit(void);
void CommTask(void);
extern uint8_t BallScene;

#endif
