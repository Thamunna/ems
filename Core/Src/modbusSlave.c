/*
 * modbusSlave.c
 *
 *  Created on: Oct 27, 2022
 *      Author: controllerstech.com
 */

#include "modbusSlave.h"
#include "modbus_crc.h"

extern uint8_t RxData[256];
extern uint8_t TxData[256];
extern UART_HandleTypeDef huart1;

uint16_t Holding_Registers_Database[50]={0};

uint16_t Input_Registers_Database[150] = {0};
//        1234,  4321,  1122,  2211,  1112,  2111,  6666,  7777,  8888,  9999,   // 0-9    30001-30010
//        12345, 15432, 15535, 10234, 19876, 13579, 10293, 19827, 13456, 14567,  // 10-19  30011-30020
//        21345, 22345, 24567, 25678, 26789, 24680, 20394, 29384, 26937, 27654,  // 20-29  30021-30030
//        31245, 31456, 34567, 35678, 36789, 37890, 30948, 34958, 35867, 36092,  // 30-39  30031-30040
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 40-49  30041-30050
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 50-59  30051-30060
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 60-69  30061-30070
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 70-79  30071-30080
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 80-89  30081-30090
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 90-99  30091-30100
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 100-109 30101-30110
//        45678, 46789, 47890, 41235, 42356, 43567, 40596, 49586, 48765, 41029,  // 110-119 30111-30120
//        51234, 52345, 53456, 54567, 55678, 56789, 57890, 58901, 59012, 50123,  // 120-129 30121-30130
//        61234, 62345, 63456, 64567, 15678, 16789, 17890, 18901, 19012, 10123,  // 130-139 30131-30140
//        21234, 22345, 23456, 24567, 25678, 26789, 27890, 28901, 29012, 20123   // 140-149 30141-30150
//};


void sendData (uint8_t *data, int size)
{
	// we will calculate the CRC in this function itself
	uint16_t crc = crc16(data, size);
	data[size] = crc&0xFF;          // CRC LOW
	data[size+1] = (crc>>8)&0xFF;  // CRC HIGH

	HAL_UART_Transmit_DMA(&huart1, data, size+2);
}

void modbusException (uint8_t exceptioncode)
{
	//| SLAVE_ID | FUNCTION_CODE | Exception code | CRC     |
	//| 1 BYTE   |  1 BYTE       |    1 BYTE      | 2 BYTES |

	TxData[0] = RxData[0];       // slave ID
	TxData[1] = RxData[1]|0x80;  // adding 1 to the MSB of the function code
	TxData[2] = exceptioncode;   // Load the Exception code
	sendData(TxData, 3);         // send Data... CRC will be calculated in the function
}


uint8_t readHoldingRegs (void)
{
	uint16_t startAddr = ((RxData[2]<<8)|RxData[3]);  // start Register Address

	uint16_t numRegs = ((RxData[4]<<8)|RxData[5]);   // number to registers master has requested
	if ((numRegs<1)||(numRegs>125))  // maximum no. of Registers as per the PDF
	{
		modbusException (ILLEGAL_DATA_VALUE);  // send an exception
		return 0;
	}

	uint16_t endAddr = startAddr+numRegs-1;  // end Register
	if (endAddr > sizeof(Holding_Registers_Database))  // end Register can not be more than 49 as we only have record of 50 Registers in total
	{
		modbusException(ILLEGAL_DATA_ADDRESS);   // send an exception
		return 0;
	}

	// Prepare TxData buffer

	//| SLAVE_ID | FUNCTION_CODE | BYTE COUNT | DATA      | CRC     |
	//| 1 BYTE   |  1 BYTE       |  1 BYTE    | N*2 BYTES | 2 BYTES |

	TxData[0] = SLAVE_ID;   // slave ID
	TxData[1] = RxData[1];  // function code
	TxData[2] = numRegs*2;  // Byte count
	int indx = 3;  // we need to keep track of how many bytes has been stored in TxData Buffer

	for (int i=0; i<numRegs; i++)   // Load the actual data into TxData buffer
	{
		TxData[indx++] = (Holding_Registers_Database[startAddr]>>8)&0xFF;  // extract the higher byte
		TxData[indx++] = (Holding_Registers_Database[startAddr])&0xFF;   // extract the lower byte
		startAddr++;  // increment the register address
	}

	sendData(TxData, indx);  // send data... CRC will be calculated in the function itself
	return 1;   // success
}

