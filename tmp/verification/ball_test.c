/* ARM仿真回归：直接包含生产源码，硬件接口使用测试桩，不代表实物已验证。 */
#include "CommTask.h"
#include "LightTask.h"
#include "KeyTask.h"
#include "usart.h"
#include <string.h>
static uint32_t tick, checks, failed_line;
static uint8_t sent[256][14];
static uint32_t sent_count;
static uint16_t input_a=0xFFFFU,input_b=0xFFF9U,output_b;
static uint16_t shift,latched;
UART_HandleTypeDef huart1;
uint32_t HAL_GetTick(void) { return tick; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h,uint8_t *b,uint16_t n) { return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h,const uint8_t *b,uint16_t n,uint32_t timeout)
{
    if (sent_count<256U && n==14U) memcpy(sent[sent_count++],b,n);
    return HAL_OK;
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port,uint16_t pin)
{
    return ((port==GPIOA?input_a:input_b)&pin)!=0U?GPIO_PIN_SET:GPIO_PIN_RESET;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port,uint16_t pin,GPIO_PinState state)
{
    uint16_t before=output_b;
    if (port!=GPIOB) return;
    if (state==GPIO_PIN_SET) output_b|=pin; else output_b&=~pin;
    if ((before&Channel_CLK_Pin)==0U && (output_b&Channel_CLK_Pin)!=0U)
        shift=(uint16_t)((shift<<1)|((output_b&Channel_SDI_Pin)!=0U?1U:0U));
    if ((before&Channel_LE_Pin)==0U && (output_b&Channel_LE_Pin)!=0U) latched=shift;
}
#include "../../Wusong_Ball/Task/Src/CommTask.c"
#include "../../Wusong_Ball/Task/Src/LightTask.c"
#include "../../Wusong_Ball/Task/Src/KeyTask.c"
#include "../../Wusong_Ball/Mid/Src/port_communicate.c"
#include "../../Wusong_Ball/Lib/Src/app_ringbuf.c"
#include "../../Wusong_Ball/Lib/Src/app_list.c"
#include "../../Wusong_Ball/Lib/Src/app_crc.c"
#define CHECK(x) do { checks++; if (!(x)) { failed_line=__LINE__; goto finish; } } while(0)
static void seal(uint8_t *f) { uint16_t crc=CRC16_calculate(f,11U);f[11]=crc>>8;f[12]=crc; }
static void feed(uint8_t *f,uint8_t n)
{
    for(uint8_t i=0;i<n;i++) Rx.Handle.RingBuf.f_WriteByte(&Rx.Handle.RingBuf,f[i]);
    CommTask();
}
uint32_t test_main(void)
{
    uint8_t f[14]={0xAA,0,10,4,1,0,0,0,1,0,0,0,0,0x55};
    uint8_t echo[14],probe_storage[4],probe_byte;uint32_t before;
    RingBuffer_t probe;
    RingBuffer_Init(&probe,probe_storage,sizeof(probe_storage));
    for(uint8_t i=0U;i<4U;i++) CHECK(probe.f_WriteByte(&probe,i));
    CHECK(!probe.f_WriteByte(&probe,9U) && probe.f_GetDatalen(&probe)==4U && !probe.f_IsEmpty(&probe));
    for(uint8_t i=0U;i<4U;i++) CHECK(probe.f_ReadByte(&probe,&probe_byte) && probe_byte==i);
    CHECK(probe.f_IsEmpty(&probe) && !probe.f_ReadByte(&probe,&probe_byte));
    CommInit();KeyAll_Init();Light_Init(); CHECK(latched==0U);
    seal(f);feed(f,14U);CHECK(latched==0x0800U);
    f[7]=8U;f[8]=0U;seal(f);feed(f,14U);CHECK(latched==1U);
    f[7]=0xFFU;f[8]=0xFFU;seal(f);feed(f,14U);CHECK(latched==0x0FFFU);
    f[7]=0U;f[8]=1U;f[12]^=1U;feed(f,14U);CHECK(latched==0x0FFFU);
    f[4]=2U;f[7]=0U;f[8]=0U;f[9]=1U;seal(f);before=sent_count;feed(f,14U);
    CHECK((output_b&Valve_Block_Pin)!=0U && sent_count==before+1U);
    tick=799U;BallValve_Task();CHECK((output_b&Valve_Block_Pin)!=0U);
    tick=800U;BallValve_Task();CHECK((output_b&Valve_Block_Pin)==0U);
    f[1]=1U;seal(f);feed(f,14U);CHECK((output_b&Valve_Block_Pin)==0U);
    f[1]=0U;f[2]=11U;f[4]=3U;f[8]=0U;seal(f);feed(f,14U);CHECK((output_b&Valve_Left_Pin)!=0U);
    f[2]=12U;f[8]=1U;seal(f);feed(f,14U);CHECK((output_b&Valve_Right_Pin)!=0U);
    f[2]=13U;f[8]=2U;seal(f);feed(f,14U);CHECK(ValveActive[2]==0U);
    f[2]=14U;f[4]=0xFFU;seal(f);feed(f,14U);
    CHECK((output_b&(Valve_Left_Pin|Valve_Right_Pin|Valve_Block_Pin))==0U);
    /* 左右洞保留原武松的800ms吸合、300ms间隔、800ms再吸合时序。 */
    f[2]=15U;f[4]=3U;f[8]=0U;seal(f);feed(f,14U);
    tick=1600U;BallValve_Task();CHECK((output_b&Valve_Left_Pin)==0U && ValveActive[0]==2U);
    tick=1899U;BallValve_Task();CHECK((output_b&Valve_Left_Pin)==0U);
    tick=1900U;BallValve_Task();CHECK((output_b&Valve_Left_Pin)!=0U);
    tick=2700U;BallValve_Task();CHECK((output_b&Valve_Left_Pin)==0U && ValveActive[0]==0U);
    tick=800U;
    /* 验证触点抖动、持续闭合不重复上报，以及J1/J12两个编号端点。 */
    before=sent_count;input_a&=~GPIO_PIN_3;tick=801U;Key_Task();tick=810U;input_a|=GPIO_PIN_3;Key_Task();tick=830U;Key_Task();
    CHECK(sent_count==before);
    tick=831U;input_a&=~GPIO_PIN_3;Key_Task();BallLight_SetChannels(0U);tick=846U;Key_Task();
    CHECK(sent_count==before+1U && sent[before][4]==1U && sent[before][8]==0U && sent[before][9]==1U && sent[before][10]==1U);
    tick=900U;Key_Task();CHECK(sent_count==before+1U);
    memcpy(echo,sent[before],14U);feed(echo,14U);CHECK(ResendList.NodeCount==0U);
    input_b&=~GPIO_PIN_5;tick=901U;Key_Task();tick=916U;Key_Task();CHECK(sent[sent_count-1U][8]==11U);
    List_Clear(&ResendList);
    before=sent_count;input_b|=Hole_Left_Pin;tick=917U;Key_Task();tick=1116U;Key_Task();CHECK(sent_count==before);
    tick=1117U;Key_Task();CHECK(sent_count==before+1U && sent[before][4]==2U && sent[before][8]==0U);
    tick=1200U;Key_Task();CHECK(sent_count==before+1U);
    memcpy(echo,sent[before],14U);feed(echo,14U);CHECK(ResendList.NodeCount==0U);
    input_b&=~Hole_Left_Pin;tick=1201U;Key_Task();input_b|=Hole_Left_Pin;tick=1202U;Key_Task();tick=1402U;Key_Task();CHECK(sent_count==before+2U);
    List_Clear(&ResendList);
    before=sent_count;Comm_SendMesg_FillData(&Tx,5U,1U,5U,0U);
    tick+=1000U;CommTask();CHECK(sent_count==before+2U && sent[before+1U][1]==1U);
    tick+=1000U;CommTask();tick+=1000U;CommTask();CHECK(ResendList.NodeCount==0U);
    before=sent_count;tick+=1000U;CommTask();CHECK(sent_count==before);
    f[2]=88U;f[4]=0U;f[9]=0U;f[8]=0U;seal(f);feed(f,6U);feed(f,14U);
    CHECK(sent[sent_count-1U][3]==5U && sent[sent_count-1U][4]==0U);
    /* 条件模式保留原成对清洞行为，并要求左右阀都空闲。 */
    f[2]=110U;f[4]=0xFFU;f[8]=0U;f[9]=1U;f[10]=0U;seal(f);feed(f,14U);
    input_b&=~(Hole_Left_Pin|Hole_Right_Pin);
    f[2]=111U;f[4]=3U;f[10]=1U;seal(f);feed(f,14U);
    CHECK(ValveActive[0]==0U && ValveActive[1]==0U);
    input_b|=Hole_Right_Pin;
    f[2]=112U;seal(f);feed(f,14U);
    CHECK(ValveActive[0]==1U && ValveActive[1]==1U && (output_b&(Valve_Left_Pin|Valve_Right_Pin))==(Valve_Left_Pin|Valve_Right_Pin));
    tick++;f[2]=113U;seal(f);feed(f,14U);
    CHECK(ValveTick[0]==tick-1U && ValveTick[1]==tick-1U);
    f[2]=114U;f[4]=0xFFU;f[10]=0U;seal(f);feed(f,14U);
    f[2]=115U;f[4]=3U;seal(f);feed(f,14U);
    f[2]=116U;f[10]=1U;seal(f);feed(f,14U);
    CHECK(ValveActive[0]==1U && ValveActive[1]==0U);
    input_b&=~(Hole_Left_Pin|Hole_Right_Pin);
    f[2]=117U;f[8]=1U;f[10]=0U;seal(f);feed(f,14U);
    CHECK(ValveActive[1]==1U);
    f[2]=118U;f[4]=0xFFU;f[8]=0U;seal(f);feed(f,14U);
    input_b|=Hole_Right_Pin;
    f[2]=119U;f[4]=3U;f[8]=1U;f[10]=1U;seal(f);feed(f,14U);
    CHECK(ValveActive[0]==0U && ValveActive[1]==0U);
    input_b=(input_b&~Hole_Right_Pin)|Hole_Left_Pin;
    f[2]=126U;f[8]=0U;seal(f);feed(f,14U);
    CHECK(ValveActive[0]==1U && ValveActive[1]==1U);
    f[2]=127U;f[4]=0xFFU;f[10]=0U;seal(f);feed(f,14U);
    input_b|=Hole_Left_Pin|Hole_Right_Pin;
    f[2]=128U;f[4]=3U;f[10]=1U;seal(f);feed(f,14U);
    CHECK(ValveActive[0]==1U && ValveActive[1]==1U);
    tick++;f[1]=1U;seal(f);feed(f,14U);
    CHECK(ValveTick[0]==tick-1U && ValveTick[1]==tick-1U);
    f[1]=0U;f[2]=129U;f[4]=0xFFU;f[10]=0U;seal(f);feed(f,14U);
    input_b&=~(Hole_Left_Pin|Hole_Right_Pin);
    /* 后台微动跟灯不能覆盖游戏中奖掩码，退出后台后应恢复游戏掩码。 */
    input_a=0xFFFFU;input_b=0xFFF9U;
    f[2]=120U;f[4]=1U;f[8]=2U;f[10]=0U;seal(f);feed(f,14U);
    CHECK(latched==0x0400U);
    f[2]=121U;f[4]=7U;f[8]=0U;seal(f);feed(f,14U);tick++;Key_Task();
    CHECK(BallScene==BALL_SCENE_SETTING && latched==0U);
    input_a&=~GPIO_PIN_3;tick++;Key_Task();CHECK(latched==0x0800U);
    input_b&=~GPIO_PIN_5;tick++;Key_Task();CHECK(latched==0x0801U);
    input_a|=GPIO_PIN_3;tick++;Key_Task();CHECK(latched==1U);
    f[2]=122U;f[4]=1U;f[8]=4U;seal(f);feed(f,14U);
    CHECK(GameChannelMask==4U && latched==1U);
    f[2]=123U;f[4]=7U;f[8]=6U;seal(f);feed(f,14U);
    CHECK(BallScene==6U && latched==0x0200U);
    f[2]=124U;f[8]=12U;seal(f);feed(f,14U);CHECK(BallScene==6U && latched==0x0200U);
    f[2]=125U;f[8]=6U;seal(f);feed(f,14U);CHECK(latched==0x0200U);
    /* 回放Bootloader成功提示的实际报文：压住微动也不能覆盖一秒全亮。 */
    {
        static const uint8_t success_frames[3][14] = {
        {0xaaU,0x00U,0x01U,0x04U,0x07U,0x00U,0x00U,0x00U,0x01U,0x00U,0x00U,0x5dU,0x0eU,0x55U},
        {0xaaU,0x00U,0x02U,0x04U,0x01U,0x00U,0x00U,0x0fU,0xffU,0x00U,0x00U,0x49U,0x1eU,0x55U},
        {0xaaU,0x00U,0x03U,0x04U,0x01U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0xfdU,0x20U,0x55U}
        };
        f[2]=130U;f[4]=7U;f[8]=0U;seal(f);feed(f,14U);
        tick++;Key_Task();CHECK(BallScene==BALL_SCENE_SETTING && latched==1U);
        memcpy(f,success_frames[0],14U);feed(f,14U);
        CHECK(BallScene==BALL_SCENE_IDLE);
        memcpy(f,success_frames[1],14U);feed(f,14U);
        tick++;Key_Task();CHECK(latched==0x0FFFU && BallLight_GetChannels()==0x0FFFU);
        tick+=999U;Key_Task();CHECK(latched==0x0FFFU);
        memcpy(f,success_frames[2],14U);feed(f,14U);
        tick++;Key_Task();CHECK(latched==0U && GameChannelMask==0U);
        CHECK(ValveActive[0]==0U && ValveActive[1]==0U && ValveActive[2]==0U);
    }
    for(uint32_t i=0;i<sent_count;i++) CHECK(CRC16_calculate(sent[i],11U)==(((uint16_t)sent[i][11]<<8)|sent[i][12]));
finish:
    __asm { BKPT 0 }
    return failed_line;
}
