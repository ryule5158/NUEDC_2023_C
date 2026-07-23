#include "Serial_Buffer.h"

#define COMMAND_MIN_LENGTH 4U   /* 一条合法指令的最小字节数。 */
#define BUFFER_SIZE 128U        /* 串口指令循环缓冲区容量。 */

static uint8_t buffer[BUFFER_SIZE]; /* 串口指令循环缓冲区。 */
static uint8_t readIndex;           /* 下一待读取位置。 */
static uint8_t writeIndex;          /* 下一待写入位置。 */

/**
* @brief 增加读索引
* @param length 要增加的长度
*/
static void Command_AddReadIndex(uint8_t length)
{
    readIndex += length;
    readIndex %= BUFFER_SIZE;
}

/**
* @brief 读取第i位数据 超过缓存区长度自动循环
* @param i 要读取的数据索引
*/

static uint8_t Command_Read(uint8_t i)
{
    uint8_t index = i % BUFFER_SIZE;
    return buffer[index];
}

/**
* @brief 计算未处理的数据长度
* @return 未处理的数据长度
*/
static uint8_t Command_GetLength(void)
{
    return (writeIndex + BUFFER_SIZE - readIndex) % BUFFER_SIZE;
}


/**
* @brief 计算缓冲区剩余空间
* @return 剩余空间
* @retval 0 缓冲区已满
* @retval 1~BUFFER_SIZE-1 剩余空间
* @retval BUFFER_SIZE 缓冲区为空
*/
static uint8_t Command_GetRemain(void)
{
    return BUFFER_SIZE - Command_GetLength();
}

/**
* @brief 向缓冲区写入数据
* @param data 要写入的数据指针
* @param length 要写入的数据长度
* @return 写入的数据长度
*/
uint8_t Command_Write(uint8_t *data, uint8_t length)
{
    // 如果缓冲区不足 则不写入数据 返回0
    if (Command_GetRemain() < length) {
        return 0;
    }
    // 使用memcpy函数将数据写入缓冲区
    if (writeIndex + length < BUFFER_SIZE) {
        memcpy(buffer + writeIndex, data, length);
        writeIndex += length;
    } else {
        uint8_t firstLength = BUFFER_SIZE - writeIndex;
        memcpy(buffer + writeIndex, data, firstLength);
        memcpy(buffer, data + firstLength, length - firstLength);
        writeIndex = length - firstLength;
    }
    return length;
}

/**
* @brief 尝试获取一条指令
* @param command 指令存放指针
* @return 获取的指令长度
* @retval 0 没有获取到指令
*/
uint8_t Command_GetCommand(uint8_t *command)
{
    // 寻找完整指令
    while (1) {
        // 如果缓冲区长度小于COMMAND_MIN_LENGTH 则不可能有完整的指令
        if (Command_GetLength() < COMMAND_MIN_LENGTH) {
        return 0;
        }
        // 如果不是包头 则跳过 重新开始寻找
        if (Command_Read(readIndex) != 0xAA) {
        Command_AddReadIndex(1);
        continue;
        }
        // 如果缓冲区长度小于指令长度 则不可能有完整的指令
        uint8_t length = Command_Read(readIndex + 1);
        if (Command_GetLength() < length) {
        return 0;
        }
        // 如果校验和不正确 则跳过 重新开始寻找
        uint8_t sum = 0;
        for (uint8_t i = 0; i < length - 1; i++) {
        sum += Command_Read(readIndex + i);
        }
        if (sum != Command_Read(readIndex + length - 1)) {
        Command_AddReadIndex(1);
        continue;
        }
        // 如果找到完整指令 则将指令写入command 返回指令长度
        for (uint8_t i = 0; i < length; i++) {
        command[i] = Command_Read(readIndex + i);
        }
        Command_AddReadIndex(length);
        return length;
    }
}
