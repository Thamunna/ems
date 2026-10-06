/******************************************************************************
 * @file    ltc6811.c
 * @brief   LTC6811-2 Battery Monitor IC Driver Implementation
 * @details Low-level SPI communication driver for LTC6811-2.
 *          All register access, command generation, and PEC handling.
 *          No application logic — only hardware communication.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ltc6811.h"
#include "crc15.h"
#include "ems_data.h"
#include <string.h>
#include <stdio.h>

/* External Variables --------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1;

/* Private Functions ---------------------------------------------------------*/

/******************************************************************************
 * Function:    delay_us
 * Description: Microsecond delay using DWT cycle counter.
 *              Requires DWT to be enabled before first use.
 * Parameters:  us - Number of microseconds to delay
 * Returns:     None
 ******************************************************************************/
static void delay_us(uint16_t us)
{
	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * (SystemCoreClock / 1000000);

	while((DWT->CYCCNT - start) < ticks);
}

/******************************************************************************
 * Function:    LTC6811_DecodeCellGroup
 * Description: Decode 3 cell voltages from a 6-byte register group.
 *              Updates EMS_Data.CellVoltage[] and EMS_Data.RawCell[].
 * Parameters:  rxBuf      - Pointer to 8-byte receive buffer
 *              startIndex - Starting cell index (0, 3, 6, or 9)
 * Returns:     None
 ******************************************************************************/
static void LTC6811_DecodeCellGroup(uint8_t *rxBuf, uint8_t startIndex)
{
	uint16_t raw;
	float raw_voltage;

	raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
	EMS_Data.RawCell[startIndex] = raw;
	raw_voltage = raw * LTC6811_CELL_LSB;
	EMS_Data.CellVoltage[startIndex] = (raw_voltage * EMS_Data.CellGain[startIndex]) + EMS_Data.CellOffset[startIndex];

	raw = ((uint16_t)rxBuf[3] << 8) | rxBuf[2];
	EMS_Data.RawCell[startIndex + 1] = raw;
	raw_voltage = raw * LTC6811_CELL_LSB;
	EMS_Data.CellVoltage[startIndex + 1] = (raw_voltage * EMS_Data.CellGain[startIndex + 1]) + EMS_Data.CellOffset[startIndex + 1];

	raw = ((uint16_t)rxBuf[5] << 8) | rxBuf[4];
	EMS_Data.RawCell[startIndex + 2] = raw;
	raw_voltage = raw * LTC6811_CELL_LSB;
	EMS_Data.CellVoltage[startIndex + 2] = (raw_voltage * EMS_Data.CellGain[startIndex + 2]) + EMS_Data.CellOffset[startIndex + 2];
}

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
void LTC6811_Init(void)
{
	/* Enable DWT cycle counter for microsecond delays */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	/* Initialize default calibration factors (Gain = 1.0, Offset = 0.0V) */
	for (int i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		EMS_Data.CellGain[i] = 1.0f;
		EMS_Data.CellOffset[i] = 0.0f;
	}

	/* Write initial configuration */
	LTC6811_WriteConfig();
}

