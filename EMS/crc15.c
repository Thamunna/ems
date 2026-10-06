/******************************************************************************
 * @file    crc15.c
 * @brief   PEC15 CRC Calculation for LTC6811
 * @details Bit-by-bit PEC15 calculation using polynomial 0x4599.
 *          This is the exact algorithm from the LTC6811 datasheet.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "crc15.h"

/* Private Defines -----------------------------------------------------------*/
#define PEC15_INITIAL_REMAINDER  16
#define PEC15_POLYNOMIAL         0x4599

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    pec15_calc
 * Description: Calculate PEC15 checksum for LTC6811 communication.
 *              Uses polynomial 0x4599 with initial remainder of 16.
 * Parameters:  len  - Number of bytes in data buffer
 *              data - Pointer to data buffer
 * Returns:     16-bit PEC value (15-bit CRC left-shifted by 1)
 ******************************************************************************/
uint16_t pec15_calc(uint8_t len, uint8_t *data)
{
	uint16_t remainder = PEC15_INITIAL_REMAINDER;
	uint16_t polynomial = PEC15_POLYNOMIAL;

	for(uint8_t i = 0; i < len; i++)
	{
		remainder ^= (uint16_t)data[i] << 7;

		for(uint8_t bit = 0; bit < 8; bit++)
		{
			if(remainder & 0x4000)
			{
				remainder = (remainder << 1) ^ polynomial;
			}
			else
			{
				remainder = (remainder << 1);
			}
		}
	}

	return (remainder << 1);
}
