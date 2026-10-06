/******************************************************************************
 * @file    ems_modbus.c
 * @brief   EMS Modbus RTU Register Mapping Implementation
 * @details Converts all fields from the centralized EMS_Data_t structure
 *          into the Modbus Input Registers database (Function Code 0x04).
 *
 *          Float values are scaled to uint16_t using predefined scale factors:
 *          - Voltages:    ×10000 (0.1mV resolution, max ~6.5535V)
 *          - Stack Volt:  ×1000  (1mV resolution,   max ~65.535V)
 *          - Temperatures:×100   (0.01°C resolution, max ~655.35°C)
 *          - Resistance:  ÷100   (units of 100Ω)
 *          - Flags/raw:   direct copy (0 or 1, or raw 16-bit value)
 *
 *          This module does NOT contain any SPI or UART communication.
 *          It only reads from EMS_Data and writes to Input_Registers_Database[].
 *
 * @version 1.0
 * @date    2026-07-07
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ems_modbus.h"
#include "ems_data.h"

/* External Variables --------------------------------------------------------*/

/*
 * Input_Registers_Database[] is defined in modbusSlave.c
 * It is the array that Modbus FC04 (Read Input Registers) serves to the master.
 */
extern uint16_t Input_Registers_Database[150];

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_UpdateModbusRegisters
 * Description: Copy all EMS_Data_t fields into Input_Registers_Database[]
 *              with appropriate scaling for 16-bit Modbus representation.
 *
 *              This function should be called once per measurement cycle
 *              in the main loop, after EMS_UpdateMeasurements() and all
 *              processing (temperature, faults, balancing) have completed.
 *
 *              Register map (86 registers):
 *                0-11   : Cell voltages      (×10000)
 *                12-23  : Raw ADC values      (direct)
 *                24     : Stack voltage        (×1000)
 *                25     : Internal temperature (×100)
 *                26     : Analog supply        (×10000)
 *                27     : Digital supply       (×10000)
 *                28     : Reference voltage 2  (×10000)
 *                29     : GPIO1 voltage        (×10000)
 *                30     : NTC temperature      (×100)
 *                31     : NTC resistance       (÷100)
 *                32     : UV threshold         (×10000)
 *                33     : OV threshold         (×10000)
 *                34-45  : OV flags             (0/1)
 *                46-57  : UV flags             (0/1)
 *                58-69  : Open wire flags      (0/1)
 *                70-81  : Balance status       (0/1)
 *                82     : DCC bitmask          (12-bit)
 *                83     : PEC error count      (low 16 bits)
 *                84     : Communication status (0=OK)
 *                85     : System fault flags   (low 16 bits)
 *
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_UpdateModbusRegisters(void)
{
	/*
	 * ---- Cell Voltages (Registers 0-11) ----
	 * Scale: ×10000 → 0.1mV resolution
	 * Example: 3.3200V × 10000 = 33200
	 */
	for (int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		Input_Registers_Database[MODBUS_REG_CELL_VOLTAGE + i] =
				(uint16_t)(EMS_Data.CellVoltage[i] * MODBUS_SCALE_VOLTAGE);
	}

	/*
	 * ---- Raw ADC Values (Registers 12-23) ----
	 * Direct copy — already uint16_t
	 */
	for (int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		Input_Registers_Database[MODBUS_REG_RAW_CELL + i] =
				EMS_Data.RawCell[i];
	}

	/*
	 * ---- Status Register Data (Registers 24-28) ----
	 */

	/* Reg 24: Stack Voltage — scale ×1000 (1mV resolution, max 65.535V) */
	Input_Registers_Database[MODBUS_REG_STACK_VOLTAGE] =
			(uint16_t)(EMS_Data.StackVoltage * MODBUS_SCALE_STACK_VOLTAGE);

	/* Reg 25: Internal Temperature — scale ×100 (0.01°C resolution) */
	Input_Registers_Database[MODBUS_REG_INTERNAL_TEMP] =
			(uint16_t)(EMS_Data.InternalTemp * MODBUS_SCALE_TEMPERATURE);

	/* Reg 26: Analog Supply (VREG) — scale ×10000 (0.1mV resolution) */
	Input_Registers_Database[MODBUS_REG_ANALOG_SUPPLY] =
			(uint16_t)(EMS_Data.AnalogSupply * MODBUS_SCALE_VOLTAGE);

	/* Reg 27: Digital Supply (VD) — scale ×10000 (0.1mV resolution) */
	Input_Registers_Database[MODBUS_REG_DIGITAL_SUPPLY] =
			(uint16_t)(EMS_Data.DigitalSupply * MODBUS_SCALE_VOLTAGE);

	/* Reg 28: Reference Voltage 2 — scale ×10000 (0.1mV resolution) */
	Input_Registers_Database[MODBUS_REG_REF_VOLTAGE2] =
			(uint16_t)(EMS_Data.ReferenceVoltage2 * MODBUS_SCALE_VOLTAGE);

	/*
	 * ---- GPIO / Temperature (Registers 29-31) ----
	 */

	/* Reg 29: GPIO1 Voltage — scale ×10000 */
	Input_Registers_Database[MODBUS_REG_GPIO1_VOLTAGE] =
			(uint16_t)(EMS_Data.GPIO1Voltage * MODBUS_SCALE_VOLTAGE);

	/* Reg 30: NTC Temperature — scale ×100 (0.01°C resolution) */
	Input_Registers_Database[MODBUS_REG_TEMPERATURE] =
			(uint16_t)(EMS_Data.Temperature * MODBUS_SCALE_TEMPERATURE);

	/* Reg 31: NTC Resistance — divide by 100 (units of 100Ω) */
	Input_Registers_Database[MODBUS_REG_NTC_RESISTANCE] =
			(uint16_t)(EMS_Data.NTCResistance / MODBUS_SCALE_RESISTANCE_DIV);

	/*
	 * ---- Thresholds (Registers 32-33) ----
	 */

	/* Reg 32: UV Threshold — scale ×10000 */
	Input_Registers_Database[MODBUS_REG_UV_THRESHOLD] =
			(uint16_t)(EMS_Data.UVThreshold * MODBUS_SCALE_VOLTAGE);

	/* Reg 33: OV Threshold — scale ×10000 */
	Input_Registers_Database[MODBUS_REG_OV_THRESHOLD] =
			(uint16_t)(EMS_Data.OVThreshold * MODBUS_SCALE_VOLTAGE);

	/*
	 * ---- Fault Flags (Registers 34-69) ----
	 * Each flag is 0 or 1, stored directly in a 16-bit register
	 */
	for (int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		/* Regs 34-45: Over-voltage flags */
		Input_Registers_Database[MODBUS_REG_OV_FLAGS + i] =
				(uint16_t)EMS_Data.OVFlag[i];

		/* Regs 46-57: Under-voltage flags */
		Input_Registers_Database[MODBUS_REG_UV_FLAGS + i] =
				(uint16_t)EMS_Data.UVFlag[i];

		/* Regs 58-69: Open wire flags */
		Input_Registers_Database[MODBUS_REG_OW_FLAGS + i] =
				(uint16_t)EMS_Data.OpenWireFlag[i];
	}

	/*
	 * ---- Balancing (Registers 70-82) ----
	 */
	for (int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		/* Regs 70-81: Per-cell balance status */
		Input_Registers_Database[MODBUS_REG_BALANCE_STATUS + i] =
				(uint16_t)EMS_Data.CellBalanceStatus[i];
	}

	/* Reg 82: DCC Bitmask — direct 12-bit value */
	Input_Registers_Database[MODBUS_REG_DCC_BITS] =
			EMS_Data.DCCBits;

	/*
	 * ---- Communication Health (Registers 83-85) ----
	 */

	/* Reg 83: PEC Error Count — lower 16 bits of 32-bit counter */
	Input_Registers_Database[MODBUS_REG_PEC_ERROR_COUNT] =
			(uint16_t)(EMS_Data.PECErrorCount & 0xFFFF);

	/* Reg 84: Communication Status — 0=OK, non-zero=error */
	Input_Registers_Database[MODBUS_REG_COMM_STATUS] =
			(uint16_t)EMS_Data.CommunicationStatus;

	/* Reg 85: System Fault Flags — lower 16 bits of 32-bit bitfield */
	Input_Registers_Database[MODBUS_REG_FAULT_FLAGS] =
			(uint16_t)(EMS_Data.SystemFaultFlags & 0xFFFF);
}
