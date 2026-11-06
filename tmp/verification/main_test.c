/* ARM仿真回归：直接包含生产源码，硬件接口使用测试桩，不代表实物已验证。 */
#include "CommTask.h"
#include "CtrlTask.h"
#include "MainTask.h"
#include "FlashTask.h"
#include "DigitalTubeTask.h"
#include "MesgTask.h"
#include "usart.h"
#include "tim.h"
#include <string.h>

static uint32_t tick;
static GPIO_PinState gpio_level = GPIO_PIN_SET;
static uint32_t checks;
static uint32_t failed_line;
static uint8_t sent[256][14];
static UART_HandleTypeDef *sent_port[256];
static uint32_t sent_count;
static uint32_t resumed, displayed, resets;
static uint32_t delay_total, backup_denied, reset_magic, reset_tick;
static uint32_t motor_power_off, switch_power_off, reset_frames;
Motor_Hoolle Motor_Hoolle1, Motor_Hoolle2;
Motor_Card Card;
Switch_Valve Lock_Valve;
servo_t Servo1;
Event_Handle_t Mesg_event, Event;
Scene_t Scene;
Setting_TypeDef Setting;
UART_HandleTypeDef huart1, huart2, huart3;
TIM_HandleTypeDef htim7;

uint32_t HAL_GetTick(void) { return tick; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *b, uint16_t n) { return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, const uint8_t *b, uint16_t n, uint32_t timeout)
{
    if (sent_count < 256U && n == 14U)
    {
        memcpy(sent[sent_count], b, n);
        sent_port[sent_count++] = h;
    }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h) { return HAL_OK; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin) { return gpio_level; }
void Hoolle_Output(Motor_Hoolle *m, uint16_t n) { m->Hoolle_num += n; }
void Card_Output(Motor_Card *m, uint16_t n) { m->Card_num += n; }
void ResumeSetting(void) { resumed++; }
void System_Reset(void)
{
    resets++;
    reset_magic = RTC->BKP0R;
    reset_tick = tick;
    reset_frames = sent_count;
}
/* 仿真备份域访问成功/超时，并记录复位前的输出关闭和等待顺序。 */
void HAL_PWR_EnableBkUpAccess(void) { if (backup_denied == 0U) PWR->CR |= PWR_CR_DBP; }
void HAL_PWR_DisableBkUpAccess(void) { PWR->CR &= ~PWR_CR_DBP; }
void HAL_Delay(uint32_t n) { delay_total += n; tick += n; }
static void motor_off(void *self)
{
    if (self == &Motor_Hoolle1.Motor) motor_power_off |= 1U;
    if (self == &Motor_Hoolle2.Motor) motor_power_off |= 2U;
}
static void switch_off(void *self)
{
    if (self == &Card.Switch) switch_power_off |= 1U;
    if (self == &Lock_Valve.Switch) switch_power_off |= 2U;
}
void DigitalTube_SetDisplayNumber(uint32_t n) { displayed = n; }
void SteelBall_MotorSwitch(uint8_t on) { Motor_Hoolle1.ManualRun = on; }

#include "../../Wusong/Task/Src/CommTask.c"
#include "../../Wusong/Task/Src/InterruptTask.c"
#include "../../Wusong/Port/Src/port_communicate.c"
#include "../../Wusong/Port/Src/port_event.c"
#include "../../Wusong/Lib/Src/app_ringbuf.c"
#include "../../Wusong/Lib/Src/app_list.c"
#include "../../Wusong/Lib/Src/app_crc.c"

#define CHECK(x) do { checks++; if (!(x)) { failed_line = __LINE__; goto finish; } } while (0)
static void seal(uint8_t *frame)
{
    uint16_t crc = CRC16_calculate(frame, 11U);
    frame[11] = (uint8_t)(crc >> 8); frame[12] = (uint8_t)crc;
}
static void feed(Rx_HandleTypeDef *rx, uint8_t *frame, uint8_t n)
{
    for (uint8_t i = 0U; i < n; i++)
        rx->Handle.RingBuf.f_WriteByte(&rx->Handle.RingBuf, frame[i]);
    CommTask();
}
static void request(Rx_HandleTypeDef *rx, uint8_t dir, uint8_t code, uint32_t value, uint8_t expand, uint8_t ack, uint8_t id)
{
    uint8_t f[14] = {0xAA,0,0,0,0,0,0,0,0,0,0,0,0,0x55};
    f[2]=id; f[3]=dir; f[4]=code; f[5]=value>>24; f[6]=value>>16;
    f[7]=value>>8; f[8]=value; f[9]=ack; f[10]=expand; seal(f); feed(rx,f,14U);
}

