#include "app_bootloader.h"
#include "app_ws2812.h"
#include "app_board.h"
#include "usart.h"
#include <stddef.h>
#include <string.h>

extern FATFS fs;
extern uint8_t r_buffer[1024];

static uint16_t RGB_buffer[Light_CRRbuffer_SIZE];

typedef struct
{
    uint8_t command;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t payload[OTA_MAX_PAYLOAD_SIZE];
} OtaPacket_t;

typedef struct
{
    uint8_t state;
    uint32_t version_code;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t expected_offset;
} OtaSession_t;

static OtaSession_t Session;
static OtaPacket_t Packet;
static FIL SdFirmwareFile;
static uint8_t RxFrame[2U + 6U + OTA_MAX_PAYLOAD_SIZE + 4U];
static uint8_t TxFrame[2U + 6U + 10U + 4U];

BootBoardId_t Boot_BoardGetId(void)
{
    /* 中文注释：只修改下一行后重新编译，不使用OTP。
     * 弹界：BOOT_BOARD_DANJIE；蟠桃：BOOT_BOARD_PANTAO；扭蛋：BOOT_BOARD_NIUDAN；武松：BOOT_BOARD_WUSONG。
     * 每次编译后将HEX另存为对应板型名称，避免下次编译覆盖。
     */
    BootBoardId_t board_id = BOOT_BOARD_WUSONG;
    return board_id;
}

const char *Boot_BoardGetFirmwareFileName(void)
{
    switch (Boot_BoardGetId())
    {
    case BOOT_BOARD_WUSONG:
        return FileName_Wusong;
    case BOOT_BOARD_PANTAO:
        return FileName_PanTao;
    case BOOT_BOARD_NIUDAN:
        return FileName_Niudan;
    case BOOT_BOARD_DANJIE:
    default:
        return FileName_Danjie;
    }
}

static uint16_t ReadU16LE(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8U);
}

