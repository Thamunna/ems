/******************************************************************************
 * @file    ems_balancing.c
 * @brief   EMS Passive Cell Balancing Implementation
 * @details Implements passive cell balancing algorithm.
 *          Selects cells for discharge based on voltage imbalance,
 *          then writes DCC bits to LTC6811 configuration register.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ems_balancing.h"
#include "ltc6811.h"
#include "ems_data.h"
#include <stdio.h>
#include <string.h>

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_CellBalancing
 * Description: Run passive cell balancing algorithm.
 *              1. Check IC temperature safety (>60C blocks balancing)
 *              2. Find minimum valid cell voltage (>2.5V)
 *              3. Select cells with >50mV imbalance for discharge
 *              4. Write DCC bits to LTC6811 configuration register
 *              Updates EMS_Data.CellBalanceStatus[] and EMS_Data.DCCBits.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_CellBalancing(void)
{
	float min_voltage;
	float diff;

	uint16_t dcc_bits = 0;

	uint8_t valid_cell_found = 0;

	/*
	 * Safety:
	 * Stop balancing if IC temperature too high
	 */
	if(EMS_Data.InternalTemp > IC_TEMP_MAX_BALANCING)
	{
		printf("Balancing blocked : High Temperature\r\n");

		EMS_StopBalancing();

		return;
	}

	/*
	 * STEP 1
	 * Find minimum valid cell voltage
	 *
	 * Ignore:
	 * - unused channels
	 * - floating channels
	 * - invalid readings
	 */
	min_voltage = 100.0f;

	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		/*
		 * Ignore unused cells
		 */
		if(EMS_Data.CellVoltage[i] < CELL_MIN_VALID_VOLTAGE)
		{
			continue;
		}

		if(EMS_Data.CellVoltage[i] < min_voltage)
		{
			min_voltage = EMS_Data.CellVoltage[i];

			valid_cell_found = 1;
		}
	}

	/*
	 * No valid cells found
	 */
	if(valid_cell_found == 0)
	{
		printf("No valid cells found\r\n");

		EMS_StopBalancing();

		return;
	}

	/*
	 * STEP 2
	 * Select cells for balancing
	 */
	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		/*
		 * Ignore unused cells
		 */
		if(EMS_Data.CellVoltage[i] < CELL_MIN_VALID_VOLTAGE)
		{
			EMS_Data.CellBalanceStatus[i] = 0;
			continue;
		}

		/*
		 * Voltage difference from minimum cell
		 */
		diff = EMS_Data.CellVoltage[i] - min_voltage;

		/*
		 * Balance only if difference exceeds threshold
		 *
		 * 50mV threshold recommended for lab testing
		 */
		if(diff > CELL_BALANCE_THRESHOLD)
		{
			dcc_bits |= (1 << i);

			EMS_Data.CellBalanceStatus[i] = 1;

			printf(
					"Balancing Cell %02d  Voltage=%.4fV  Diff=%.4fV\r\n",
					i + 1,
					EMS_Data.CellVoltage[i],
					diff
			);
		}
		else
		{
			EMS_Data.CellBalanceStatus[i] = 0;
		}
	}

	/*
	 * STEP 3
	 * If no balancing required
	 */
	if(dcc_bits == 0)
	{
		printf("Cells already balanced\r\n");

		EMS_StopBalancing();

		return;
	}

	/*
	 * STEP 4
	 * Write configuration with DCC bits
	 */
	LTC6811_WriteConfigWithDCC(dcc_bits);

	EMS_Data.DCCBits = dcc_bits;

	/*
	 * Print balancing mask
	 */
	printf(
			"Balancing Enabled  DCC = 0x%03X\r\n",
			dcc_bits
	);
}

/******************************************************************************
 * Function:    EMS_StopBalancing
 * Description: Stop all cell balancing by clearing DCC bits.
 *              Writes configuration with DCC=0 to the LTC6811.
 *              Updates EMS_Data.CellBalanceStatus[] and EMS_Data.DCCBits.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_StopBalancing(void)
{
	/*
	 * Write config with no DCC bits
	 */
	LTC6811_WriteConfigWithDCC(0x0000);

	EMS_Data.DCCBits = 0;

	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		EMS_Data.CellBalanceStatus[i] = 0;
	}

	printf("Balancing Stopped\r\n");
}
