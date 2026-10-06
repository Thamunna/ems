/******************************************************************************
 * @file    ems_modbus.h
 * @brief   EMS Modbus RTU Register Mapping
 * @details Maps all EMS_Data_t fields to the Modbus Input Registers database
 *          (Function Code 0x04). The mapping function converts float values
 *          to scaled uint16_t values suitable for 16-bit Modbus registers.
 *
 *          Register Map (86 registers total):
 *          ┌────────────┬──────────────────────────────┬─────────┐
 *          │ Reg 0-11   │ CellVoltage[0..11]           │ x10000  │
 *          │ Reg 12-23  │ RawCell[0..11]               │ raw     │
 *          │ Reg 24     │ StackVoltage                 │ x1000   │
 *          │ Reg 25     │ InternalTemp                 │ x100    │
 *          │ Reg 26     │ AnalogSupply                 │ x10000  │
 *          │ Reg 27     │ DigitalSupply                │ x10000  │
 *          │ Reg 28     │ ReferenceVoltage2            │ x10000  │
 *          │ Reg 29     │ GPIO1Voltage                 │ x10000  │
 *          │ Reg 30     │ Temperature                  │ x100    │
 *          │ Reg 31     │ NTCResistance                │ /100    │
 *          │ Reg 32     │ UVThreshold                  │ x10000  │
 *          │ Reg 33     │ OVThreshold                  │ x10000  │
 *          │ Reg 34-45  │ OVFlag[0..11]                │ 0/1     │
 *          │ Reg 46-57  │ UVFlag[0..11]                │ 0/1     │
 *          │ Reg 58-69  │ OpenWireFlag[0..11]          │ 0/1     │
 *          │ Reg 70-81  │ CellBalanceStatus[0..11]     │ 0/1     │
 *          │ Reg 82     │ DCCBits                      │ bitmask │
 *          │ Reg 83     │ PECErrorCount (low 16)       │ count   │
 *          │ Reg 84     │ CommunicationStatus          │ 0=OK    │
 *          │ Reg 85     │ SystemFaultFlags (low 16)    │ bitfield│
 *          └────────────┴──────────────────────────────┴─────────┘
 *
 * @version 1.0
 * @date    2026-07-07
 ******************************************************************************/

#ifndef EMS_MODBUS_H
#define EMS_MODBUS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ems_data.h"

/* Defines -------------------------------------------------------------------*/

/* Total number of Modbus Input Registers populated by EMS */
#define EMS_MODBUS_INPUT_REG_COUNT   86

/*
 * Scaling Factors
 *
 * Modbus registers are 16-bit unsigned integers.
 * Float values are multiplied by a scale factor before storing.
 * The Modbus master must divide by the same factor to recover the actual value.
 *
 * Example: CellVoltage = 3.3200V → Register = 3.3200 × 10000 = 33200
 *          Master reads 33200 → 33200 / 10000 = 3.3200V
 */
#define MODBUS_SCALE_VOLTAGE         10000  /* 0.1mV resolution (max ~6.5535V)   */
#define MODBUS_SCALE_STACK_VOLTAGE   1000   /* 1mV resolution   (max ~65.535V)   */
#define MODBUS_SCALE_TEMPERATURE     100    /* 0.01°C resolution (max ~655.35°C) */
#define MODBUS_SCALE_RESISTANCE_DIV  100    /* Divide by 100 (units of 100Ω)     */

/* Register Address Offsets (0-based index into Input_Registers_Database[]) */
#define MODBUS_REG_CELL_VOLTAGE      0      /* Regs 0-11:  CellVoltage[0..11]      */
#define MODBUS_REG_RAW_CELL          12     /* Regs 12-23: RawCell[0..11]           */
#define MODBUS_REG_STACK_VOLTAGE     24     /* Reg 24:     StackVoltage             */
#define MODBUS_REG_INTERNAL_TEMP     25     /* Reg 25:     InternalTemp             */
#define MODBUS_REG_ANALOG_SUPPLY     26     /* Reg 26:     AnalogSupply             */
#define MODBUS_REG_DIGITAL_SUPPLY    27     /* Reg 27:     DigitalSupply            */
#define MODBUS_REG_REF_VOLTAGE2      28     /* Reg 28:     ReferenceVoltage2        */
#define MODBUS_REG_GPIO1_VOLTAGE     29     /* Reg 29:     GPIO1Voltage             */
#define MODBUS_REG_TEMPERATURE       30     /* Reg 30:     Temperature              */
#define MODBUS_REG_NTC_RESISTANCE    31     /* Reg 31:     NTCResistance            */
#define MODBUS_REG_UV_THRESHOLD      32     /* Reg 32:     UVThreshold              */
#define MODBUS_REG_OV_THRESHOLD      33     /* Reg 33:     OVThreshold              */
#define MODBUS_REG_OV_FLAGS          34     /* Regs 34-45: OVFlag[0..11]            */
#define MODBUS_REG_UV_FLAGS          46     /* Regs 46-57: UVFlag[0..11]            */
#define MODBUS_REG_OW_FLAGS          58     /* Regs 58-69: OpenWireFlag[0..11]      */
#define MODBUS_REG_BALANCE_STATUS    70     /* Regs 70-81: CellBalanceStatus[0..11] */
#define MODBUS_REG_DCC_BITS          82     /* Reg 82:     DCCBits                  */
#define MODBUS_REG_PEC_ERROR_COUNT   83     /* Reg 83:     PECErrorCount (low 16)   */
#define MODBUS_REG_COMM_STATUS       84     /* Reg 84:     CommunicationStatus      */
#define MODBUS_REG_FAULT_FLAGS       85     /* Reg 85:     SystemFaultFlags (low 16)*/

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_UpdateModbusRegisters
 * Description: Copy all EMS_Data_t fields into the Modbus Input Registers
 *              database with appropriate scaling for 16-bit representation.
 *              Call this once per measurement cycle before Modbus responses
 *              are served. The function is non-blocking and interrupt-safe
 *              (writes to the register array atomically per-field).
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_UpdateModbusRegisters(void);

#ifdef __cplusplus
}
#endif

#endif /* EMS_MODBUS_H */
