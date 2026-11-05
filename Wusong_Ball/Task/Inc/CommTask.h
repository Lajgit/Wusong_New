#ifndef __COMM_TASK_H__
#define __COMM_TASK_H__

#include "stdint.h"
#include "port_communicate.h"

#define VERSION 20260430U

/* 中文注释：球盘只与主板通信。 */
#define Board_to_Ball 0x04U
#define Ball_to_Board 0x05U

#define BALL_CMD_VERSION 0x00U
#define BALL_CMD_DIGITAL_TUBE 0x01U
#define BALL_CMD_RGB_MODE 0x02U
#define BALL_CMD_BRIGHTNESS 0x04U

#define BALL_REPORT_VERSION 0x00U
#define BALL_REPORT_EYE 0x01U

#define BALL_EYE_TRIGGER 0x01U

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
void CommInit(void);
void CommTask(void);

#endif
