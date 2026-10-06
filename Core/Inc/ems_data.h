/******************************************************************************
 * @file    ems_data.h
 * @brief   Centralized EMS Data Structure
 * @details Single data structure shared across all EMS modules.
 *          All modules read from or write to this structure.
 *          Future communication interfaces (Modbus RTU, CAN, Ethernet)
 *          should only consume data from this structure.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef EMS_DATA_H
#define EMS_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Defines -------------------------------------------------------------------*/
#define EMS_TOTAL_CELLS          12
#define EMS_TOTAL_GPIOS           5

/* Voltage Thresholds */
#define CELL_OV_THRESHOLD        4.20f    /* Over Voltage threshold (V)  */
#define CELL_UV_THRESHOLD        0.5f     /* Under Voltage threshold (V) */

/* Balancing Thresholds */
#define CELL_BALANCE_THRESHOLD   0.050f   /* Balance diff threshold (V)  */
#define CELL_MIN_VALID_VOLTAGE   1.0f     /* Minimum valid cell voltage   */

/* Temperature Thresholds */
#define IC_TEMP_MAX_BALANCING    60.0f    /* Max IC temp for balancing (C) */

/* Open Wire Detection */
#define OPEN_WIRE_THRESHOLD      0.4f     /* Open wire diff threshold (V) */

/* LTC6811 Configuration Registers
 * Formula to calculate register value from desired voltage threshold:
 * Register Value = Desired Threshold (V) / (16 * 100uV) = Desired Threshold (V) / 0.0016
 */
#define LTC6811_VUV_VALUE        437      /* UV comparison value (0.7V / 0.0016 = 437.5 ~ 437) */
#define LTC6811_VOV_VALUE        2625     /* OV comparison value (4.2V / 0.0016 = 2625)        */

/* NTC Parameters */
#define NTC_BETA                 3950.0f  /* Beta coefficient             */
#define NTC_R_NOMINAL            10000.0f /* Nominal resistance at T0 (Ohm) */
#define NTC_T_NOMINAL            25.0f    /* Nominal temperature (C)      */
#define NTC_R_PULLUP_GPIO        1000000.0f /* GPIO1 pull-up resistor (Ohm) */

/* ADC Conversion Factors
 * These constants convert raw 16-bit register values to actual units (V, C)
 * according to the LTC6811 datasheet specifications.
 */
#define LTC6811_CELL_LSB         0.0001f  /* Cell ADC LSB = 100uV (Raw * 0.0001V)                 */
#define LTC6811_GPIO_LSB         0.0001f  /* AUX ADC LSB = 100uV (Raw * 0.0001V)                  */
#define LTC6811_SOC_LSB          0.002f   /* Sum of Cells LSB = 2mV (Raw * 0.002V)                */
#define LTC6811_ITEMP_SCALE      0.013333f /* Temp scale = 100uV / 7.5mV/C sensor sensitivity    */
#define LTC6811_ITEMP_OFFSET     273.0f   /* Offset to subtract to convert Kelvin to Celsius      */
#define LTC6811_SUPPLY_LSB       0.0001f  /* Supply voltage LSB = 100uV (Raw * 0.0001V)           */
#define LTC6811_UVOV_LSB         0.0016f  /* UV/OV threshold LSB = 16 * 100uV (1.6mV per LSB)     */

/* Exported Types ------------------------------------------------------------*/

/******************************************************************************
 * @brief   Centralized EMS Data Structure
 * @details Contains all measured data, fault flags, and system status.
 *          One global instance is shared across all modules.
 ******************************************************************************/
typedef struct
{
    /* Cell Voltage Measurements */
    float    CellVoltage[EMS_TOTAL_CELLS];   /* Converted cell voltages (V) */
    uint16_t RawCell[EMS_TOTAL_CELLS];       /* Raw ADC values              */

    /* Calibration Coefficients */
    float    CellGain[EMS_TOTAL_CELLS];       /* Cell voltage gains          */
    float    CellOffset[EMS_TOTAL_CELLS];     /* Cell voltage offsets (V)    */

    /* GPIO / Auxiliary Measurements */
    uint16_t RawGPIO[EMS_TOTAL_GPIOS];       /* Raw GPIO ADC values         */
    float    GPIO1Voltage;                    /* GPIO1 voltage (V)           */

    /* Status Register Data */
    float    StackVoltage;                    /* Sum of cells (V)            */
    float    InternalTemp;                    /* IC die temperature (C)      */
    float    AnalogSupply;                    /* Analog supply voltage (V)   */
    float    DigitalSupply;                   /* Digital supply voltage (V)  */
    float    ReferenceVoltage2;               /* REF2 voltage (V)            */

    /* Temperature */
    float    Temperature;                     /* GPIO1 NTC temperature (C)   */
    float    NTCResistance;                   /* Calculated NTC resistance   */

    /* Thresholds (decoded from config register) */
    float    UVThreshold;                     /* Under voltage threshold (V) */
    float    OVThreshold;                     /* Over voltage threshold (V)  */

    /* Fault Flags */
    uint8_t  OVFlag[EMS_TOTAL_CELLS];         /* Over voltage flags          */
    uint8_t  UVFlag[EMS_TOTAL_CELLS];         /* Under voltage flags         */
    uint8_t  OpenWireFlag[EMS_TOTAL_CELLS];   /* Open wire flags             */

    /* Balancing */
    uint8_t  CellBalanceStatus[EMS_TOTAL_CELLS]; /* Per-cell balance status  */
    uint16_t DCCBits;                         /* Current DCC bitmask         */

    /* Communication Health */
    uint32_t PECErrorCount;                   /* PEC error counter           */
    uint8_t  CommunicationStatus;             /* 0=OK, non-zero=error        */
    uint32_t SystemFaultFlags;                /* Bitfield for system faults  */

} EMS_Data_t;

/* Exported Variables --------------------------------------------------------*/

/******************************************************************************
 * @brief   Global EMS Data instance
 * @details Defined in main.c. All modules reference this via extern.
 ******************************************************************************/
extern EMS_Data_t EMS_Data;

#ifdef __cplusplus
}
#endif

#endif /* EMS_DATA_H */
