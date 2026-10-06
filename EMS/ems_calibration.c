/******************************************************************************
 * @file    ems_calibration.c
 * @brief   EMS Cell Calibration Utilities Implementation
 * @details Implements runtime single-point and two-point calibration functions
 *          that dynamically calculate and update cell gain and offset factors.
 *          These coefficients are used by the LTC6811 driver to adjust readings.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ems_calibration.h"
#include "ems_data.h"
#include <stddef.h>

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_CalibrateCellSinglePoint
 * Description: Calibrate a specific cell using a single known reference voltage.
 *              Measures the raw input voltage and computes the required offset.
 *              Assumes gain is exactly 1.0.
 * Parameters:  cell_index     - Index of the cell (0 to 11)
 *              actual_voltage - Precise reference voltage applied to the cell input (V)
 * Returns:     None
 ******************************************************************************/
void EMS_CalibrateCellSinglePoint(uint8_t cell_index, float actual_voltage)
{
	if (cell_index >= EMS_TOTAL_CELLS)
	{
		return;
	}

	/* Get the uncalibrated voltage directly from the raw ADC value */
	float raw_uncalibrated = EMS_Data.RawCell[cell_index] * LTC6811_CELL_LSB;

	/* Calculate offset: Offset = Actual - Raw */
	EMS_Data.CellOffset[cell_index] = actual_voltage - raw_uncalibrated;

	/* Reset gain to nominal 1.0 */
	EMS_Data.CellGain[cell_index] = 1.0f;
}

/******************************************************************************
 * Function:    EMS_CalibrateCellTwoPoint
 * Description: Calibrate a specific cell using two reference voltage points.
 *              Calculates and updates both Gain and Offset coefficients.
 * Parameters:  cell_index   - Index of the cell (0 to 11)
 *              low_actual   - Precise reference voltage at low point (e.g., 1.5000V)
 *              low_raw      - Uncalibrated raw voltage read at low point (V)
 *              high_actual  - Precise reference voltage at high point (e.g., 4.0000V)
 *              high_raw     - Uncalibrated raw voltage read at high point (V)
 * Returns:     None
 ******************************************************************************/
void EMS_CalibrateCellTwoPoint(uint8_t cell_index, float low_actual, float low_raw, float high_actual, float high_raw)
{
	if (cell_index >= EMS_TOTAL_CELLS)
	{
		return;
	}

	/* Avoid division by zero */
	if (high_raw == low_raw)
	{
		return;
	}

	/* Calculate Gain: Gain = (V_high_actual - V_low_actual) / (V_high_raw - V_low_raw) */
	float gain = (high_actual - low_actual) / (high_raw - low_raw);

	/* Calculate Offset: Offset = V_high_actual - (Gain * V_high_raw) */
	float offset = high_actual - (gain * high_raw);

	/* Store calculated calibration values */
	EMS_Data.CellGain[cell_index] = gain;
	EMS_Data.CellOffset[cell_index] = offset;
}

/******************************************************************************
 * Function:    EMS_CalibrateAllCellsSinglePoint
 * Description: Calibrate all 12 cells at once using an array of 12 reference voltages.
 *              Adjusts the offset coefficients, assuming gains are exactly 1.0.
 * Parameters:  actual_voltages - Array of 12 precise voltages applied to cell inputs (V)
 * Returns:     None
 ******************************************************************************/
void EMS_CalibrateAllCellsSinglePoint(const float *actual_voltages)
{
	if (actual_voltages == NULL)
	{
		return;
	}

	for (uint8_t i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		EMS_CalibrateCellSinglePoint(i, actual_voltages[i]);
	}
}

/******************************************************************************
 * Function:    EMS_CalibrateAllCellsTwoPoint
 * Description: Calibrate all 12 cells at once using arrays of 12 reference points.
 *              Calculates and updates both Gain and Offset coefficients for all cells.
 * Parameters:  low_actuals  - Array of 12 reference voltages at low points (V)
 *              low_raws     - Array of 12 raw voltages at low points (V)
 *              high_actuals - Array of 12 reference voltages at high points (V)
 *              high_raws    - Array of 12 raw voltages at high points (V)
 * Returns:     None
 ******************************************************************************/
void EMS_CalibrateAllCellsTwoPoint(const float *low_actuals,  const float *low_raws,
                                  const float *high_actuals, const float *high_raws)
{
	if (low_actuals == NULL || low_raws == NULL || high_actuals == NULL || high_raws == NULL)
	{
		return;
	}

	for (uint8_t i = 0; i < EMS_TOTAL_CELLS; i++)
	{
		EMS_CalibrateCellTwoPoint(i, low_actuals[i], low_raws[i], high_actuals[i], high_raws[i]);
	}
}