static uint32_t ReadU32LE(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static void WriteU16LE(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void WriteU32LE(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static uint32_t CRC32_UpdateByte(uint32_t crc, uint8_t data)
{
    crc ^= data;
    for (uint8_t i = 0U; i < 8U; i++)
        crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
    return crc;
}

static uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    if (data == NULL && length != 0U)
        return 0U;
    for (uint32_t i = 0U; i < length; i++)
        crc = CRC32_UpdateByte(crc, data[i]);
    return crc ^ 0xFFFFFFFFU;
}

static uint32_t CRC32_CalculateFlash(uint32_t address, uint32_t length)
{
    return CRC32_Calculate((const uint8_t *)address, length);
}

static void Flash_ClearFlags(void)
{
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

static uint32_t Flash_GetSector(uint32_t address)
{
    if (address < 0x08004000U) return FLASH_SECTOR_0;
    if (address < 0x08008000U) return FLASH_SECTOR_1;
    if (address < 0x0800C000U) return FLASH_SECTOR_2;
    if (address < 0x08010000U) return FLASH_SECTOR_3;
    if (address < 0x08020000U) return FLASH_SECTOR_4;
    if (address < 0x08040000U) return FLASH_SECTOR_5;
    if (address < 0x08060000U) return FLASH_SECTOR_6;
    if (address < 0x08080000U) return FLASH_SECTOR_7;
    if (address < 0x080A0000U) return FLASH_SECTOR_8;
    if (address < 0x080C0000U) return FLASH_SECTOR_9;
    if (address < 0x080E0000U) return FLASH_SECTOR_10;
    return FLASH_SECTOR_11;
}

static HAL_StatusTypeDef Flash_WriteUnlocked(uint32_t address, const uint8_t *data, uint32_t size)
{
    uint32_t offset = 0U;
    if ((address & 3U) != 0U || data == NULL || size == 0U)
        return HAL_ERROR;
    while (offset < size)
    {
        uint32_t word = 0xFFFFFFFFU;
        uint32_t copy_size = size - offset >= 4U ? 4U : size - offset;
        memcpy(&word, &data[offset], copy_size);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, word) != HAL_OK)
            return HAL_ERROR;
        address += 4U;
        offset += copy_size;
    }
    return HAL_OK;
}

/* 复用微动板14字节协议：04/07设置场景，04/01以Data3/4控制12路灯。 */
static void Boot_BallSend(uint8_t command, uint16_t data)
{
    static uint8_t id = 0U;
    uint8_t frame[14] = {0xAAU, 0U, 0U, 0x04U, command, 0U, 0U,
                         (uint8_t)(data >> 8U), (uint8_t)data, 0U, 0U, 0U, 0U, 0x55U};
    uint16_t crc = 0xFFFFU;
    frame[2] = ++id;
    /* CRC16覆盖前11字节，高字节先发；提示帧不要求回显应答。 */
    for (uint8_t i = 0U; i < 11U; i++)
    {
        crc ^= frame[i];
        for (uint8_t bit = 0U; bit < 8U; bit++)
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xA001U : crc >> 1U;
    }
    frame[11] = (uint8_t)(crc >> 8U);
    frame[12] = (uint8_t)crc;
    HAL_UART_Transmit(&huart2, frame, sizeof(frame), 100U);
}

/* 武松等待/失败仍用板载灯，成功改用微动板；其他板型保留原RGB提示。 */
void Boot_LightWaiting(void)
{
    if (Boot_BoardGetId() == BOOT_BOARD_WUSONG)
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    else
        Light_SetAllColor(SKYBLUE, RGB_buffer);
}
void Boot_LightSuccess(void)
{
    if (Boot_BoardGetId() == BOOT_BOARD_WUSONG)
    {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        /* 先退出后台跟灯场景，避免微动扫描覆盖全亮；一秒后主动熄灭再交给APP。 */
        Boot_BallSend(0x07U, 0x0001U);
        Boot_BallSend(0x01U, 0x0FFFU);
        HAL_Delay(1000U);
        Boot_BallSend(0x01U, 0x0000U);
    }
    else
        Light_SetAllColor(GREEN, RGB_buffer);
}
void Boot_LightError(void)
{
    if (Boot_BoardGetId() == BOOT_BOARD_WUSONG)
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    else
        Light_SetAllColor(RED, RGB_buffer);
}

HAL_StatusTypeDef Flash_Program(uint32_t StartAddress, uint8_t *Data, uint32_t Size)
{
    return Flash_WriteUnlocked(StartAddress, Data, Size);
}

HAL_StatusTypeDef Boot_FlashWrite(uint32_t address, const uint8_t *data, uint32_t size)
{
    HAL_StatusTypeDef status;
    if ((address & 3U) != 0U || data == NULL || size == 0U || HAL_FLASH_Unlock() != HAL_OK)
        return HAL_ERROR;
    Flash_ClearFlags();
    status = Flash_WriteUnlocked(address, data, size);
    HAL_FLASH_Lock();
    return status;
}

HAL_StatusTypeDef Boot_EraseCache(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    HAL_StatusTypeDef status;
    if (HAL_FLASH_Unlock() != HAL_OK)
        return HAL_ERROR;
    Flash_ClearFlags();
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector = FLASH_SECTOR_9;
    erase.NbSectors = 2U;
    status = HAL_FLASHEx_Erase(&erase, &sector_error);
    HAL_FLASH_Lock();
    return status;
}

bool Boot_FlashCompare(uint32_t address, const uint8_t *data, uint32_t size)
{
    return data != NULL && memcmp((const void *)address, data, size) == 0;
}

static bool StackPointerIsValid(uint32_t value)
{
    return value >= 0x20000000U && value < 0x20020000U;
}

static bool ResetHandlerIsValid(uint32_t value)
{
    uint32_t address = value & ~1U;
    return (value & 1U) != 0U && address >= APP_ADDR && address < APP_END_ADDR;
}

bool Boot_AppIsValid(void)
{
    return StackPointerIsValid(*(volatile uint32_t *)APP_ADDR) &&
           ResetHandlerIsValid(*(volatile uint32_t *)(APP_ADDR + 4U));
}

bool Boot_CachedImageVectorIsValid(uint32_t image_size)
{
    if (image_size < 8U || image_size > OTA_CACHE_MAX_SIZE || image_size > APP_MAX_SIZE)
        return false;
    return StackPointerIsValid(*(volatile uint32_t *)OTA_CACHE_ADDR) &&
           ResetHandlerIsValid(*(volatile uint32_t *)(OTA_CACHE_ADDR + 4U));
}

bool Boot_ConsumeOtaRequest(void)
{
    uint32_t timeout = 100000U;
    bool requested;
    __HAL_RCC_PWR_CLK_ENABLE();
    __DSB();
    (void)RCC->APB1ENR;
    HAL_PWR_EnableBkUpAccess();
    while (((PWR->CR & PWR_CR_DBP) == 0U) && timeout > 0U)
        timeout--;
    if (timeout == 0U)
    {
        HAL_PWR_DisableBkUpAccess();
        return false;
    }
    __HAL_RCC_RTC_ENABLE();
    __DSB();
    (void)RCC->BDCR;
    requested = RTC->BKP0R == OTA_REQUEST_MAGIC;
    for (uint32_t retry = 0U; retry < 3U; retry++)
    {
        RTC->BKP0R = 0U;
        __DSB();
        __ISB();
        if (RTC->BKP0R == 0U)
            break;
    }
    HAL_PWR_DisableBkUpAccess();
    return requested;
}

static uint32_t Metadata_CalculateCrc(const OtaMetadata_t *metadata)
{
    uint32_t fields[4] = {metadata->magic, metadata->version_code,
                          metadata->image_size, metadata->image_crc32};
    return CRC32_Calculate((const uint8_t *)fields, sizeof(fields));
}

bool Boot_ReadMetadata(OtaMetadata_t *metadata)
{
    if (metadata == NULL)
        return false;
    memcpy(metadata, (const void *)OTA_META_VALID_ADDR, sizeof(*metadata));
    if (metadata->magic != OTA_METADATA_MAGIC || metadata->state != OTA_META_VALID ||
        metadata->image_size == 0U || metadata->image_size > OTA_CACHE_MAX_SIZE ||
        metadata->image_size > APP_MAX_SIZE ||
        metadata->header_crc32 != Metadata_CalculateCrc(metadata))
        return false;
    if (*(volatile uint32_t *)OTA_META_INSTALLED_MARKER_ADDR == OTA_META_MARKER_SET)
        metadata->state = OTA_META_INSTALLED;
    else if (*(volatile uint32_t *)OTA_META_INSTALLING_MARKER_ADDR == OTA_META_MARKER_SET)
        metadata->state = OTA_META_INSTALLING;
    return true;
}

HAL_StatusTypeDef Boot_WriteMetadata(uint32_t version_code, uint32_t image_size,
                                     uint32_t image_crc32)
{
    OtaMetadata_t metadata;
    metadata.magic = OTA_METADATA_MAGIC;
    metadata.state = OTA_META_VALID;
    metadata.version_code = version_code;
    metadata.image_size = image_size;
    metadata.image_crc32 = image_crc32;
    metadata.header_crc32 = Metadata_CalculateCrc(&metadata);
    if (*(volatile uint32_t *)OTA_META_INSTALLING_MARKER_ADDR != OTA_META_MARKER_EMPTY ||
        *(volatile uint32_t *)OTA_META_INSTALLED_MARKER_ADDR != OTA_META_MARKER_EMPTY)
        return HAL_ERROR;
    return Boot_FlashWrite(OTA_META_VALID_ADDR, (const uint8_t *)&metadata, sizeof(metadata));
}

HAL_StatusTypeDef Boot_SetMetadataState(uint32_t state)
{
    uint32_t address;
    uint32_t marker = OTA_META_MARKER_SET;
    if (state == OTA_META_INSTALLING)
        address = OTA_META_INSTALLING_MARKER_ADDR;
    else if (state == OTA_META_INSTALLED)
        address = OTA_META_INSTALLED_MARKER_ADDR;
    else
        return HAL_ERROR;
    if (*(volatile uint32_t *)address == OTA_META_MARKER_SET)
        return HAL_OK;
    if (*(volatile uint32_t *)address != OTA_META_MARKER_EMPTY ||
        Boot_FlashWrite(address, (const uint8_t *)&marker, sizeof(marker)) != HAL_OK)
        return HAL_ERROR;
    __DSB();
    __ISB();
    return *(volatile uint32_t *)address == OTA_META_MARKER_SET ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef Boot_InstallCachedImage(void)
{
    OtaMetadata_t metadata;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    uint32_t offset = 0U;
    HAL_StatusTypeDef status = HAL_ERROR;
    if (!Boot_ReadMetadata(&metadata) ||
        CRC32_CalculateFlash(OTA_CACHE_ADDR, metadata.image_size) != metadata.image_crc32 ||
        !Boot_CachedImageVectorIsValid(metadata.image_size))
        return HAL_ERROR;
    if (metadata.state != OTA_META_INSTALLING &&
        Boot_SetMetadataState(OTA_META_INSTALLING) != HAL_OK)
        return HAL_ERROR;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector = Flash_GetSector(APP_ADDR);
    erase.NbSectors = Flash_GetSector(APP_ADDR + metadata.image_size - 1U) - erase.Sector + 1U;
    if (HAL_FLASH_Unlock() != HAL_OK)
        return HAL_ERROR;
    Flash_ClearFlags();
    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
        goto finish;
    while (offset < metadata.image_size)
    {
        uint32_t block_size = metadata.image_size - offset > 1024U ?
                              1024U : metadata.image_size - offset;
        if (Flash_WriteUnlocked(APP_ADDR + offset,
                                (const uint8_t *)(OTA_CACHE_ADDR + offset),
                                block_size) != HAL_OK)
            goto finish;
        offset += block_size;
    }
    if (CRC32_CalculateFlash(APP_ADDR, metadata.image_size) == metadata.image_crc32 &&
        Boot_AppIsValid())
        status = HAL_OK;
finish:
    HAL_FLASH_Lock();
    if (status == HAL_OK &&
        (Boot_SetMetadataState(OTA_META_INSTALLED) != HAL_OK ||
         !Boot_ReadMetadata(&metadata) || metadata.state != OTA_META_INSTALLED))
        return HAL_ERROR;
    return status;
}

static HAL_StatusTypeDef OTA_SendPacket(uint8_t command, uint16_t sequence,
                                        const uint8_t *payload, uint16_t length)
{
    if (length > 10U)
        return HAL_ERROR;
    TxFrame[0] = OTA_SOF1;
    TxFrame[1] = OTA_SOF2;
    TxFrame[2] = OTA_PROTOCOL_VERSION;
    TxFrame[3] = command;
    WriteU16LE(&TxFrame[4], sequence);
    WriteU16LE(&TxFrame[6], length);
    if (length > 0U && payload != NULL)
        memcpy(&TxFrame[8], payload, length);
    WriteU32LE(&TxFrame[8U + length], CRC32_Calculate(&TxFrame[2], 6U + length));
    return HAL_UART_Transmit(&huart1, TxFrame, (uint16_t)(12U + length), 1000U);
}

static void OTA_SendReply(uint16_t sequence, uint8_t request_command,
                          uint8_t result, uint32_t value)
{
    uint8_t payload[10];
    payload[0] = request_command;
    payload[1] = result;
    payload[2] = Session.state;
    payload[3] = 0U;
    WriteU32LE(&payload[4], value);
    WriteU16LE(&payload[8], OTA_MAX_DATA_SIZE);
    OTA_SendPacket(result == OTA_RESULT_OK ? OTA_CMD_ACK : OTA_CMD_NACK,
                   sequence, payload, sizeof(payload));
}

static HAL_StatusTypeDef OTA_ReceivePacket(OtaPacket_t *packet)
{
    uint8_t byte;
    uint16_t length;
    while (1)
    {
        if (HAL_UART_Receive(&huart1, &byte, 1U, 1000U) != HAL_OK)
            return HAL_TIMEOUT;
        if (byte != OTA_SOF1)
            continue;
        RxFrame[0] = byte;
        if (HAL_UART_Receive(&huart1, &RxFrame[1], 1U, 100U) != HAL_OK ||
            RxFrame[1] != OTA_SOF2)
            continue;
        if (HAL_UART_Receive(&huart1, &RxFrame[2], 6U, 1000U) != HAL_OK)
            return HAL_ERROR;
        packet->command = RxFrame[3];
        packet->sequence = ReadU16LE(&RxFrame[4]);
        length = ReadU16LE(&RxFrame[6]);
        packet->payload_length = length;
        if (RxFrame[2] != OTA_PROTOCOL_VERSION)
        {
            OTA_SendReply(packet->sequence, packet->command,
                          OTA_RESULT_BAD_PROTOCOL, Session.expected_offset);
            return HAL_ERROR;
        }
        if (length > OTA_MAX_PAYLOAD_SIZE)
        {
            OTA_SendReply(packet->sequence, packet->command,
                          OTA_RESULT_BAD_LENGTH, Session.expected_offset);
            return HAL_ERROR;
        }
        if (HAL_UART_Receive(&huart1, &RxFrame[8], (uint16_t)(length + 4U), 3000U) != HAL_OK)
            return HAL_ERROR;
        if (ReadU32LE(&RxFrame[8U + length]) != CRC32_Calculate(&RxFrame[2], 6U + length))
        {
            OTA_SendReply(packet->sequence, packet->command,
                          OTA_RESULT_BAD_FRAME_CRC, Session.expected_offset);
            return HAL_ERROR;
        }
        if (length > 0U)
            memcpy(packet->payload, &RxFrame[8], length);
        return HAL_OK;
    }
}

static void OTA_HandleHello(const OtaPacket_t *packet)
{
    OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_OK, OTA_BOOT_VERSION);
}

static void OTA_HandleBegin(const OtaPacket_t *packet)
{
    uint32_t image_size;
    if (packet->payload_length != 16U)
    {
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_BAD_LENGTH, 0U);
        return;
    }
    if (ReadU32LE(&packet->payload[0]) != OTA_TARGET_MAGIC)
    {
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_BAD_TARGET,
                      ReadU32LE(&packet->payload[0]));
        return;
    }
    image_size = ReadU32LE(&packet->payload[8]);
    if (image_size == 0U || image_size > OTA_CACHE_MAX_SIZE || image_size > APP_MAX_SIZE)
    {
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_BAD_IMAGE_SIZE, image_size);
        return;
    }
    memset(&Session, 0, sizeof(Session));
    if (Boot_EraseCache() != HAL_OK)
    {
        Session.state = OTA_SESSION_ERROR;
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_FLASH_ERASE_FAILED, 0U);
        return;
    }
    Session.version_code = ReadU32LE(&packet->payload[4]);
    Session.image_size = image_size;
    Session.image_crc32 = ReadU32LE(&packet->payload[12]);
    Session.state = OTA_SESSION_RECEIVING;
    OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_OK, 0U);
}

