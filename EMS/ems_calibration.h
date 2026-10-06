/******************************************************************************
 * @file    ems_calibration.h
 * @brief   EMS Cell Calibration Utilities
 * @details Implements run-time single-point and two-point calibration functions
 *          to dynamically adjust cell voltage readings on the fly.
 *          Calibration coefficients are stored in the global EMS_Data structure.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef EMS_CALIBRATION_H
#define EMS_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ems_data.h"

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
void EMS_CalibrateCellSinglePoint(uint8_t cell_index, float actual_voltage);

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
void EMS_CalibrateCellTwoPoint(uint8_t cell_index, float low_actual, float low_raw, float high_actual, float high_raw);

/******************************************************************************
 * Function:    EMS_CalibrateAllCellsSinglePoint
 * Description: Calibrate all 12 cells at once using an array of 12 reference voltages.
 *              Adjusts the offset coefficients, assuming gains are exactly 1.0.
 * Parameters:  actual_voltages - Array of 12 precise voltages applied to cell inputs (V)
 * Returns:     None
 ******************************************************************************/
void EMS_CalibrateAllCellsSinglePoint(const float *actual_voltages);

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
                                  const float *high_actuals, const float *high_raws);

#ifdef __cplusplus
}
#endif

#endif /* EMS_CALIBRATION_H */

