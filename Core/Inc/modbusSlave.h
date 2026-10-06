/*
 * modbusSlave.h
 *
 *  Created on: Oct 27, 2022
 *      Author: controllerstech.com
 */

#ifndef INC_MODBUSSLAVE_H_
#define INC_MODBUSSLAVE_H_

#include "modbus_crc.h"
#include "stm32f4xx_hal.h"

#define SLAVE_ID 1

#define ILLEGAL_FUNCTION       0x01
#define ILLEGAL_DATA_ADDRESS   0x02
#define ILLEGAL_DATA_VALUE     0x03





uint8_t readHoldingRegs (void);
uint8_t readInputRegs (void);

uint8_t writeSingleReg (void);
uint8_t writeHoldingRegs (void);

void modbusException (uint8_t exceptioncode);
#endif /* INC_MODBUSSLAVE_H_ */
