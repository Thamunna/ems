/******************************************************************************
 * @file    usb_debug.h
 * @brief   USB Debug Print Utilities
 * @details Helper functions for printing EMS diagnostic data.
 *          Uses printf() which routes through USB CDC via _write().
 *          No application logic — only data formatting and output.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef USB_DEBUG_H
#define USB_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ems_data.h"

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    USB_PrintCellVoltages
 * Description: Print all 12 cell voltages and total stack voltage.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintCellVoltages(const EMS_Data_t *data);

/******************************************************************************
 * Function:    USB_PrintStatusA
 * Description: Print Status A register data:
 *              Sum of Cells, Internal Temperature, Analog Supply.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintStatusA(const EMS_Data_t *data);

/******************************************************************************
 * Function:    USB_PrintStatusB
 * Description: Print Status B register data:
 *              Digital Supply, REF2 Voltage.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintStatusB(const EMS_Data_t *data);

/******************************************************************************
 * Function:    USB_PrintGPIO1
 * Description: Print GPIO1 voltage value.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintGPIO1(const EMS_Data_t *data);

/******************************************************************************
 * Function:    USB_PrintGPIO1Temperature
 * Description: Print GPIO1 voltage and NTC temperature.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintGPIO1Temperature(const EMS_Data_t *data);

/******************************************************************************
 * Function:    USB_PrintUVOV
 * Description: Print UV and OV threshold values.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintUVOV(const EMS_Data_t *data);

/******************************************************************************
 * Function:    USB_PrintFaultStatus
 * Description: Print OV/UV fault flags for all cells.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintFaultStatus(const EMS_Data_t *data);

/******************************************************************************
 * Function:    USB_PrintDebugMessage
 * Description: Print a generic debug message string.
 * Parameters:  msg - Pointer to null-terminated message string
 * Returns:     None
 ******************************************************************************/
void USB_PrintDebugMessage(const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* USB_DEBUG_H */