/******************************************************************************
 * Function:    LTC6811_Wakeup
 * Description: Wake up the LTC6811 from sleep/standby state.
 *              Toggles CS low for 300us then high for 10us.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void LTC6811_Wakeup(void)
{
	LTC6811_CS_LOW();

	delay_us(LTC6811_WAKEUP_US);

	LTC6811_CS_HIGH();

	delay_us(LTC6811_WAKEUP_SETTLE_US);
}

/******************************************************************************
 * Function:    LTC6811_SendCommand
 * Description: Send a 2-byte command with PEC to the LTC6811.
 *              Automatically wakes the IC, asserts CS, transmits
 *              4 bytes (2 cmd + 2 PEC), and de-asserts CS.
 * Parameters:  cmd0 - First command byte
 *              cmd1 - Second command byte
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_SendCommand(uint8_t cmd0, uint8_t cmd1)
{
	uint8_t cmd[LTC6811_CMD_SIZE];
	uint16_t pec;
	HAL_StatusTypeDef status;

	cmd[0] = cmd0;
	cmd[1] = cmd1;

	pec = pec15_calc(2, cmd);

	cmd[2] = (uint8_t)(pec >> 8);
	cmd[3] = (uint8_t)(pec);

	LTC6811_Wakeup();

	LTC6811_CS_LOW();

	status = HAL_SPI_Transmit(&hspi1, cmd, LTC6811_CMD_SIZE, LTC6811_SPI_TIMEOUT);

	LTC6811_CS_HIGH();

	return status;
}

/******************************************************************************
 * Function:    LTC6811_ReadRegister
 * Description: Send a read command and receive 8 bytes (6 data + 2 PEC).
 *              Data is stored in the provided buffer.
 * Parameters:  cmd0   - First command byte
 *              cmd1   - Second command byte
 *              rxBuf  - Pointer to 8-byte receive buffer
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadRegister(uint8_t cmd0, uint8_t cmd1, uint8_t *rxBuf)
{
	uint8_t cmd[LTC6811_CMD_SIZE];
	uint16_t pec;
	HAL_StatusTypeDef status;

	uint8_t dummy_tx[LTC6811_REG_FRAME_SIZE] = {0};

	cmd[0] = cmd0;
	cmd[1] = cmd1;

	pec = pec15_calc(2, cmd);

	cmd[2] = (uint8_t)(pec >> 8);
	cmd[3] = (uint8_t)(pec);

	LTC6811_Wakeup();

	LTC6811_CS_LOW();

	status = HAL_SPI_Transmit(&hspi1, cmd, LTC6811_CMD_SIZE, LTC6811_SPI_TIMEOUT);

	if(status == HAL_OK)
	{
		status = HAL_SPI_TransmitReceive(
				&hspi1,
				dummy_tx,
				rxBuf,
				LTC6811_REG_FRAME_SIZE,
				LTC6811_SPI_TIMEOUT
		);
	}

	LTC6811_CS_HIGH();

	/* Debug: print raw register data */
	printf(
			"RAW: %02X %02X %02X %02X %02X %02X\r\n",
			rxBuf[0],
			rxBuf[1],
			rxBuf[2],
			rxBuf[3],
			rxBuf[4],
			rxBuf[5]
	);

	return status;
}

/******************************************************************************
 * Function:    LTC6811_WriteConfig
 * Description: Write configuration register to the LTC6811.
 *              Sets REFON=1 and UV/OV thresholds.
 *              Does NOT set any DCC (balancing) bits.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_WriteConfig(void)
{
	uint8_t tx_data[LTC6811_REG_FRAME_SIZE];
	HAL_StatusTypeDef status;

	uint16_t vuv = LTC6811_VUV_VALUE;
	uint16_t vov = LTC6811_VOV_VALUE;

	/*
	 * CFGR0
	 * REFON = 1
	 */
	tx_data[0] = LTC6811_CFGR0_REFON;

	/*
	 * CFGR1
	 */
	tx_data[1] = 0x00;

	/*
	 * UV/OV packing
	 */
	tx_data[2] = (uint8_t)(vuv & 0xFF);

	tx_data[3] =
			(uint8_t)((vuv >> 8) & 0x0F) |
			(uint8_t)((vov & 0x0F) << 4);

	tx_data[4] = (uint8_t)(vov >> 4);

	/*
	 * CFGR5
	 */
	tx_data[5] = 0x00;

	/*
	 * PEC
	 */
	uint16_t pec = pec15_calc(LTC6811_REG_DATA_SIZE, tx_data);

	tx_data[6] = (uint8_t)(pec >> 8);
	tx_data[7] = (uint8_t)(pec);

	LTC6811_Wakeup();

	LTC6811_CS_LOW();

	/*
	 * WRCFG command
	 */
	uint8_t cmd[LTC6811_CMD_SIZE];

	cmd[0] = LTC6811_CMD_WRCFG_0;
	cmd[1] = LTC6811_CMD_WRCFG_1;

	pec = pec15_calc(2, cmd);

	cmd[2] = (uint8_t)(pec >> 8);
	cmd[3] = (uint8_t)(pec);

	status = HAL_SPI_Transmit(&hspi1, cmd, LTC6811_CMD_SIZE, LTC6811_SPI_TIMEOUT);

	if(status == HAL_OK)
	{
		status = HAL_SPI_Transmit(&hspi1, tx_data, LTC6811_REG_FRAME_SIZE, LTC6811_SPI_TIMEOUT);
	}

	LTC6811_CS_HIGH();

	return status;
}

