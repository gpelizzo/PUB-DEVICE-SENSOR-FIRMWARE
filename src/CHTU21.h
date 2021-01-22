/**	
 *	This is a free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
 *
 *
 *	Author: Gilles PELIZZO
 *	Date: November 5th, 2020.
 */
#ifndef __CHTU21_H__
#define __CHTU21_H__

#include <Arduino.h>
#include <Wire.h>
#include "logging.h"

#define HTU21_ADDR          		0x40
#define REG_TRIG_TEMP__HM   		0xE3
#define REG_TRIG_HUM__HM    		0xE5
#define REG_TRIG_TEMP__NHM  		0xF3
#define REG_TRIG_HUM__NHM   		0xEF
#define REG_USER_WRITE      		0xE6
#define REG_USER_READ       		0xE7
#define REG_SOFT_RESET      		0xFE

#define I2C_FREQUENCY       		4000000

#define TIMEOUT_MILLISEC    		100

#define SENSOR_CONSTANT_A			8.1332
#define SENSOR_CONSTANT_B			1762.39
#define SENSOR_CONSTANT_C			235.66

#define HTU21_CRC					0x131		

#define MAX_RETRIES					2

class CHTU21 {
public:
	boolean		init();
    void 		i2cReceiveCallback(int p_iCountAvailable);

	struct STRUCT_SENSOR_VALUES {
		float	fTemperatureValue;
		float	fHumidityValue;
		float	fPartialPressureValue;
		float	fDewPointTemperatureValue;
	};

	STRUCT_SENSOR_VALUES getSensorValues();

private:
	boolean		readI2C(byte p_byDeviceAddr, byte p_byRegister, byte *p_byDataArray, byte p_byLengthToRead);
    byte 		writeI2C(byte p_byDeviceAddr, byte* p_byDataArray, byte p_byLengthToWrite);
	float 		readTemperatureValue();
	float 		readHumidityValue();
	boolean		checkCRC(uint16_t p_uiMeasureValue, uint8_t p_uiCRCValue);
};

#endif
