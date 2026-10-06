/******************************************************************************
 * @file    ems_monitor.h
 * @brief   EMS Measurement Monitor
 * @details Orchestrates reading all cell voltages, GPIO values,
 *          and status registers from the LTC6811 driver.
 *          Updates the centralized EMS_Data structure.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef EMS_MONITOR_H
#define EMS_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ems_data.h"

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
void EMS_UpdateMeasurements(void);

#ifdef __cplusplus
}
#endif

#endif /* EMS_MONITOR_H */
