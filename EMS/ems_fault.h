/******************************************************************************
 * @file    ems_fault.h
 * @brief   EMS Fault Detection
 * @details Over voltage, under voltage, open wire detection,
 *          PEC error handling, and communication fault monitoring.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef EMS_FAULT_H
#define EMS_FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ems_data.h"

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_CheckFaults
 * Description: Check all cell voltages for over/under voltage conditions.
 *              Updates EMS_Data.OVFlag[] and EMS_Data.UVFlag[].
 *              Only checks cells with valid voltages (>= UV threshold).
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_CheckFaults(void);

/******************************************************************************
 * Function:    EMS_OpenWireTest
 * Description: Perform open wire detection on all cell connections.
 *              Executes pull-up and pull-down ADOW conversions,
 *              compares results to detect open wires.
 *              Updates EMS_Data.OpenWireFlag[].
 *              Prints results via USB CDC debug output.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_OpenWireTest(void);

#ifdef __cplusplus
}
#endif

#endif /* EMS_FAULT_H */
