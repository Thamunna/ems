/******************************************************************************
 * @file    crc15.h
 * @brief   PEC15 CRC Calculation for LTC6811
 * @details Implements the 15-bit Packet Error Code used by the LTC6811
 *          for data integrity verification on SPI communications.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef CRC15_H
#define CRC15_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    pec15_calc
 * Description: Calculate PEC15 checksum for LTC6811 communication.
 *              Uses polynomial 0x4599 with initial remainder of 16.
 * Parameters:  len  - Number of bytes in data buffer
 *              data - Pointer to data buffer
 * Returns:     16-bit PEC value (15-bit CRC left-shifted by 1)
 ******************************************************************************/
uint16_t pec15_calc(uint8_t len, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* CRC15_H */