uint32_t test_main(void)
{
    uint8_t f[14], saved[14];
    uint32_t before;
    /* 第二路光眼入口已停用：不得改写第一路计时或产生计数/上报。 */
    htim7.Instance = TIM7;
    TIM7->CNT = 123U;
    Motor_Hoolle2.Hoolle_num = 2U;
    Motor_Hoolle2.Motor.state = DEVICE_STATE_BUSY;
    HAL_GPIO_EXTI_Callback(HoolleOutput_2_Pin);
    CHECK(TIM7->CNT == 123U);
    CHECK(Motor_Hoolle2.Hoolle_num == 2U && Motor_Hoolle2.Motor.state == DEVICE_STATE_BUSY);
    CHECK(EventGroupCheckBits(&Mesg_event, MesgEvent_RemainingSteelBall2 | MesgEvent_SteelBall2OutputTimeout) == false);
    gpio_level = GPIO_PIN_RESET;
    HAL_GPIO_EXTI_Callback(HoolleOutput_2_Pin);
    CHECK(TIM7->CNT == 123U);
    CHECK(Motor_Hoolle2.Hoolle_num == 2U && Motor_Hoolle2.Motor.state == DEVICE_STATE_BUSY);
    gpio_level = GPIO_PIN_SET;
    Motor_Hoolle2.Hoolle_num = 0U;
    Motor_Hoolle2.Motor.state = DEVICE_STATE_IDLE;
    CommInit();
    request(&Rx3,3U,0U,0xAA55AA55U,0U,0U,1U);
    CHECK(CtrlVersion == 0xAA55AA55U);
    request(&Rx3,3U,1U,2U,1U,0U,2U);
    CHECK(sent_count == 1U && sent_port[0] == &huart1 && sent[0][4] == 3U && sent[0][8] == 2U);
    before = sent_count;
    request(&Rx3,3U,2U,1U,1U,0U,3U);
    request(&Rx3,3U,3U,0U,1U,0U,4U);
    request(&Rx3,3U,4U,8U,2U,0U,5U);
    CHECK(sent_count == before && CtrlSpecialButton.Data4 == 1U && CtrlEncoder.ExpandCode == 1U && CtrlJoystick.Data4 == 8U);
    request(&Rx1,1U,1U,3U,0U,0U,6U);
    CHECK(Motor_Hoolle1.Hoolle_num == 3U && Motor_Hoolle2.Hoolle_num == 0U);
    /* 旧模板的Expand=1也只按武松第一路出珠处理，不启动第二路。 */
    request(&Rx1,1U,1U,0U,1U,0U,90U);
    CHECK(Motor_Hoolle1.Hoolle_num == 3U && Motor_Hoolle2.Hoolle_num == 0U);
    request(&Rx1,1U,0x11U,0U,0U,0U,7U);
    CHECK(resumed == 1U);
    request(&Rx1,1U,0x21U,0x01010100U,0U,0U,80U);
    CHECK(FlowCount==1U && FlowStarted==0U);
    request(&Rx1,1U,7U,6U,0U,0U,8U);
    request(&Rx1,1U,0x21U,0x01010200U,0U,0U,9U);
    CHECK(Scene == SCENE_PLAYING && FlowCount == 2U && ChannelMask == 3U && Motor_Hoolle1.ManualRun == 0U);
    CHECK(sent_port[sent_count-1U] == &huart2 && sent[sent_count-1U][4] == 1U && sent[sent_count-1U][8] == 3U);
    tick=100U; CommTask();
    CHECK(ChannelMask == 6U);
    before=sent_count;
    ChannelMask=0U; request(&Rx2,5U,1U,1U,1U,1U,10U);
    CHECK(sent_count == before+3U && sent_port[before] == &huart2 && sent[before][3] == 5U);
    CHECK(sent[before+1U][4] == 0x16U && sent[before+2U][4] == 0x0EU && sent[before+2U][8] == 1U);
    memcpy(f,sent[before],14U); f[1]=1U; seal(f);
    before=sent_count; feed(&Rx2,f,14U);
    CHECK(sent_count == before+1U);
    before=sent_count;
    request(&Rx2,5U,1U,11U,0U,1U,11U);
    CHECK(sent[before+1U][4] == 0x17U && sent[before+2U][8] == 11U);
    before=sent_count;
    request(&Rx1,1U,3U,0U,0U,1U,12U);
    CHECK(sent_count == before+2U && sent[before][3] == 1U && sent[before+1U][3] == 4U && sent[before+1U][9] == 1U);
    memcpy(saved,sent[before+1U],14U);
    before=ResendList.NodeCount;
    feed(&Rx2,saved,14U);
    CHECK(ResendList.NodeCount == before-1U);
    request(&Rx1,1U,0x16U,0x123455AAU,0U,0U,13U);
    CHECK(displayed == 0x123455AAU && sent_port[sent_count-1U] == &huart3);
    before=sent_count;
    memcpy(f,saved,14U); f[3]=1U; f[4]=1U; f[12]^=1U;
    feed(&Rx1,f,14U);
    CHECK(sent_count == before && Motor_Hoolle1.Hoolle_num == 3U);
    /* 截断帧之后紧接合法帧，接收器必须能重新同步。 */
    f[0]=0xAA; f[2]=99U; f[3]=3U; f[4]=0U; f[5]=0x12;f[6]=0x34;f[7]=0x56;f[8]=0x78;f[9]=0;f[10]=0;f[13]=0x55;seal(f);
    feed(&Rx3,f,7U); feed(&Rx3,f,14U);
    CHECK(CtrlVersion == 0x12345678U);
    /* 控台按键06只触发防摇，不再作为安卓普通游玩按键6转发。 */
    FlowCount=0U; before=sent_count; tick=200U;
    request(&Rx3,3U,1U,6U,1U,0U,14U);
    CHECK(sent_count == before+1U && sent[before][4] == 0x18U && sent[before][8] == 0U);
    tick=300U; before=sent_count;
    request(&Rx3,3U,1U,6U,2U,0U,15U);
    CHECK(AntiShakeActive == 1U && AntiShakeTick == 300U);
    tick=5299U; CommTask(); CHECK(AntiShakeActive == 1U);
    tick=5300U; before=sent_count; CommTask();
    CHECK(AntiShakeActive == 0U && sent[before][4] == 0x18U && sent[before][8] == 1U);
    FlowCount=0U;
    List_Clear(&ResendList);
    before=sent_count;
    Ball_SendValve(BALL_CMD_VALVE,0U);
    tick+=1000U; Resend_Task();
    CHECK(sent_port[before+1U] == &huart2 && sent[before+1U][1] == 1U && sent[before+1U][3] == 4U);
    tick+=1000U; Resend_Task(); tick+=1000U; Resend_Task();
    CHECK(ResendList.NodeCount == 0U && sent[before+3U][1] == 3U);
    before=sent_count; tick+=1000U; Resend_Task(); CHECK(sent_count == before);
    before=sent_count;
    CommTransmitFillData(&Tx1,Board_to_Android,t_Switch,0U,0U);
    Ball_SendValve(BALL_CMD_VALVE,0U);
    tick+=1000U;Resend_Task();
    CHECK(sent_port[before+2U]==&huart1 && sent_port[before+3U]==&huart2);
    memcpy(saved,sent[before+3U],14U);feed(&Rx1,saved,14U);CHECK(ResendList.NodeCount==2U);
    feed(&Rx2,saved,14U);CHECK(ResendList.NodeCount==1U);
    tick+=1000U;Resend_Task();tick+=1000U;Resend_Task();CHECK(ResendList.NodeCount==0U);
    Ball_SendValve(BALL_CMD_HOLE_VALVE,1U);
    request(&Rx1,1U,0xFFU,0U,0U,0U,16U);
    CHECK(ResendList.NodeCount == 1U && MesgTable[ResendList.Head->ID].Code2 == BALL_CMD_STOP);
    List_Clear(&ResendList);
    before=sent_count;
    request(&Rx1,1U,0x0CU,0U,0U,0U,81U);
    CHECK(sent_count==before+1U && sent_port[before]==&huart2 && sent[before][4]==3U && sent[before][8]==0U && sent[before][10]==1U && sent[before][9]==1U);
    before=sent_count;
    request(&Rx1,1U,0x0FU,0U,0U,0U,82U);
    CHECK(sent_count==before+2U && sent[before][8]==0U && sent[before+1U][8]==1U && sent[before][10]==0U && sent[before+1U][10]==0U);
    List_Clear(&ResendList);
    request(&Rx1,1U,7U,0U,0U,0U,83U);
    CHECK(Scene==SCENE_SETTING && sent[sent_count-1U][4]==BALL_CMD_SCENE && sent[sent_count-1U][8]==0U && sent[sent_count-1U][9]==1U);
    request(&Rx1,1U,7U,6U,0U,0U,84U);
    CHECK(ResendList.NodeCount==1U && MesgTable[ResendList.Head->ID].Code2==BALL_CMD_SCENE && MesgTable[ResendList.Head->ID].Data4==6U);
    before=sent_count;request(&Rx1,1U,7U,12U,0U,0U,85U);
    CHECK(Scene==SCENE_PLAYING && sent_count==before);
    request(&Rx1,1U,7U,1U,0U,0U,87U);
    before=sent_count;request(&Rx2,5U,2U,0U,0U,1U,86U);
    CHECK(sent_count==before+3U && sent[before+1U][4]==t_Hole && sent_port[before+2U]==&huart2 && sent[before+2U][4]==3U && sent[before+2U][10]==1U);
    /* 普通F0和错误升级标记仍只执行原复位；备份域失败不得误复位。 */
    request(&Rx1,1U,0xF0U,0U,0U,1U,88U);
    CHECK(resets==1U && reset_magic==0U && delay_total==0U);
    request(&Rx1,1U,0xF0U,0x424F5440U,0U,0U,89U);
    CHECK(resets==2U && RTC->BKP0R==0U && motor_power_off==0U && switch_power_off==0U);
    backup_denied=1U; before=sent_count;
    request(&Rx1,1U,0xF0U,OTA_REQUEST_MAGIC,0U,1U,90U);
    CHECK(resets==2U && RTC->BKP0R==0U && delay_total==0U);
    CHECK(sent_count==before+1U && sent[before][3]==1U && sent[before][4]==0xF0U);
    CHECK((PWR->CR & PWR_CR_DBP)==0U && motor_power_off==0U && switch_power_off==0U);
    /* 从手动常转、流水灯和机构工作状态发起升级，验证实际关闭后才复位。 */
    backup_denied=0U;
    Motor_Hoolle1.Motor.LosePower=motor_off;
    Motor_Hoolle2.Motor.LosePower=motor_off;
    Card.Switch.off=switch_off; Lock_Valve.Switch.off=switch_off;
    Motor_Hoolle1.ManualRun=1U;
    Motor_Hoolle2.Motor.state=DEVICE_STATE_BUSY;
    Card.Switch.state=DEVICE_STATE_BUSY; Lock_Valve.Switch.state=DEVICE_STATE_BUSY;
    FlowCount=2U; FlowStarted=1U;
    before=sent_count; tick=20000U;
    request(&Rx1,1U,0xF0U,OTA_REQUEST_MAGIC,0U,1U,91U);
    CHECK(resets==3U && reset_magic==OTA_REQUEST_MAGIC && RTC->BKP0R==OTA_REQUEST_MAGIC);
    CHECK(delay_total==100U && reset_tick==20100U && reset_frames==sent_count);
    CHECK(motor_power_off==3U && switch_power_off==3U && Motor_Hoolle1.ManualRun==0U);
    CHECK(Motor_Hoolle2.Motor.state==DEVICE_STATE_IDLE && Card.Switch.state==DEVICE_STATE_IDLE && Lock_Valve.Switch.state==DEVICE_STATE_IDLE);
    CHECK(FlowCount==0U && ChannelMask==0U && (PWR->CR & PWR_CR_DBP)==0U);
    CHECK(sent_count==before+3U && sent_port[before]==&huart1 && sent[before][4]==0xF0U);
    CHECK(sent_port[before+1U]==&huart2 && sent[before+1U][4]==BALL_CMD_CHANNEL_LIGHT && sent[before+1U][8]==0U);
    CHECK(sent_port[before+2U]==&huart2 && sent[before+2U][3]==Board_to_Ball && sent[before+2U][4]==BALL_CMD_STOP && sent[before+2U][9]==1U);
    /* 错误CRC及错误方向均不得触发升级，也不得写备份标记。 */
    RTC->BKP0R=0U;
    memcpy(f,sent[before],14U); f[11]^=1U;
    feed(&Rx1,f,14U);
    CHECK(resets==3U && RTC->BKP0R==0U);
    request(&Rx1,Board_to_Android,0xF0U,OTA_REQUEST_MAGIC,0U,0U,92U);
    CHECK(resets==3U && RTC->BKP0R==0U);
    for (uint32_t i=0; i<sent_count; i++)
        CHECK(CRC16_calculate(sent[i],11U) == (((uint16_t)sent[i][11]<<8)|sent[i][12]));
finish:
    __asm { BKPT 0 }
    return failed_line;
}
