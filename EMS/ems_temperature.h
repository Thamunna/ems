/******************************************************************************
 * @file    ems_temperature.h
 * @brief   EMS Temperature Calculation
 * @details Converts GPIO voltage readings to NTC temperature using
 *          the Beta equation. No SPI communication.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef EMS_TEMPERATURE_H
#define EMS_TEMPERATURE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ems_data.h"

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_UpdateTemperature
 * Description: Calculate NTC temperature from GPIO1 voltage.
 *              Uses the voltage divider and Beta equation to convert
 *              GPIO1 voltage to temperature in Celsius.
 *              Updates EMS_Data.Temperature and EMS_Data.NTCResistance.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_UpdateTemperature(void);

/******************************************************************************
 * Function:    EMS_CalcNTCTemperature
 * Description: Calculate NTC temperature from GPIO voltage and supply.
 *              Standalone calculation function for reuse.
 *              Circuit: VREG --- R_PULLUP --- GPIO --- NTC --- V-
 * Parameters:  gpio_voltage   - Measured GPIO voltage (V)
 *              supply_voltage - Supply voltage / VREG (V)
 * Returns:     float - Temperature in Celsius
 ******************************************************************************/
float EMS_CalcNTCTemperature(float gpio_voltage, float supply_voltage);

#ifdef __cplusplus
}
#endif

#endif /* EMS_TEMPERATURE_H */
