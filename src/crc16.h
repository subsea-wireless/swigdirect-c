#ifndef CRC16_H_
#define CRC16_H_

#include <stdint.h>

/**
 * @brief Calculates the CRC for a given message block
 * @param message The input message buffer
 * @param message_length The number of bytes in the input message buffer
 * @return The calculated CRC
 * @note This generates a 16-bit CRC using CRC16/CCITT-FALSE
 */
uint16_t Crc16CalculateChecksum(uint8_t* message, uint16_t message_length);

#endif // CRC16_H_
