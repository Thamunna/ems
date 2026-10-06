/******************************************************************************
 * @file    ems_fault.c
 * @brief   EMS Fault Detection Implementation
 * @details Over voltage, under voltage, open wire detection.
 *          All fault flags are stored in the centralized EMS_Data structure.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ems_fault.h"
#include "ltc6811.h"
#include "ems_data.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_CheckFaults
 * Description: Check all cell voltages for over/under voltage conditions.
 *              Updates EMS_Data.OVFlag[] and EMS_Data.UVFlag[].
 *              Only checks cells with valid voltages (>= UV threshold).
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_CheckFaults(void)
{
	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		/*
		 * Skip unused / floating channels
		 */
		if(EMS_Data.CellVoltage[i] < CELL_UV_THRESHOLD)
		{
			EMS_Data.OVFlag[i] = 0;
			EMS_Data.UVFlag[i] = 0;
			continue;
		}

		/*
		 * Over Voltage Check
		 */
		if(EMS_Data.CellVoltage[i] > CELL_OV_THRESHOLD)
		{
			EMS_Data.OVFlag[i] = 1;
		}
		else
		{
			EMS_Data.OVFlag[i] = 0;
		}

		/*
		 * Under Voltage Check
		 */
		if(EMS_Data.CellVoltage[i] < CELL_UV_THRESHOLD)
		{
			EMS_Data.UVFlag[i] = 1;
		}
		else
		{
			EMS_Data.UVFlag[i] = 0;
		}
	}
}

/******************************************************************************
 * Function:    EMS_OpenWireTest
 * Description: Perform open wire detection on all cell connections.
 *              Executes pull-up and pull-down ADOW conversions,
 *              compares results to detect open wires.
 *              Updates EMS_Data.OpenWireFlag[].
 *              Prints results via UART debug.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_OpenWireTest(void)
{
	float pullup_voltage[EMS_TOTAL_CELLS];
	float pulldown_voltage[EMS_TOTAL_CELLS];

	printf("\r\n========== OPEN WIRE TEST ==========\r\n");

	/*
	 * STEP 1
	 * Pull-Up Test
	 */
	LTC6811_StartADOW(1);

	HAL_Delay(10);

	LTC6811_ReadAllCells();

	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		pullup_voltage[i] = EMS_Data.CellVoltage[i];
	}

	/*
	 * STEP 2
	 * Pull-Down Test
	 */
	LTC6811_StartADOW(0);

	HAL_Delay(10);

	LTC6811_ReadAllCells();

	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		pulldown_voltage[i] = EMS_Data.CellVoltage[i];
	}

	/*
	 * STEP 3
	 * Compare Results
	 */
	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		float diff;

		diff = pullup_voltage[i] - pulldown_voltage[i];

		if(diff < 0)
		{
			diff = -diff;
		}

		printf(
				"Cell %02d : PU=%.4fV  PD=%.4fV  DIFF=%.4fV ",
				i + 1,
				pullup_voltage[i],
				pulldown_voltage[i],
				diff
		);

		/*
		 * Open wire detection threshold
		 */
		if(diff > OPEN_WIRE_THRESHOLD)
		{
			EMS_Data.OpenWireFlag[i] = 1;

			printf(" --> OPEN WIRE SUSPECTED");
		}
		else
		{
			EMS_Data.OpenWireFlag[i] = 0;
		}

		printf("\r\n");
	}

	printf("====================================\r\n\r\n");
}