/******************************************************************************
 * Function:    LTC6811_WriteConfigWithDCC
 * Description: Write configuration register with DCC balancing bits.
 *              Sets REFON=1, UV/OV thresholds, and DCC1-DCC12 bits.
 * Parameters:  dcc_bits - 12-bit bitmask for DCC1-DCC12
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_WriteConfigWithDCC(uint16_t dcc_bits)
{
	uint8_t config_register[LTC6811_REG_DATA_SIZE];
	uint8_t tx_data[12];
	uint16_t pec;
	HAL_StatusTypeDef status;

	uint16_t vuv = LTC6811_VUV_VALUE;
	uint16_t vov = LTC6811_VOV_VALUE;

	memset(config_register, 0, LTC6811_REG_DATA_SIZE);

	/*
	 * REFON = 1
	 */
	config_register[0] = LTC6811_CFGR0_REFON;

	/*
	 * UV / OV Thresholds
	 */
	config_register[2] = (uint8_t)(vuv & 0xFF);

	config_register[3] =
			((vuv >> 8) & 0x0F) |
			((vov & 0x0F) << 4);

	config_register[4] = (uint8_t)(vov >> 4);

	/*
	 * Apply DCC bits
	 *
	 * DCC1-DCC4 -> CFGR4[7:4]
	 * DCC5-DCC12 -> CFGR5[7:0]
	 */
	config_register[4] |= (uint8_t)((dcc_bits & 0x000F) << 4);

	config_register[5] |= (uint8_t)((dcc_bits >> 4) & 0xFF);

	/*
	 * WRCFG Command
	 */
	tx_data[0] = LTC6811_CMD_WRCFG_0;
	tx_data[1] = LTC6811_CMD_WRCFG_1;

	pec = pec15_calc(2, tx_data);

	tx_data[2] = (uint8_t)(pec >> 8);
	tx_data[3] = (uint8_t)(pec);

	/*
	 * Config Data
	 */
	memcpy(&tx_data[4], config_register, LTC6811_REG_DATA_SIZE);

	/*
	 * Config PEC
	 */
	pec = pec15_calc(LTC6811_REG_DATA_SIZE, config_register);

	tx_data[10] = (uint8_t)(pec >> 8);
	tx_data[11] = (uint8_t)(pec);

	/*
	 * Send configuration
	 */
	LTC6811_Wakeup();

	LTC6811_CS_LOW();

	status = HAL_SPI_Transmit(&hspi1, tx_data, 12, LTC6811_SPI_TIMEOUT);

	LTC6811_CS_HIGH();

	return status;
}