static void OTA_HandleData(const OtaPacket_t *packet)
{
    uint32_t offset;
    uint16_t length;
    const uint8_t *data;
    if (Session.state != OTA_SESSION_RECEIVING)
    {
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_NOT_STARTED, Session.expected_offset);
        return;
    }
    if (packet->payload_length < 7U)
    {
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_BAD_LENGTH, Session.expected_offset);
        return;
    }
    offset = ReadU32LE(&packet->payload[0]);
    length = ReadU16LE(&packet->payload[4]);
    data = &packet->payload[6];
    if (length == 0U || length > OTA_MAX_DATA_SIZE ||
        packet->payload_length != (uint16_t)(6U + length) ||
        offset > Session.image_size || length > Session.image_size - offset ||
        (offset & 3U) != 0U ||
        ((offset + length) < Session.image_size && (length & 3U) != 0U))
    {
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_BAD_LENGTH, Session.expected_offset);
        return;
    }
    if (offset == Session.expected_offset)
    {
        if (Boot_FlashWrite(OTA_CACHE_ADDR + offset, data, length) != HAL_OK ||
            !Boot_FlashCompare(OTA_CACHE_ADDR + offset, data, length))
        {
            Session.state = OTA_SESSION_ERROR;
            OTA_SendReply(packet->sequence, packet->command,
                          OTA_RESULT_FLASH_WRITE_FAILED, Session.expected_offset);
            return;
        }
        Session.expected_offset += length;
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_OK, Session.expected_offset);
        return;
    }
    if (offset < Session.expected_offset && offset + length <= Session.expected_offset &&
        Boot_FlashCompare(OTA_CACHE_ADDR + offset, data, length))
    {
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_OK, Session.expected_offset);
        return;
    }
    OTA_SendReply(packet->sequence, packet->command,
                  OTA_RESULT_OFFSET_MISMATCH, Session.expected_offset);
}