uint8_t readInputRegs (void)
{
	uint16_t startAddr = ((RxData[2]<<8)|RxData[3]);  // start Register Address

	uint16_t numRegs = ((RxData[4]<<8)|RxData[5]);   // number to registers master has requested
	if ((numRegs<1)||(numRegs>125))  // maximum no. of Registers as per the PDF
	{
		modbusException (ILLEGAL_DATA_VALUE);  // send an exception
		return 0;
	}

	uint16_t endAddr = startAddr+numRegs-1;  // end Register
	if (endAddr > sizeof(Input_Registers_Database))  // end Register can not be more than 150 as we only have record of 50 Registers in total
	{
		modbusException(ILLEGAL_DATA_ADDRESS);   // send an exception
		return 0;
	}

	// Prepare TxData buffer

	//| SLAVE_ID | FUNCTION_CODE | BYTE COUNT | DATA      | CRC     |
	//| 1 BYTE   |  1 BYTE       |  1 BYTE    | N*2 BYTES | 2 BYTES |

	TxData[0] = SLAVE_ID;  // slave ID
	TxData[1] = RxData[1];  // function code
	TxData[2] = numRegs*2;  // Byte count
	int indx = 3;  // we need to keep track of how many bytes has been stored in TxData Buffer

	for (int i=0; i<numRegs; i++)   // Load the actual data into TxData buffer
	{
		TxData[indx++] = (Input_Registers_Database[startAddr]>>8)&0xFF;  // extract the higher byte
		TxData[indx++] = (Input_Registers_Database[startAddr])&0xFF;   // extract the lower byte
		startAddr++;  // increment the register address
	}

	sendData(TxData, indx);  // send data... CRC will be calculated in the function itself
	return 1;   // success
}

uint8_t writeHoldingRegs (void)
{
	uint16_t startAddr = ((RxData[2]<<8)|RxData[3]);  // start Register Address

	uint16_t numRegs = ((RxData[4]<<8)|RxData[5]);   // number to registers master has requested
	if ((numRegs<1)||(numRegs>123))  // maximum no. of Registers as per the PDF
	{
		modbusException (ILLEGAL_DATA_VALUE);  // send an exception
		return 0;
	}

	uint16_t endAddr = startAddr+numRegs-1;  // end Register
	if (endAddr > sizeof(Holding_Registers_Database))  // end Register can not be more than 49 as we only have record of 50 Registers in total
	{
		modbusException(ILLEGAL_DATA_ADDRESS);   // send an exception
		return 0;
	}

	/* start saving 16 bit data
	 * Data starts from RxData[7] and we need to combine 2 bytes together
	 * 16 bit Data = firstByte<<8|secondByte
	 */
	int indx = 7;  // we need to keep track of index in RxData
	for (int i=0; i<numRegs; i++)
	{
		Holding_Registers_Database[startAddr++] = (RxData[indx]<<8)|RxData[indx+1];
		indx = indx + 2;
	}

	// Prepare Response

	//| SLAVE_ID | FUNCTION_CODE | Start Addr | num of Regs    | CRC     |
	//| 1 BYTE   |  1 BYTE       |  2 BYTE    | 2 BYTES      | 2 BYTES |

	TxData[0] = SLAVE_ID;    // slave ID
	TxData[1] = RxData[1];   // function code
	TxData[2] = RxData[2];   // Start Addr HIGH Byte
	TxData[3] = RxData[3];   // Start Addr LOW Byte
	TxData[4] = RxData[4];   // num of Regs HIGH Byte
	TxData[5] = RxData[5];   // num of Regs LOW Byte

	sendData(TxData, 6);  // send data... CRC will be calculated in the function itself
	return 1;   // success
}

uint8_t writeSingleReg (void)
{
	uint16_t startAddr = ((RxData[2]<<8)|RxData[3]);  // start Register Address

	if (startAddr > sizeof(Holding_Registers_Database))  // The Register Address can not be more than 49 as we only have record of 50 Registers in total
	{
		modbusException(ILLEGAL_DATA_ADDRESS);   // send an exception
		return 0;
	}

	/* Save the 16 bit data
	 * Data is the combination of 2 bytes, RxData[4] and RxData[5]
	 */

	Holding_Registers_Database[startAddr] = (RxData[4]<<8)|RxData[5];

	// Prepare Response

	//| SLAVE_ID | FUNCTION_CODE | Start Addr | Data     | CRC     |
	//| 1 BYTE   |  1 BYTE       |  2 BYTE    | 2 BYTES  | 2 BYTES |

	TxData[0] = SLAVE_ID;    // slave ID
	TxData[1] = RxData[1];   // function code
	TxData[2] = RxData[2];   // Start Addr HIGH Byte
	TxData[3] = RxData[3];   // Start Addr LOW Byte
	TxData[4] = RxData[4];   // Reg Data HIGH Byte
	TxData[5] = RxData[5];   // Reg Data LOW  Byte

	sendData(TxData, 6);  // send data... CRC will be calculated in the function itself
	return 1;   // success
}
