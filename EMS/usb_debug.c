/******************************************************************************
 * @file    usb_debug.c
 * @brief   USB Debug Print Utilities Implementation
 * @details Formatted output of EMS diagnostic data via printf().
 *          printf() routes through USB CDC via the _write() override
 *          in main.c. No application logic — only formatting.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "usb_debug.h"
#include "ems_data.h"
#include <stdio.h>

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    USB_PrintCellVoltages
 * Description: Print all 12 cell voltages and total stack voltage.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintCellVoltages(const EMS_Data_t *data)
{
	float total_voltage = 0.0f;

	printf("\r\n========== CELL VOLTAGES ==========\r\n");

	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		printf(
				"Cell %02d : %.4f V\r\n",
				i + 1,
				data->CellVoltage[i]
		);

		total_voltage += data->CellVoltage[i];
	}

	printf("-----------------------------------\r\n");

	printf(
			"TOTAL STACK VOLTAGE : %.4f V\r\n",
			total_voltage
	);

	printf("===================================\r\n\r\n");
}

/******************************************************************************
 * Function:    USB_PrintStatusA
 * Description: Print Status A register data:
 *              Sum of Cells, Internal Temperature, Analog Supply.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintStatusA(const EMS_Data_t *data)
{
	printf("\r\n========== STATUS A ==========\r\n");

	printf(
			"Sum Of Cells    : %.4f V\r\n",
			data->StackVoltage
	);

	printf(
			"Internal Temp   : %.4f\r\n",
			data->InternalTemp
	);

	printf(
			"Analog Supply   : %.4f V\r\n",
			data->AnalogSupply
	);

	printf("================================\r\n\r\n");
}

/******************************************************************************
 * Function:    USB_PrintStatusB
 * Description: Print Status B register data:
 *              Digital Supply, REF2 Voltage.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintStatusB(const EMS_Data_t *data)
{
	printf("\r\n========== STATUS B ==========\r\n");

	printf(
			"Digital Supply  : %.4f V\r\n",
			data->DigitalSupply
		);

	printf(
			"REF2 Voltage    : %.4f V\r\n",
			data->ReferenceVoltage2
	);

	printf("================================\r\n\r\n");
}

/******************************************************************************
 * Function:    USB_PrintGPIO1
 * Description: Print GPIO1 voltage value.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintGPIO1(const EMS_Data_t *data)
{
	printf(
			"GPIO1 Voltage : %.4f V\r\n",
			data->GPIO1Voltage
	);
}

/******************************************************************************
 * Function:    USB_PrintGPIO1Temperature
 * Description: Print GPIO1 voltage and NTC temperature.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 ******************************************************************************/
void USB_PrintGPIO1Temperature(const EMS_Data_t *data)
{
	printf(
			"GPIO1 : %.4f V  Temp : %.2f C\r\n",
			data->GPIO1Voltage,
			data->Temperature
	);
}

/******************************************************************************
 * Function:    USB_PrintUVOV
 * Description: Print UV and OV threshold values.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 *************************************************----------------*************/
void USB_PrintUVOV(const EMS_Data_t *data)
{
	printf("\r\n========== UV / OV ==========\r\n");

	printf("UV Threshold : %.4f V\r\n", data->UVThreshold);

	printf("OV Threshold : %.4f V\r\n", data->OVThreshold);

	printf("================================\r\n\r\n");
}

/******************************************************************************
 * Function:    USB_PrintFaultStatus
 * Description: Print OV/UV fault flags for all cells.
 * Parameters:  data - Pointer to EMS_Data structure
 * Returns:     None
 *************************************************----------------*************/
void USB_PrintFaultStatus(const EMS_Data_t *data)
{
	printf("\r\n========== FAULT STATUS ==========\r\n");

	for(int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		if(data->OVFlag[i] || data->UVFlag[i] || data->OpenWireFlag[i])
		{
			printf(
					"Cell %02d : OV=%d  UV=%d  OW=%d\r\n",
					i + 1,
					data->OVFlag[i],
					data->UVFlag[i],
					data->OpenWireFlag[i]
			);
		}
	}

	printf("==================================\r\n\r\n");
}

/******************************************************************************
 * Function:    USB_PrintDebugMessage
 * Description: Print a generic debug message string.
 * Parameters:  msg - Pointer to null-terminated message string
 * Returns:     None
 *************************************************----------------*************/
void USB_PrintDebugMessage(const char *msg)
{
	printf("%s\r\n", msg);
}