static void OTA_HandleEnd(const OtaPacket_t *packet)
{
    uint32_t crc;
    if (packet->payload_length != 0U)
    {
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_BAD_LENGTH, Session.expected_offset);
        return;
    }
    if (Session.state != OTA_SESSION_RECEIVING || Session.expected_offset != Session.image_size)
    {
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_OFFSET_MISMATCH, Session.expected_offset);
        return;
    }
    crc = CRC32_CalculateFlash(OTA_CACHE_ADDR, Session.image_size);
    if (crc != Session.image_crc32)
    {
        Session.state = OTA_SESSION_ERROR;
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_IMAGE_CRC_FAILED, crc);
        return;
    }
    if (!Boot_CachedImageVectorIsValid(Session.image_size))
    {
        Session.state = OTA_SESSION_ERROR;
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_IMAGE_INVALID, 0U);
        return;
    }
    if (Boot_WriteMetadata(Session.version_code, Session.image_size, Session.image_crc32) != HAL_OK)
    {
        Session.state = OTA_SESSION_ERROR;
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_FLASH_WRITE_FAILED, 0U);
        return;
    }
    Session.state = OTA_SESSION_VERIFIED;
    OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_OK, Session.image_size);
}

static void OTA_HandleInstall(const OtaPacket_t *packet)
{
    OtaMetadata_t metadata;
    if (packet->payload_length != 0U)
    {
        OTA_SendReply(packet->sequence, packet->command,
                      OTA_RESULT_BAD_LENGTH, Session.expected_offset);
        return;
    }
    if (!Boot_ReadMetadata(&metadata))
    {
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_NO_VALID_IMAGE, 0U);
        return;
    }
    Session.state = OTA_SESSION_INSTALLING;
    if (Boot_InstallCachedImage() != HAL_OK)
    {
        Session.state = OTA_SESSION_ERROR;
        Boot_LightError();
        OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_INSTALL_FAILED, 0U);
        return;
    }
    Session.state = OTA_SESSION_INSTALLED;
    OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_OK, metadata.version_code);
    Boot_LightSuccess();
    if (Boot_BoardGetId() != BOOT_BOARD_WUSONG)
        HAL_Delay(1000U);
    NVIC_SystemReset();
}

