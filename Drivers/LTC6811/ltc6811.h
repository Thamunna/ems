/******************************************************************************
 * @file    ltc6811.h
 * @brief   LTC6811-2 Battery Monitor IC Driver
 * @details Low-level driver for communication with LTC6811-2 via SPI.
 *          Handles wakeup, command generation, PEC, register read/write,
 *          ADC conversion commands, and raw data retrieval.
 *
 *          This driver does NOT contain application logic such as:
 *          - Temperature calculations
 *          - OV/UV detection
 *          - Balancing algorithms
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef LTC6811_H
#define LTC6811_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ems_data.h"
#include <stdint.h>

/* Defines -------------------------------------------------------------------*/

/* Chip Select Macros using CubeMX-generated pin definitions */
#define LTC6811_CS_LOW()    HAL_GPIO_WritePin(CHIP_SELECT_GPIO_Port, CHIP_SELECT_Pin, GPIO_PIN_RESET)
#define LTC6811_CS_HIGH()   HAL_GPIO_WritePin(CHIP_SELECT_GPIO_Port, CHIP_SELECT_Pin, GPIO_PIN_SET)

/* LTC6811 Command Bytes */
#define LTC6811_CMD_RDCFG_0        0x00
#define LTC6811_CMD_RDCFG_1        0x02

#define LTC6811_CMD_WRCFG_0        0x00
#define LTC6811_CMD_WRCFG_1        0x01

#define LTC6811_CMD_RDCVA_0        0x00
#define LTC6811_CMD_RDCVA_1        0x04

#define LTC6811_CMD_RDCVB_0        0x00
#define LTC6811_CMD_RDCVB_1        0x06

#define LTC6811_CMD_RDCVC_0        0x00
#define LTC6811_CMD_RDCVC_1        0x08

#define LTC6811_CMD_RDCVD_0        0x00
#define LTC6811_CMD_RDCVD_1        0x0A

#define LTC6811_CMD_RDAUXA_0       0x00
#define LTC6811_CMD_RDAUXA_1       0x0C

#define LTC6811_CMD_RDSTATA_0      0x00
#define LTC6811_CMD_RDSTATA_1      0x10

#define LTC6811_CMD_RDSTATB_0      0x00
#define LTC6811_CMD_RDSTATB_1      0x12

#define LTC6811_CMD_ADCVSC_0       0x04
#define LTC6811_CMD_ADCVSC_1       0x67

#define LTC6811_CMD_ADSTAT_0       0x04
#define LTC6811_CMD_ADSTAT_1       0x68

#define LTC6811_CMD_ADAX_0         0x05
#define LTC6811_CMD_ADAX_1         0x60

#define LTC6811_CMD_ADOW_PU_0     0x02
#define LTC6811_CMD_ADOW_PU_1     0x28

#define LTC6811_CMD_ADOW_PD_0     0x02
#define LTC6811_CMD_ADOW_PD_1     0x68

/* SPI Timeout */
#define LTC6811_SPI_TIMEOUT        100

/* Wakeup Timing */
#define LTC6811_WAKEUP_US          300
#define LTC6811_WAKEUP_SETTLE_US   10

/* Configuration Register Defaults
 * Bit 2 of Configuration Register Group A is REFON (Reference Remain On).
 * Setting this bit to 1 (1 << 2 = 0x04) ensures the internal voltage reference
 * remains powered on between ADC measurements, eliminating settling delays.
 */
#define LTC6811_CFGR0_REFON        0x04

/* Register Sizes */
#define LTC6811_REG_DATA_SIZE      6
#define LTC6811_REG_FRAME_SIZE     8    /* 6 data + 2 PEC */
#define LTC6811_CMD_SIZE           4    /* 2 cmd + 2 PEC  */

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    LTC6811_Init
 * Description: Initialize the LTC6811-2.
 *              Enables DWT cycle counter for microsecond delays,
 *              performs wakeup, and writes initial configuration
 *              (REFON=1, UV/OV thresholds).
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void LTC6811_Init(void);

