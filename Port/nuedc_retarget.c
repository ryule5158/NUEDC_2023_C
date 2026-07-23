#include "stm32h7xx_hal.h"
#include <stdio.h>

/* 把标准输出重定向到TI板UART0。 */
int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;

    UNUSED(f);
    (void)HAL_UART_Transmit(&huart2, &c, 1U, 1U);
    return ch;
}

/* 提供无阻塞的标准输入占位接口。 */
int fgetc(FILE *f)
{
    UNUSED(f);
    return 0;
}

/* 在裸机程序退出时保持处理器停留在安全循环。 */
void _sys_exit(int return_code)
{
    UNUSED(return_code);
    while (1) {
        __NOP();
    }
}
