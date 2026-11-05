#include "InterruptTask.h"
#include "CommTask.h"

extern Rx_HandleTypeDef Rx;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == Rx.Handle.huart)
    {
        /* 中文注释：USART2每收到1字节写入环形缓冲区，并立即继续接收下一字节。 */
        Rx.Handle.RingBuf.f_WriteByte(&Rx.Handle.RingBuf, Rx.Handle.temp_data);
        HAL_UART_Receive_IT(huart, &Rx.Handle.temp_data, 1);
    }
}