/******************************************************************************
 * Function:    LTC6811_Wakeup
 * Description: Wake up the LTC6811 from sleep/standby state.
 *              Toggles CS low for 300us then high for 10us.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void LTC6811_Wakeup(void);

/******************************************************************************
 * Function:    LTC6811_SendCommand
 * Description: Send a 2-byte command with PEC to the LTC6811.
 *              Automatically wakes the IC, asserts CS, transmits
 *              4 bytes (2 cmd + 2 PEC), and de-asserts CS.
 * Parameters:  cmd0 - First command byte
 *              cmd1 - Second command byte
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_SendCommand(uint8_t cmd0, uint8_t cmd1);

/******************************************************************************
 * Function:    LTC6811_ReadRegister
 * Description: Send a read command and receive 8 bytes (6 data + 2 PEC).
 *              Data is stored in the provided buffer.
 * Parameters:  cmd0   - First command byte
 *              cmd1   - Second command byte
 *              rxBuf  - Pointer to 8-byte receive buffer
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadRegister(uint8_t cmd0, uint8_t cmd1, uint8_t *rxBuf);

/******************************************************************************
 * Function:    LTC6811_WriteConfig
 * Description: Write configuration register to the LTC6811.
 *              Sets REFON=1 and UV/OV thresholds.
 *              Does NOT set any DCC (balancing) bits.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_WriteConfig(void);

/******************************************************************************
 * Function:    LTC6811_WriteConfigWithDCC
 * Description: Write configuration register with DCC balancing bits.
 *              Sets REFON=1, UV/OV thresholds, and DCC1-DCC12 bits.
 * Parameters:  dcc_bits - 12-bit bitmask for DCC1-DCC12
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_WriteConfigWithDCC(uint16_t dcc_bits);

/******************************************************************************
 * Function:    LTC6811_ReadConfig
 * Description: Read configuration register from the LTC6811.
 *              Data is stored in the provided buffer (8 bytes).
 * Parameters:  cfgBuf - Pointer to 8-byte buffer for config data
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadConfig(uint8_t *cfgBuf);

/******************************************************************************
 * Function:    LTC6811_StartCellADC_SC
 * Description: Start cell voltage ADC conversion with Sum of Cells.
 *              ADCVSC command: MD=01 (Normal), DCP=0, CH=All.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartCellADC_SC(void);

/******************************************************************************
 * Function:    LTC6811_StartStatusADC
 * Description: Start status group ADC conversion.
 *              ADSTAT command: MD=01 (Normal), CHST=All.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartStatusADC(void);

/******************************************************************************
 * Function:    LTC6811_StartGPIOADC
 * Description: Start GPIO ADC conversion.
 *              ADAX command: MD=01 (Normal), CHG=All.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartGPIOADC(void);

/******************************************************************************
 * Function:    LTC6811_StartADOW
 * Description: Start open wire ADC conversion.
 *              ADOW command with selectable pull-up/pull-down current.
 * Parameters:  pullup - 1 for pull-up test, 0 for pull-down test
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartADOW(uint8_t pullup);

/******************************************************************************
 * Function:    LTC6811_ReadAllCells
 * Description: Start cell ADC conversion and read all 12 cell voltages.
 *              Reads cell register groups A, B, C, D and updates
 *              EMS_Data.CellVoltage[] and EMS_Data.RawCell[].
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadAllCells(void);

/******************************************************************************
 * Function:    LTC6811_ReadStatusA
 * Description: Read Status Register Group A.
 *              Updates EMS_Data: StackVoltage, InternalTemp, AnalogSupply.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadStatusA(void);

/******************************************************************************
 * Function:    LTC6811_ReadStatusB
 * Description: Read Status Register Group B.
 *              Updates EMS_Data: DigitalSupply, ReferenceVoltage2.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadStatusB(void);

/******************************************************************************
 * Function:    LTC6811_ReadGPIO1
 * Description: Start GPIO ADC conversion and read GPIO1 voltage.
 *              Updates EMS_Data.GPIO1Voltage.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadGPIO1(void);

/******************************************************************************
 * Function:    LTC6811_DecodeUVOV
 * Description: Decode UV and OV threshold values from configuration data.
 *              Updates EMS_Data.UVThreshold and EMS_Data.OVThreshold.
 * Parameters:  cfgData - Pointer to 8-byte configuration register data
 * Returns:     None
 ******************************************************************************/
void LTC6811_DecodeUVOV(uint8_t *cfgData);

#ifdef __cplusplus
}
#endif

#endif /* LTC6811_H */