static void OTA_HandleStatus(const OtaPacket_t *packet)
{
    OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_OK, Session.expected_offset);
}

static void OTA_HandleAbort(const OtaPacket_t *packet)
{
    memset(&Session, 0, sizeof(Session));
    Session.state = OTA_SESSION_IDLE;
    OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_OK, 0U);
}

static void OTA_HandleReboot(const OtaPacket_t *packet)
{
    OTA_SendReply(packet->sequence, packet->command, OTA_RESULT_OK, 0U);
    HAL_Delay(100U);
    NVIC_SystemReset();
}

void OTA_Run(void)
{
    OtaMetadata_t metadata;
    memset(&Session, 0, sizeof(Session));
    Session.state = OTA_SESSION_IDLE;
    if (Boot_ReadMetadata(&metadata) && metadata.state == OTA_META_INSTALLED)
        Session.state = OTA_SESSION_INSTALLED;
    while (1)
    {
        if (OTA_ReceivePacket(&Packet) != HAL_OK)
            continue;
        switch (Packet.command)
        {
        case OTA_CMD_HELLO: OTA_HandleHello(&Packet); break;
        case OTA_CMD_BEGIN: OTA_HandleBegin(&Packet); break;
        case OTA_CMD_DATA: OTA_HandleData(&Packet); break;
        case OTA_CMD_END: OTA_HandleEnd(&Packet); break;
        case OTA_CMD_INSTALL: OTA_HandleInstall(&Packet); break;
        case OTA_CMD_STATUS: OTA_HandleStatus(&Packet); break;
        case OTA_CMD_ABORT: OTA_HandleAbort(&Packet); break;
        case OTA_CMD_REBOOT: OTA_HandleReboot(&Packet); break;
        default:
            OTA_SendReply(Packet.sequence, Packet.command,
                          OTA_RESULT_UNKNOWN_COMMAND, Session.expected_offset);
            break;
        }
    }
}

