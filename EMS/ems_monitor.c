/******************************************************************************
 * @file    ems_monitor.c
 * @brief   EMS Measurement Monitor Implementation
 * @details Orchestrates the measurement sequence by calling LTC6811 driver
 *          functions in the correct order with appropriate delays.
 *          All data is stored in the centralized EMS_Data structure.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ems_monitor.h"
#include "ltc6811.h"
#include "stm32f4xx_hal.h"

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_UpdateMeasurements
 * Description: Read all cell voltages, status registers, and GPIO values.
 *              Calls LTC6811 driver functions in the correct sequence
 *              with appropriate delays between conversions.
 *              Updates EMS_Data: CellVoltage[], StackVoltage, InternalTemp,
 *              AnalogSupply, DigitalSupply, ReferenceVoltage2, GPIO1Voltage.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_UpdateMeasurements(void)
{
	/*
	 * STEP 1
	 * Read accurate cell voltages
	 */
	LTC6811_ReadAllCells();

	HAL_Delay(20);

	/*
	 * STEP 2
	 * Read status ADC
	 */
	LTC6811_StartStatusADC();

	HAL_Delay(20);

	LTC6811_ReadStatusA();

	HAL_Delay(10);

	LTC6811_ReadStatusB();

	HAL_Delay(10);

	/*
	 * STEP 3
	 * Read GPIO1 voltage
	 */
	LTC6811_ReadGPIO1();
}