/******************************************************************************
 * Function:    LTC6811_ReadConfig
 * Description: Read configuration register from the LTC6811.
 *              Data is stored in the provided buffer (8 bytes).
 * Parameters:  cfgBuf - Pointer to 8-byte buffer for config data
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadConfig(uint8_t *cfgBuf)
{
	return LTC6811_ReadRegister(
			LTC6811_CMD_RDCFG_0,
			LTC6811_CMD_RDCFG_1,
			cfgBuf
	);
}

/******************************************************************************
 * Function:    LTC6811_StartCellADC_SC
 * Description: Start cell voltage ADC conversion with Sum of Cells.
 *              ADCVSC command: MD=01 (Normal), DCP=0, CH=All.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartCellADC_SC(void)
{
	return LTC6811_SendCommand(LTC6811_CMD_ADCVSC_0, LTC6811_CMD_ADCVSC_1);
}

/******************************************************************************
 * Function:    LTC6811_StartStatusADC
 * Description: Start status group ADC conversion.
 *              ADSTAT command: MD=01 (Normal), CHST=All.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartStatusADC(void)
{
	return LTC6811_SendCommand(LTC6811_CMD_ADSTAT_0, LTC6811_CMD_ADSTAT_1);
}

/******************************************************************************
 * Function:    LTC6811_StartGPIOADC
 * Description: Start GPIO ADC conversion.
 *              ADAX command: MD=01 (Normal), CHG=All.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartGPIOADC(void)
{
	return LTC6811_SendCommand(LTC6811_CMD_ADAX_0, LTC6811_CMD_ADAX_1);
}

/******************************************************************************
 * Function:    LTC6811_StartADOW
 * Description: Start open wire ADC conversion.
 *              ADOW command with selectable pull-up/pull-down current.
 * Parameters:  pullup - 1 for pull-up test, 0 for pull-down test
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_StartADOW(uint8_t pullup)
{
	if(pullup)
	{
		/*
		 * Pull-Up Open Wire Test
		 */
		return LTC6811_SendCommand(LTC6811_CMD_ADOW_PU_0, LTC6811_CMD_ADOW_PU_1);
	}
	else
	{
		/*
		 * Pull-Down Open Wire Test
		 */
		return LTC6811_SendCommand(LTC6811_CMD_ADOW_PD_0, LTC6811_CMD_ADOW_PD_1);
	}
}

/******************************************************************************
 * Function:    LTC6811_ReadAllCells
 * Description: Start cell ADC conversion and read all 12 cell voltages.
 *              Reads cell register groups A, B, C, D and updates
 *              EMS_Data.CellVoltage[] and EMS_Data.RawCell[].
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadAllCells(void)
{
	uint8_t rxBuf[LTC6811_REG_FRAME_SIZE];
	HAL_StatusTypeDef status;

	/* Start Cell ADC + Sum of Cells */
	status = LTC6811_StartCellADC_SC();
	if(status != HAL_OK) return status;

	HAL_Delay(10);

	/* Group A: Cells 1-3 */
	status = LTC6811_ReadRegister(LTC6811_CMD_RDCVA_0, LTC6811_CMD_RDCVA_1, rxBuf);
	if(status != HAL_OK) return status;
	LTC6811_DecodeCellGroup(rxBuf, 0);

	/* Group B: Cells 4-6 */
	status = LTC6811_ReadRegister(LTC6811_CMD_RDCVB_0, LTC6811_CMD_RDCVB_1, rxBuf);
	if(status != HAL_OK) return status;
	LTC6811_DecodeCellGroup(rxBuf, 3);

	/* Group C: Cells 7-9 */
	status = LTC6811_ReadRegister(LTC6811_CMD_RDCVC_0, LTC6811_CMD_RDCVC_1, rxBuf);
	if(status != HAL_OK) return status;
	LTC6811_DecodeCellGroup(rxBuf, 6);

	/* Group D: Cells 10-12 */
	status = LTC6811_ReadRegister(LTC6811_CMD_RDCVD_0, LTC6811_CMD_RDCVD_1, rxBuf);
	if(status != HAL_OK) return status;
	LTC6811_DecodeCellGroup(rxBuf, 9);

	return HAL_OK;
}

/******************************************************************************
 * Function:    LTC6811_ReadStatusA
 * Description: Read Status Register Group A.
 *              Updates EMS_Data: StackVoltage, InternalTemp, AnalogSupply.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadStatusA(void)
{
	uint8_t rxBuf[LTC6811_REG_FRAME_SIZE];
	uint16_t raw;
	HAL_StatusTypeDef status;

	status = LTC6811_ReadRegister(LTC6811_CMD_RDSTATA_0, LTC6811_CMD_RDSTATA_1, rxBuf);
	if(status != HAL_OK) return status;

	/* Sum Of Cells */
	raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
	EMS_Data.StackVoltage = raw * LTC6811_SOC_LSB;

	/* Internal Die Temperature */
	raw = ((uint16_t)rxBuf[3] << 8) | rxBuf[2];
	EMS_Data.InternalTemp = (raw * LTC6811_ITEMP_SCALE) - LTC6811_ITEMP_OFFSET;

	/* Analog Supply Voltage */
	raw = ((uint16_t)rxBuf[5] << 8) | rxBuf[4];
	EMS_Data.AnalogSupply = raw * LTC6811_SUPPLY_LSB;

	return HAL_OK;
}

