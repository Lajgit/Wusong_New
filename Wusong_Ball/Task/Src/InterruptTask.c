#include "InterruptTask.h"
#include "CommTask.h"

extern Rx_HandleTypeDef Rx;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == Rx.Handle.huart)
    {
        /* 中文注释：USART1每收到1字节写入环形缓冲区，并立即继续接收下一字节。 */
        Rx.Handle.RingBuf.f_WriteByte(&Rx.Handle.RingBuf, Rx.Handle.temp_data);
        HAL_UART_Receive_IT(huart, &Rx.Handle.temp_data, 1);
    }
}

/* 错误后丢弃当前未完成帧并重新挂接单字节接收，等待下一帧头重同步。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == Rx.Handle.huart)
    {
        HAL_UART_AbortReceive(huart);
        Rx.State = WAIT_HEAD;
        Rx.Queue.Index = 0U;
        HAL_UART_Receive_IT(huart, &Rx.Handle.temp_data, 1U);
    }
}
