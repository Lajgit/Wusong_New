#ifndef __APP_BOARD_H__
#define __APP_BOARD_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 中文注释：
 * Board ID在Boot_BoardGetId()中直接指定，修改后分别编译三份Bootloader。
 * 不读取或写入OTP，各板型通过Board ID选择升级文件名和原有灯效。
 */
typedef enum
{
    BOOT_BOARD_DANJIE = 1U,
    BOOT_BOARD_PANTAO = 2U,
    BOOT_BOARD_NIUDAN = 3U
} BootBoardId_t;

BootBoardId_t Boot_BoardGetId(void);
const char *Boot_BoardGetFirmwareFileName(void);

#ifdef __cplusplus
}
#endif

#endif