/******************************************************************************
 * Function:    LTC6811_ReadStatusB
 * Description: Read Status Register Group B.
 *              Updates EMS_Data: DigitalSupply, ReferenceVoltage2.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadStatusB(void)
{
	uint8_t rxBuf[LTC6811_REG_FRAME_SIZE];
	uint16_t raw;
	HAL_StatusTypeDef status;

	status = LTC6811_ReadRegister(LTC6811_CMD_RDSTATB_0, LTC6811_CMD_RDSTATB_1, rxBuf);
	if(status != HAL_OK) return status;

	/* Digital Supply */
	raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
	EMS_Data.DigitalSupply = raw * LTC6811_SUPPLY_LSB;

	/* REF2 */
	raw = ((uint16_t)rxBuf[5] << 8) | rxBuf[4];
	EMS_Data.ReferenceVoltage2 = raw * LTC6811_SUPPLY_LSB;

	return HAL_OK;
}

/******************************************************************************
 * Function:    LTC6811_ReadGPIO1
 * Description: Start GPIO ADC conversion and read GPIO1 voltage.
 *              Updates EMS_Data.GPIO1Voltage.
 * Parameters:  None
 * Returns:     HAL_StatusTypeDef - HAL_OK on success
 ******************************************************************************/
HAL_StatusTypeDef LTC6811_ReadGPIO1(void)
{
	uint8_t rxBuf[LTC6811_REG_FRAME_SIZE];
	uint16_t raw;
	HAL_StatusTypeDef status;

	/*
	 * Start GPIO Conversion
	 */
	status = LTC6811_StartGPIOADC();
	if(status != HAL_OK) return status;

	HAL_Delay(10);

	/*
	 * RDAUXA Command
	 * Reads: GPIO1, GPIO2, GPIO3
	 */
	status = LTC6811_ReadRegister(LTC6811_CMD_RDAUXA_0, LTC6811_CMD_RDAUXA_1, rxBuf);
	if(status != HAL_OK) return status;

	/*
	 * GPIO1 is first 16-bit value
	 * AUX ADC LSB = 100uV
	 */
	raw = ((uint16_t)rxBuf[1] << 8) | rxBuf[0];
	EMS_Data.RawGPIO[0] = raw;
	EMS_Data.GPIO1Voltage = raw * LTC6811_GPIO_LSB;

	return HAL_OK;
}

/******************************************************************************
 * Function:    LTC6811_DecodeUVOV
 * Description: Decode UV and OV threshold values from configuration data.
 *              Updates EMS_Data.UVThreshold and EMS_Data.OVThreshold.
 * Parameters:  cfgData - Pointer to 8-byte configuration register data
 * Returns:     None
 ******************************************************************************/
void LTC6811_DecodeUVOV(uint8_t *cfgData)
{
	uint16_t vuv;
	uint16_t vov;

	/*
	 * VUV:
	 * CFGR2 + lower nibble of CFGR3
	 */
	vuv =
			((uint16_t)(cfgData[3] & 0x0F) << 8) |
			cfgData[2];

	/*
	 * VOV:
	 * upper nibble of CFGR3 + CFGR4
	 */
	vov =
			((uint16_t)cfgData[4] << 4) |
			((cfgData[3] & 0xF0) >> 4);

	EMS_Data.UVThreshold = vuv * LTC6811_UVOV_LSB;
	EMS_Data.OVThreshold = vov * LTC6811_UVOV_LSB;
}
