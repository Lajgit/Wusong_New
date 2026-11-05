#ifndef __APP_BOOTLOADER_H__
#define __APP_BOOTLOADER_H__

#include "main.h"
#include "app_sd.h"
#include <stdbool.h>
#include <stdint.h>

#define BOOTLOADER_ADDR 0x08000000U
#define BOOTLOADER_MAX_SIZE 0x0000C000U

#define APP_ADDR 0x0800C000U
#define APP_END_ADDR 0x080A0000U
#define APP_MAX_SIZE (APP_END_ADDR - APP_ADDR)

/*
 * 弹界原设置数据位于0x080E0000（Sector 11）。
 * OTA缓存只使用Sector 9、10，避免擦除原设置数据。
 */
#define OTA_CACHE_ADDR 0x080A0000U
#define OTA_META_ADDR 0x080DFF00U
#define OTA_META_VALID_ADDR OTA_META_ADDR
#define OTA_META_INSTALLING_MARKER_ADDR (OTA_META_ADDR + 0x20U)
#define OTA_META_INSTALLED_MARKER_ADDR (OTA_META_ADDR + 0x24U)
#define OTA_CACHE_MAX_SIZE (OTA_META_ADDR - OTA_CACHE_ADDR)

#define OTA_META_MARKER_EMPTY 0xFFFFFFFFU
#define OTA_META_MARKER_SET 0x00000000U

/* APP写入RTC备份寄存器的请求值。 */
#define OTA_REQUEST_MAGIC 0x424F5441U
/* BEGIN负载前4字节为42 4F 54 41，即ASCII“BOTA”。 */
#define OTA_TARGET_MAGIC 0x41544F42U
#define OTA_METADATA_MAGIC 0x4D41544FU

#define OTA_META_EMPTY 0xFFFFFFFFU
#define OTA_META_VALID 0xFFFFFFFEU
#define OTA_META_INSTALLING 0xFFFFFFFCU
#define OTA_META_INSTALLED 0xFFFFFFF8U

#define OTA_SOF1 0xAAU
#define OTA_SOF2 0x5AU
#define OTA_PROTOCOL_VERSION 0x01U
#define OTA_MAX_DATA_SIZE 1024U
#define OTA_MAX_PAYLOAD_SIZE (OTA_MAX_DATA_SIZE + 6U)
#define OTA_BOOT_VERSION 0x00010000U

#define OTA_CMD_HELLO 0x01U
#define OTA_CMD_BEGIN 0x02U
#define OTA_CMD_DATA 0x03U
#define OTA_CMD_END 0x04U
#define OTA_CMD_INSTALL 0x05U
#define OTA_CMD_STATUS 0x06U
#define OTA_CMD_ABORT 0x07U
#define OTA_CMD_REBOOT 0x08U
#define OTA_CMD_ACK 0x80U
#define OTA_CMD_NACK 0x81U

#define OTA_RESULT_OK 0x00U
#define OTA_RESULT_BAD_PROTOCOL 0x01U
#define OTA_RESULT_BAD_FRAME_CRC 0x02U
#define OTA_RESULT_BAD_LENGTH 0x03U
#define OTA_RESULT_BAD_TARGET 0x04U
#define OTA_RESULT_BAD_IMAGE_SIZE 0x05U
#define OTA_RESULT_FLASH_ERASE_FAILED 0x06U
#define OTA_RESULT_FLASH_WRITE_FAILED 0x07U
#define OTA_RESULT_OFFSET_MISMATCH 0x08U
#define OTA_RESULT_IMAGE_CRC_FAILED 0x09U
#define OTA_RESULT_IMAGE_INVALID 0x0AU
#define OTA_RESULT_NO_VALID_IMAGE 0x0BU
#define OTA_RESULT_INSTALL_FAILED 0x0CU
#define OTA_RESULT_NOT_STARTED 0x0DU
#define OTA_RESULT_UNKNOWN_COMMAND 0x0EU

#define OTA_SESSION_IDLE 0U
#define OTA_SESSION_RECEIVING 1U
#define OTA_SESSION_VERIFIED 2U
#define OTA_SESSION_INSTALLING 3U
#define OTA_SESSION_INSTALLED 4U
#define OTA_SESSION_ERROR 5U

typedef struct
{
    uint32_t magic;
    uint32_t state;
    uint32_t version_code;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t header_crc32;
} OtaMetadata_t;

HAL_StatusTypeDef Flash_Program(uint32_t StartAddress, uint8_t *Data, uint32_t Size);
HAL_StatusTypeDef Boot_FlashWrite(uint32_t start_address, const uint8_t *data, uint32_t size);
HAL_StatusTypeDef Boot_EraseCache(void);
bool Boot_FlashCompare(uint32_t address, const uint8_t *data, uint32_t size);
bool Boot_AppIsValid(void);
bool Boot_CachedImageVectorIsValid(uint32_t image_size);
bool Boot_ConsumeOtaRequest(void);
bool Boot_ReadMetadata(OtaMetadata_t *metadata);
HAL_StatusTypeDef Boot_WriteMetadata(uint32_t version_code, uint32_t image_size, uint32_t image_crc32);
HAL_StatusTypeDef Boot_SetMetadataState(uint32_t state);
HAL_StatusTypeDef Boot_InstallCachedImage(void);
void Boot_LightWaiting(void);
void Boot_LightSuccess(void);
void Boot_LightError(void);
void OTA_Run(void);
void JumpToApplication(void);
void App_Bootloader(void);

#endif
