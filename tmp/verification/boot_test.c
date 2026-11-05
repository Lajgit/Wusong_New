/* ARM仿真回归：直接包含生产源码，硬件接口使用测试桩，不代表实物已验证。 */
#include "app_bootloader.h"
#include "app_board.h"
#include "app_ws2812.h"
#include "usart.h"
#include <string.h>

static uint32_t checks,failed_line,erase_count,program_count,rgb_calls;
static uint8_t image[129],serial_rx[1100],serial_tx[1100];
static uint32_t serial_length,serial_offset,tx_length,sd_offset,sd_size=129U;
static uint8_t sd_present=1U;
static uint16_t led_state;
static uint32_t sector_start[13]={0x08000000,0x08004000,0x08008000,0x0800C000,0x08010000,0x08020000,0x08040000,0x08060000,0x08080000,0x080A0000,0x080C0000,0x080E0000,0x08100000};
FATFS fs;
uint8_t r_buffer[1024];
UART_HandleTypeDef huart1;
RGB_t Color_table[10];
HAL_StatusTypeDef HAL_DeInit(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_UART_DeInit(UART_HandleTypeDef *h) { return HAL_OK; }
void HAL_PWR_EnableBkUpAccess(void) {}
void HAL_PWR_DisableBkUpAccess(void) {}
HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Lock(void) { return HAL_OK; }
HAL_StatusTypeDef HAL_FLASH_Program(uint32_t type,uint32_t address,uint64_t data)
{
    *(uint32_t *)address &= (uint32_t)data;
    program_count++;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_FLASHEx_Erase(FLASH_EraseInitTypeDef *e,uint32_t *error)
{
    if(e->Sector<3U || e->Sector+e->NbSectors>11U) return HAL_ERROR;
    memset((void *)sector_start[e->Sector],0xFFU,sector_start[e->Sector+e->NbSectors]-sector_start[e->Sector]);
    erase_count++;
    *error=0xFFFFFFFFU;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h,const uint8_t *b,uint16_t n,uint32_t timeout)
{ memcpy(serial_tx,b,n);tx_length=n;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *h,uint8_t *b,uint16_t n,uint32_t timeout)
{
    if(serial_offset+n>serial_length) return HAL_TIMEOUT;
    memcpy(b,serial_rx+serial_offset,n);serial_offset+=n;return HAL_OK;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState state) { led_state=state; }
void HAL_GPIO_TogglePin(GPIO_TypeDef *p,uint16_t pin) { led_state=!led_state; }
void HAL_Delay(uint32_t ms) {}
void Light_SetAllColor(RGB_t c,uint16_t *b) { rgb_calls++; }
FRESULT mount_disk(FATFS *f,const TCHAR *p,BYTE o) { return FR_OK; }
FRESULT f_mount(FATFS *f,const TCHAR *p,BYTE o) { return FR_OK; }
FRESULT f_open(FIL *f,const TCHAR *path,BYTE mode)
{
    if(!sd_present || strcmp(path,"wusong.bin")!=0) return FR_NO_FILE;
    f->obj.objsize=sd_size;sd_offset=0U;return FR_OK;
}
FRESULT read_file(FIL *f,void *buffer,UINT count,UINT *got)
{
    *got=sizeof(image)-sd_offset;
    if(*got>count) *got=count;
    memcpy(buffer,image+sd_offset,*got);sd_offset+=*got;return FR_OK;
}
FRESULT f_lseek(FIL *f,FSIZE_t offset) { sd_offset=offset;return FR_OK; }
FRESULT close_file(FIL *f) { return FR_OK; }

#include "../../Bootloader_Danjie/App/Src/app_bootloader.c"
#define CHECK(x) do { checks++; if(!(x)) { failed_line=__LINE__;goto finish; } } while(0)

uint32_t test_main(void)
{
    OtaPacket_t packet={0};OtaMetadata_t metadata;
    uint32_t crc,before;
    memset((void *)0x08008000U,0xA5U,0x100U);
    memset((void *)0x080A0000U,0xFFU,0x40000U);
    memset((void *)0x080E0000U,0x5AU,0x100U);
    CHECK(Boot_BoardGetId()==BOOT_BOARD_WUSONG);
    CHECK(strcmp(Boot_BoardGetFirmwareFileName(),"wusong.bin")==0);
    Boot_LightWaiting();CHECK(led_state==GPIO_PIN_RESET);
    Boot_LightSuccess();Boot_LightError();CHECK(rgb_calls==0U && led_state==GPIO_PIN_SET);
    CHECK(CRC32_Calculate((const uint8_t *)"123456789",9U)==0xCBF43926U);
    for(uint32_t i=0U;i<sizeof(image);i++) image[i]=(uint8_t)i;
    WriteU32LE(image,0x20010000U);WriteU32LE(image+4U,APP_ADDR+0x41U);
    crc=CRC32_Calculate(image,sizeof(image));
    packet.command=OTA_CMD_HELLO;packet.sequence=0x1234U;
    OTA_HandleHello(&packet);
    CHECK(tx_length==22U && serial_tx[0]==0xAAU && serial_tx[1]==0x5AU && serial_tx[3]==OTA_CMD_ACK && ReadU16LE(serial_tx+4U)==0x1234U);
    CHECK(ReadU32LE(serial_tx+18U)==CRC32_Calculate(serial_tx+2U,16U));
    packet.command=OTA_CMD_BEGIN;packet.payload_length=16U;
    WriteU32LE(packet.payload,0U);WriteU32LE(packet.payload+8U,sizeof(image));
    OTA_HandleBegin(&packet);CHECK(serial_tx[3]==OTA_CMD_NACK && serial_tx[9]==OTA_RESULT_BAD_TARGET && erase_count==0U);
    WriteU32LE(packet.payload,OTA_TARGET_MAGIC);WriteU32LE(packet.payload+4U,0x01000400U);
    WriteU32LE(packet.payload+12U,crc);OTA_HandleBegin(&packet);
    CHECK(Session.state==OTA_SESSION_RECEIVING && erase_count==1U);
    packet.command=OTA_CMD_DATA;packet.payload_length=6U+sizeof(image);
    WriteU32LE(packet.payload,0U);WriteU16LE(packet.payload+4U,sizeof(image));memcpy(packet.payload+6U,image,sizeof(image));
    OTA_HandleData(&packet);CHECK(Session.expected_offset==sizeof(image) && memcmp((void *)OTA_CACHE_ADDR,image,sizeof(image))==0);
    before=program_count;OTA_HandleData(&packet);CHECK(program_count==before && serial_tx[3]==OTA_CMD_ACK);
    packet.payload_length=0U;OTA_HandleEnd(&packet);
    CHECK(Session.state==OTA_SESSION_VERIFIED && Boot_ReadMetadata(&metadata) && metadata.state==OTA_META_VALID);
    CHECK(Boot_InstallCachedImage()==HAL_OK && Boot_AppIsValid());
    CHECK(memcmp((void *)APP_ADDR,image,sizeof(image))==0 && Boot_ReadMetadata(&metadata) && metadata.state==OTA_META_INSTALLED);
    CHECK(*(uint32_t *)0x08008000U==0xA5A5A5A5U && *(uint32_t *)0x080E0000U==0x5A5A5A5AU);
    sd_present=0U;before=erase_count;CHECK(!Boot_TrySdUpgrade() && erase_count==before);
    sd_present=1U;sd_size=APP_MAX_SIZE+1U;CHECK(!Boot_TrySdUpgrade() && erase_count==before);
    sd_size=sizeof(image);WriteU32LE(image+4U,0x08004041U);
    before=erase_count;CHECK(!Boot_TrySdUpgrade() && erase_count==before && Boot_AppIsValid());
    WriteU32LE(image+4U,APP_ADDR+0x41U);
    memset((void *)APP_ADDR,0,256U);CHECK(Boot_TrySdUpgrade());
    CHECK(memcmp((void *)APP_ADDR,image,sizeof(image))==0 && ((uint8_t *)APP_ADDR)[129]==0xFFU);
    CHECK(*(uint32_t *)0x08008000U==0xA5A5A5A5U && *(uint32_t *)0x080E0000U==0x5A5A5A5AU && rgb_calls==0U);
    /* 验证Bootloader串口帧解析及错误CRC拒绝，不接入应用端升级入口。 */
    serial_rx[0]=0xAA;serial_rx[1]=0x5A;serial_rx[2]=1;serial_rx[3]=OTA_CMD_HELLO;
    WriteU16LE(serial_rx+4U,55U);WriteU16LE(serial_rx+6U,0U);
    WriteU32LE(serial_rx+8U,CRC32_Calculate(serial_rx+2U,6U));serial_offset=0U;serial_length=12U;
    CHECK(OTA_ReceivePacket(&packet)==HAL_OK && packet.command==OTA_CMD_HELLO && packet.sequence==55U);
    serial_rx[8]^=1U;serial_offset=0U;CHECK(OTA_ReceivePacket(&packet)==HAL_ERROR && serial_tx[9]==OTA_RESULT_BAD_FRAME_CRC);
finish:
    __asm { BKPT 0 }
    return failed_line;
}