void JumpToApplication(void)
{
    typedef void (*AppEntry_t)(void);
    uint32_t app_stack;
    AppEntry_t entry;
    if (!Boot_AppIsValid())
        return;
    app_stack = *(volatile uint32_t *)APP_ADDR;
    entry = (AppEntry_t)(*(volatile uint32_t *)(APP_ADDR + 4U));
    __disable_irq();
    HAL_UART_DeInit(&huart1);
    if (Boot_BoardGetId() == BOOT_BOARD_WUSONG)
        HAL_UART_DeInit(&huart2);
    HAL_DeInit();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    SCB->VTOR = APP_ADDR;
    __set_CONTROL(0U);
    __set_MSP(app_stack);
    __DSB();
    __ISB();
    __enable_irq();
    entry();
    while (1) {}
}

static bool Boot_TrySdUpgrade(void)
{
    UINT bytes_read;
    uint32_t file_size;
    uint32_t sector_error = 0U;
    uint32_t offset = 0U;
    FLASH_EraseInitTypeDef erase = {0};
    HAL_StatusTypeDef program_status = HAL_ERROR;
    bool programmed = false;
    if (mount_disk(&fs, "", 0) != FR_OK)
    {
        f_mount(NULL, "", 0);
        return false;
    }
    if (f_open(&SdFirmwareFile, Boot_BoardGetFirmwareFileName(), FA_READ) != FR_OK)
    {
        f_mount(NULL, "", 0);
        return false;
    }
    file_size = f_size(&SdFirmwareFile);
    if (file_size < 8U || file_size > APP_MAX_SIZE)
        goto finish;
    if (Boot_BoardGetId() == BOOT_BOARD_WUSONG)
    {
        /* 旧武松APP链接于0x08004000，新APP为0x0800C000；先检查再擦除。 */
        if (read_file(&SdFirmwareFile, r_buffer, 8U, &bytes_read) != FR_OK || bytes_read != 8U ||
            !StackPointerIsValid(ReadU32LE(r_buffer)) ||
            !ResetHandlerIsValid(ReadU32LE(&r_buffer[4])) ||
            f_lseek(&SdFirmwareFile, 0U) != FR_OK)
            goto finish;
    }
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_SECTOR_3;
    erase.NbSectors = Flash_GetSector(APP_ADDR + file_size - 1U) - FLASH_SECTOR_3 + 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    if (HAL_FLASH_Unlock() != HAL_OK)
        goto finish;
    Flash_ClearFlags();
    if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
    {
        HAL_FLASH_Lock();
        goto finish;
    }
    program_status = HAL_OK;
    while (offset < file_size)
    {
        if (read_file(&SdFirmwareFile, r_buffer, sizeof(r_buffer), &bytes_read) != FR_OK ||
            bytes_read == 0U ||
            Flash_Program(APP_ADDR + offset, r_buffer, bytes_read) != HAL_OK)
        {
            program_status = HAL_ERROR;
            break;
        }
        offset += bytes_read;
    }
    HAL_FLASH_Lock();
    programmed = program_status == HAL_OK && offset == file_size && Boot_AppIsValid();
finish:
    close_file(&SdFirmwareFile);
    f_mount(NULL, "", 0);
    if (programmed)
    {
        Boot_LightSuccess();
        /* 武松的一秒提示已在Boot_LightSuccess完成，其他板型保留原闪烁时序。 */
        if (Boot_BoardGetId() != BOOT_BOARD_WUSONG)
        {
            for (uint8_t i = 0U; i < 6U; i++)
            {
                HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
                HAL_Delay(100U);
            }
            HAL_Delay(400U);
        }
    }
    else
        Boot_LightError();
    return programmed;
}

void App_Bootloader(void)
{
    OtaMetadata_t metadata;
    bool recovery_pending = false;
    if (Boot_ConsumeOtaRequest())
    {
        Boot_LightWaiting();
        OTA_Run();
    }
    if (Boot_ReadMetadata(&metadata) &&
        (metadata.state == OTA_META_VALID || metadata.state == OTA_META_INSTALLING))
    {
        recovery_pending = true;
        Boot_LightWaiting();
        if (Boot_InstallCachedImage() == HAL_OK)
        {
            Boot_LightSuccess();
            if (Boot_BoardGetId() != BOOT_BOARD_WUSONG)
                HAL_Delay(1000U);
            JumpToApplication();
        }
        Boot_LightError();
    }
    if (!recovery_pending && Boot_TrySdUpgrade())
        JumpToApplication();
    if (!recovery_pending && Boot_AppIsValid())
    {
        Boot_LightWaiting();
        HAL_Delay(1000U);
        JumpToApplication();
    }
    if (!recovery_pending)
        Boot_LightWaiting();
    OTA_Run();
}
