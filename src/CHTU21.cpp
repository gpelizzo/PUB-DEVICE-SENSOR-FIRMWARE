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
#include "CHTU21.h"

/****************************************************************************************
 * 
 *     *****    *     *     *****       *         *****       **** 
 *     *    *   *     *     *    *      *           *       *     
 *     * * *    *     *     *  *        *           *      *       
 *     *        *     *     *    *      *           *       *        
 *     *         * * *      ******      ******    *****       ****        
 *    
 * **************************************************************************************/

/**
*   Init the driver and set SPI  
*   params: 
*       NONE
*   return:
*       TRUE
*/
boolean CHTU21::init() {
    Wire.begin();
    Wire.setClock(I2C_FREQUENCY);
    return true;
}

CHTU21::STRUCT_SENSOR_VALUES CHTU21::getSensorValues() {
    CHTU21::STRUCT_SENSOR_VALUES l_strSensorValues;
    uint8_t l_uiRetriesCounter;

    l_uiRetriesCounter = MAX_RETRIES;
    while (l_uiRetriesCounter-- >= 0) {
        if ((l_strSensorValues.fTemperatureValue = readTemperatureValue()) != -1) {
            break;
        }

        //CRC is bad
        l_strSensorValues.fTemperatureValue = -255 ;
    }

    l_uiRetriesCounter = MAX_RETRIES;
    while (l_uiRetriesCounter-- >= 0) {
        if ((l_strSensorValues.fHumidityValue = readHumidityValue()) != -1) {
            break;
        }

        //CRC is bad
        l_strSensorValues.fHumidityValue = -255;
    }
    

    //only if CRC is correct
    if ((l_strSensorValues.fTemperatureValue != -255) && (l_strSensorValues.fHumidityValue != -255)) {
        //relatif humidy compensation vs temperature
        l_strSensorValues.fHumidityValue = l_strSensorValues.fHumidityValue - (0.15 * (25 - l_strSensorValues.fTemperatureValue));

        float l_fTemp = ((float)SENSOR_CONSTANT_A)  - ((float)SENSOR_CONSTANT_B / (float)(l_strSensorValues.fTemperatureValue + SENSOR_CONSTANT_C)); 
        l_strSensorValues.fPartialPressureValue = powf(10.00, l_fTemp);

        l_fTemp = (l_strSensorValues.fHumidityValue * l_strSensorValues.fPartialPressureValue) / 100;
        l_fTemp = log10f(l_fTemp) - (float)SENSOR_CONSTANT_A;
        l_fTemp = ((float)SENSOR_CONSTANT_B) / l_fTemp;

        l_strSensorValues.fDewPointTemperatureValue = (l_fTemp + (float)SENSOR_CONSTANT_C)*-1;
    }

    return l_strSensorValues;
}

/****************************************************************************************
 * 
 *     *****    *****      ***     *       *     *****      *******     ******   
 *     *    *   *    *      *       *     *     *     *        *        *
 *     * * *    * * *       *        *   *      * *** *        *        ******
 *     *        *    *      *         * *       *     *        *        *
 *     *        *     *    ***         *        *     *        *        ******
 *   
 * **************************************************************************************/

/**
*   Retreive temperature value   
*   params: 
*       NONE
*   return:
*       floting value
*/
float CHTU21::readTemperatureValue() {
    byte l_byDataArray[3];
    
    if (!this->readI2C(HTU21_ADDR, REG_TRIG_TEMP__HM, &l_byDataArray[0], 3)) {
        return -1;
    }

    float l_fCurrentTemperatureValue = -46.85 + (175.72 * (((l_byDataArray[0] << 8) | l_byDataArray[1])) / 65536.0);

    return l_fCurrentTemperatureValue; 
}

/**
*   Retreive humidity value   
*   params: 
*       NONE
*   return:
*       floting value
*/	
float CHTU21::readHumidityValue() {
    byte l_byDataArray[3];
    
    if (!this->readI2C(HTU21_ADDR, REG_TRIG_HUM__HM, &l_byDataArray[0], 3)) {
        return -1;
    }
    
    float l_fCurrentHumidityValue = -6 + (125.0 * ((l_byDataArray[0] << 8) | l_byDataArray[1]) / 65536.0);

    if (l_fCurrentHumidityValue < 0) {
        l_fCurrentHumidityValue = l_fCurrentHumidityValue * -1.0;
    }

    return l_fCurrentHumidityValue; 
}


/**
*   Read I2C register
*   params: 
*       p_byDeviceAddr:         I2C address of the device
*       p_byRegister:           register address to read
*       p_byDataArray:          byte array to store the result of the reading
*       p_byLengthToRead:       length to read
*   return:
*       true if register has been read. Otherwise false
*/
bool CHTU21::readI2C(byte p_byDeviceAddr, byte p_byRegister, byte *p_pbyDataArray, byte p_byLengthToRead) {
    Wire.beginTransmission(p_byDeviceAddr);
    Wire.write(p_byRegister);
    Wire.endTransmission(false);
    Wire.requestFrom((int)p_byDeviceAddr, 3);

    unsigned long l_timeoutCounter = millis();
    
    while (Wire.available() < p_byLengthToRead) {
        if (millis() > (l_timeoutCounter + TIMEOUT_MILLISEC)) {
            return false;
        }
    };

    byte l_byDataCount = 0; 
    for (l_byDataCount=0; l_byDataCount<p_byLengthToRead; l_byDataCount++) {
       p_pbyDataArray[l_byDataCount] = Wire.read();
    }

    return checkCRC((p_pbyDataArray[0] << 8) | p_pbyDataArray[1], p_pbyDataArray[2]);
}

/**
*   Write to I2C register
*   params: 
*       p_byDeviceAddr:         I2C address of the device
*       p_byDataArray:          byte array containing the data to write
*       p_byLengthToWrite:      length to write
*   return:
*       true if register has been read. Otherwise false
*/
byte CHTU21::writeI2C(byte p_byDeviceAddr, byte* p_byDataArray, byte p_byLengthToWrite) {
    Wire.beginTransmission(p_byDeviceAddr);
    byte l_byDataWritten = Wire.write(p_byDataArray, p_byLengthToWrite);
    Wire.endTransmission(false);
    
    return l_byDataWritten;
}


/**
*   Verify CRC accuracy
*   params: 
*       p_uiMeasureValue:      value of the measure returned by the sensor
*       p_uiCRCValue:          CRC value returned by the sensor
*   return:
*       true if CRC calculated is equal to the one returned by the sensor 
*/
boolean CHTU21::checkCRC(uint16_t p_uiMeasureValue, uint8_t p_uiCRCValue) {
    
    uint32_t p_uiPolynom = 0x988000; // x^8 + x^5 + x^4 + 1
	uint32_t p_uiMsb     = 0x800000;
	uint32_t p_uiMask    = 0xFF8000;
	uint32_t p_uiResult  = (uint32_t)p_uiMeasureValue << 8; // Pad with zeros as specified in spec
	
	while( p_uiMsb != 0x80 ) {
		
		// Check if msb of current value is 1 and apply XOR mask
		if( p_uiResult & p_uiMsb )
			p_uiResult = ((p_uiResult ^ p_uiPolynom) & p_uiMask) | ( p_uiResult & ~p_uiMask);
			
		// Shift by one
		p_uiMsb >>= 1;
		p_uiMask >>= 1;
		p_uiPolynom >>=1;
	}
	
    return ((uint8_t)p_uiResult == p_uiCRCValue) ? true : false;
}
