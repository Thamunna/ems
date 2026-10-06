/******************************************************************************
 * @file    ems_temperature.c
 * @brief   EMS Temperature Calculation Implementation
 * @details Converts GPIO voltage readings to NTC temperature using
 *          the Beta equation. No SPI communication — pure math.
 *
 *          Circuit: VREG --- 1M Pull-up --- GPIO1 --- NTC --- V-
 *          NTC: 10K nominal, Beta = 3950
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ems_temperature.h"
#include "ems_data.h"
#include <math.h>

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_CalcNTCTemperature
 * Description: Calculate NTC temperature from GPIO voltage and supply.
 *              Standalone calculation function for reuse.
 *              Circuit: VREG --- R_PULLUP --- GPIO --- NTC --- V-
 * Parameters:  gpio_voltage   - Measured GPIO voltage (V)
 *              supply_voltage - Supply voltage / VREG (V)
 * Returns:     float - Temperature in Celsius
 ******************************************************************************/
float EMS_CalcNTCTemperature(float gpio_voltage, float supply_voltage)
{
	float steinhart;
	float ntc_resistance;

	/*
	 * Convert GPIO Voltage to NTC Resistance
	 *
	 * VREG --- 1M --- GPIO1 --- NTC --- V-
	 */
	ntc_resistance =
			(NTC_R_PULLUP_GPIO * gpio_voltage) /
			(supply_voltage - gpio_voltage);

	/*
	 * Beta Equation
	 *
	 * 10K NTC
	 * Beta = 3950
	 */
	steinhart = ntc_resistance / NTC_R_NOMINAL;
	steinhart = logf(steinhart);
	steinhart /= NTC_BETA;
	steinhart += 1.0f / (NTC_T_NOMINAL + 273.15f);
	steinhart = 1.0f / steinhart;
	steinhart -= 273.15f;

	return steinhart;
}

/******************************************************************************
 * Function:    EMS_UpdateTemperature
 * Description: Calculate NTC temperature from GPIO1 voltage.
 *              Uses the voltage divider and Beta equation to convert
 *              GPIO1 voltage to temperature in Celsius.
 *              Updates EMS_Data.Temperature and EMS_Data.NTCResistance.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_UpdateTemperature(void)
{
	/*
	 * GPIO1 Voltage should already be updated by:
	 * EMS_UpdateMeasurements() -> LTC6811_ReadGPIO1()
	 */

	/*
	 * Calculate NTC resistance for diagnostics
	 */
	EMS_Data.NTCResistance =
			(NTC_R_PULLUP_GPIO * EMS_Data.GPIO1Voltage) /
			(EMS_Data.AnalogSupply - EMS_Data.GPIO1Voltage);

	/*
	 * Calculate temperature using measured VREG (AnalogSupply)
	 */
	EMS_Data.Temperature = EMS_CalcNTCTemperature(
			EMS_Data.GPIO1Voltage,
			EMS_Data.AnalogSupply
	);
}
