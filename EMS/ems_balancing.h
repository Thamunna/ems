/******************************************************************************
 * @file    ems_balancing.h
 * @brief   EMS Passive Cell Balancing
 * @details Selects cells for passive balancing based on voltage
 *          imbalance and writes DCC bits to LTC6811 configuration.
 *
 * @version 1.0
 * @date    2026-07-06
 ******************************************************************************/

#ifndef EMS_BALANCING_H
#define EMS_BALANCING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ems_data.h"

/* Exported Functions --------------------------------------------------------*/

/******************************************************************************
 * Function:    EMS_CellBalancing
 * Description: Run passive cell balancing algorithm.
 *              1. Check IC temperature safety (>60C blocks balancing)
 *              2. Find minimum valid cell voltage (>2.5V)
 *              3. Select cells with >50mV imbalance for discharge
 *              4. Write DCC bits to LTC6811 configuration register
 *              Updates EMS_Data.CellBalanceStatus[] and EMS_Data.DCCBits.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_CellBalancing(void);

/******************************************************************************
 * Function:    EMS_StopBalancing
 * Description: Stop all cell balancing by clearing DCC bits.
 *              Writes configuration with DCC=0 to the LTC6811.
 *              Updates EMS_Data.CellBalanceStatus[] and EMS_Data.DCCBits.
 * Parameters:  None
 * Returns:     None
 ******************************************************************************/
void EMS_StopBalancing(void);

#ifdef __cplusplus
}
#endif

#endif /* EMS_BALANCING_H */
