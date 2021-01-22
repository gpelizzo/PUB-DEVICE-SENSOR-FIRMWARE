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
#include "CATSettings.h"

#define AT_PREFIXE_COMMAND                          PROGMEM("AT+")
#define AT_API_HOSTNAME                             PROGMEM("APIHOSTNAME")
#define AT_API_AUTHORIZATION                        PROGMEM("APIAUTHORIZATION")
#define AT_API_URI                                  PROGMEM("APIURI")
#define AT_API_PORT                                 PROGMEM("APIPORT")
#define AT_API_CLIENT_ID                            PROGMEM("APICLIENTID")
#define AT_API_KEEP_ALIVE_TIMEOUT                   PROGMEM("APIKEEPALIVETIMEOUT")
#define AT_AP_SSID                                  PROGMEM("APSSID")
#define AT_AP_KEY                                   PROGMEM("APKEY")
#define AT_RADIO_DEVICE_ID                          PROGMEM("RADIODEVICEID")
#define AT_RADIO_SERVER_ID                          PROGMEM("RADIOSERVERID")
#define AT_RADIO_OUTPUT_PWR                         PROGMEM("RADIOOUTPWR")
#define AT_RADIO_MAX_RETRIES                        PROGMEM("RADIOMAXRETRIES")
#define AT_RADIO_MESSAGE_SIGNATURE                  PROGMEM("RADIOMSGSIGNATURE")
#define AT_RADIO_KEEP_ALIVE_TIMEOUT                 PROGMEM("RADIOKEEPALIVETIMEOUT")
#define AT_MISCELLANEOUS_SENSOR_MEASUREMENT_TIMEOUT PROGMEM("SENSORMEASUREMENTTIMEOUT")
#define AT_SENSOR_VALUES                            PROGMEM("SENSORVALUES")
#define AT_JSON_STATUS                              PROGMEM("JSONSTATUS")
#define AT_JSON_SETTINGS                            PROGMEM("JSONSETTINGS")
#define AT_STATUS                                   PROGMEM("STATUS")
#define AT_VERSION                                  PROGMEM("VERSION")
#define AT_UID                                      PROGMEM("UID")
#define AT_DEVICE_TYPE                              PROGMEM("DEVICETYPE")
#define AT_SAVE_SETTINGS                            PROGMEM("SAVESETTINGS")
#define AT_FACTORY_RESET                            PROGMEM("FACTORYRESET")
#define AT_ECHO                                     PROGMEM("ECHO")
#define AT_USAGE                                    PROGMEM("USAGE")

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
*   int AT settings giving ability to retreive and update device settings 
*   params: 
*       p_pSerialPort:                  Serial port receiving AT commands
*       p_pGlobalSettingsAndStatus:     pointer to the global settings 
*       p_pcallbackSettings:            callback to perform seetings saving into the flash and bridge factory reset
*   return:
*       NONE      
*/
void CATSettings::init(Serial_*p_pSerialPort, STRUCT_GLOBAL_SETTINGS_AND_STATUS *p_pGlobalSettingsAndStatus, 
                        EventGroupHandle_t *p_pxEventGroupMiscellaneousHandle, QueueHandle_t *p_pxQueueATSettingsHandle) {
    m_pSerialPort = p_pSerialPort;
    m_pGlobalSettingsAndStatus = p_pGlobalSettingsAndStatus;
    m_pxEventGroupMiscellaneousHandle = p_pxEventGroupMiscellaneousHandle;
    m_pxQueueATSettingsHandle = p_pxQueueATSettingsHandle;

    m_cBufferIncomingIndex = 0;
}

/**
*   pool AT serial port and check if chars ahs been entered 
*   params: 
*       NONE
*   return:
*       NONE      
*/
void CATSettings::pool() {
    if (*m_pSerialPort) {
        if (m_pSerialPort->available()) {
            if (m_cBufferIncomingIndex < MAX_SETTINGSDEVICE_INCOMING_BUFFER_LENGTH) {
                m_bufferIncoming[m_cBufferIncomingIndex++] = m_pSerialPort->read();

                //echo char if echo mode is set
                if (m_bEchoEnabled) {
                    m_pSerialPort->write( m_bufferIncoming[m_cBufferIncomingIndex - 1]);
                }

                //verify if entered chars form an AT command
                checkATCommand();
            } else {
                //buffer is full. free it
                m_pSerialPort->println(PROGMEM("BUFFER FULL")); 
                m_cBufferIncomingIndex = 0;
                memset(m_bufferIncoming, 0, MAX_SETTINGSDEVICE_INCOMING_BUFFER_LENGTH);
            }
        }
    }
}

/**
*   check if chars received by serial port form an AT command. Determine if AT command is a GET, SET or DO method 
*   params: 
*       NONE
*   return:
*       NONE      
*/
void CATSettings::checkATCommand() {
    char *l_pcMethodChar;

    char *l_pcEndATSequence = strstr(m_bufferIncoming, "\r\n");
    
    //received \r\n
    if (l_pcEndATSequence != NULL) {
        char *l_pcATStartSequence = strstr(m_bufferIncoming, "AT");

        //receive string starting with AT
        if (l_pcATStartSequence != NULL) {

            //simple "AT", then reply only OK
            if (l_pcEndATSequence == (l_pcATStartSequence + 2)) {
                m_pSerialPort->println("OK");  
                m_pSerialPort->println("");   
            } else {
                //otherwise, ensure that 'AT' is followed by '+'
                *l_pcEndATSequence = '\0';
                if (*(l_pcATStartSequence + 2) == '+') {
                    //'+' has been recived. Now, parse following to retreive the method ('?', '=' or nothing)
                    l_pcMethodChar = strstr(m_bufferIncoming, "?");
                    if (l_pcMethodChar != NULL) {
                        //'?' means GET method
                        m_strctATCommand.enmMethod = CATSettings::ENM_METHOD::GET;
                    } else {
                        l_pcMethodChar = strstr(m_bufferIncoming, "=");
                        if (l_pcMethodChar != NULL) {
                            //'=' means SET method
                            m_strctATCommand.enmMethod = CATSettings::ENM_METHOD::SET;
                        } else {
                            //nothing means DO method
                            m_strctATCommand.enmMethod = CATSettings::ENM_METHOD::DO;
                            m_strctATCommand.pcCommand = l_pcATStartSequence + 3;
                            m_strctATCommand.pcParam = NULL;
                        }
                    }
                    
                    //sort command and param
                    if (m_strctATCommand.enmMethod != CATSettings::ENM_METHOD::DO)  {
                        if (m_strctATCommand.enmMethod == CATSettings::ENM_METHOD::SET) {
                            m_strctATCommand.pcParam = l_pcMethodChar+1;
                        }
                        *l_pcMethodChar = '\0';
                        m_strctATCommand.pcCommand = l_pcATStartSequence + 3;
                    }

                    //now, operate according to the command
                    operateATCommand();
                } else {
                    m_pSerialPort->println(PROGMEM("INCORRECT")); 
                }
            }
        }   

        m_cBufferIncomingIndex = 0;
        memset(m_bufferIncoming, 0, MAX_SETTINGSDEVICE_INCOMING_BUFFER_LENGTH);
    }
}


/**
*   Operate AT command according to the method
*   params: 
*       NONE
*   return:
*       NONE      
*/
void CATSettings::operateATCommand() {
    char * l_pcValue;

#ifdef BRIDGE_MODE
    //Get API Hostname
    if (isGetCommand(AT_API_HOSTNAME)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cHostname);
        goto ok;
    }

    //Set API Hostname
    if (isSetCommand(AT_API_HOSTNAME)) {
        if (isParamLengthConsistent(MAX_HOSTNAME_LENGTH)) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cHostname, m_strctATCommand.pcParam);
            goto ok;
        } else {
            goto length_error;
        }
    }

    //Get API authorization token
    if (isGetCommand(AT_API_AUTHORIZATION)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cAuthorizationToken);
        goto ok;
    }

    //Set API authorization token
    if (isSetCommand(AT_API_AUTHORIZATION)) {
        if (isParamLengthConsistent(MAX_AUTHORIZATION_LENGTH)) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cAuthorizationToken, m_strctATCommand.pcParam);
            goto ok;
        } else {
            goto length_error;
        }
    }

    //Get API URI
    if (isGetCommand(AT_API_URI)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cUri);
        goto ok;
    }

    //Set API URI
    if (isSetCommand(AT_API_URI)) {
        if (isParamLengthConsistent(MAX_URI_LENGTH)) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cUri, m_strctATCommand.pcParam);
            goto ok;
        } else {
            goto length_error;
        }
    }

    //Get API PORT
    if (isGetCommand(AT_API_PORT)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiPort);
        goto ok;
    }

    //Set API PORT
    if (isSetCommand(AT_API_PORT)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
           m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiPort = getParamNumericValue(m_strctATCommand.pcParam);
           goto ok;
        } else {
            goto error;
        }
    }

    //Get API URI
    if (isGetCommand(AT_API_CLIENT_ID)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cClientID);
        goto ok;
    }

    //Set API URI
    if (isSetCommand(AT_API_CLIENT_ID)) {
        if (isParamLengthConsistent(MAX_URI_LENGTH)) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cClientID, m_strctATCommand.pcParam);
            goto ok;
        } else {
            goto length_error;
        }
    }

    //Get API KEEPALIVE TIMEOUT
    if (isGetCommand(AT_API_KEEP_ALIVE_TIMEOUT)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiKeepAliveTimeout);
        goto ok;
    }

    //Set API KEEPALIVE TIMEOUT
    if (isSetCommand(AT_API_KEEP_ALIVE_TIMEOUT)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
           m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiKeepAliveTimeout = getParamNumericValue(m_strctATCommand.pcParam);
           goto ok;
        } else {
            goto error;
        }
    }

    //Get Access Point SSID
    if (isGetCommand(AT_AP_SSID)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctWifiSettings.cSSID);
        goto ok;
    }

    //Set Access Point SSID
    if (isSetCommand(AT_AP_SSID)) {
        if (isParamLengthConsistent(MAX_URI_LENGTH)) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctWifiSettings.cSSID, m_strctATCommand.pcParam);
            goto ok;
        } else {
            goto length_error;
        }
    }

    //Set Access Point password
    if (isSetCommand(AT_AP_KEY)) {
        if (isParamLengthConsistent(MAX_WIFI_KEY_LENGTH)) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctWifiSettings.cKey, m_strctATCommand.pcParam);
            goto ok;
        } else {
            goto length_error;
        }
    }
#endif
    //Get Device ID
    if (isGetCommand(AT_RADIO_DEVICE_ID)) {
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctRadioSettings.uiDeviceID);
         m_pSerialPort->println(PROGMEM(" (from )"));
        goto ok;
    }

    //Set Device ID
    if (isSetCommand(AT_RADIO_DEVICE_ID)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
            if ((getParamNumericValue(m_strctATCommand.pcParam) >= 1) && (getParamNumericValue(m_strctATCommand.pcParam) < MAX_RADIO_DEVICES)) {
                m_pGlobalSettingsAndStatus->strctRadioSettings.uiDeviceID = getParamNumericValue(m_strctATCommand.pcParam);
                goto ok;
            }
        }
        
        goto error;
    }

#ifndef BRIDGE_MODE
    //Get Server ID
    if (isGetCommand(AT_RADIO_SERVER_ID)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiServerID);
        goto ok;
    }

    //Set Server ID
    if (isSetCommand(AT_RADIO_SERVER_ID)) {
       if (isParamNumericValue(m_strctATCommand.pcParam)) {
           m_pGlobalSettingsAndStatus->strctRadioSettings.uiServerID = getParamNumericValue(m_strctATCommand.pcParam);
           goto ok;
        } else {
            goto error;
        }
    }

#endif
    //Get Output Power
    if (isGetCommand(AT_RADIO_OUTPUT_PWR)) {
        switch (m_pGlobalSettingsAndStatus->strctRadioSettings.uiOutputPower) {
            case 1:
            m_pSerialPort->println(PROGMEM("-30dbm"));
            break;

            case 2:
            m_pSerialPort->println(PROGMEM("-20dbm"));
            break;

            case 3:
            m_pSerialPort->println(PROGMEM("-15dbm"));
            break;

            case 4:
            m_pSerialPort->println(PROGMEM("-10dbm"));
            break;

            case 5:
            m_pSerialPort->println(PROGMEM("0dbm"));
            break;

            case 6:
            m_pSerialPort->println(PROGMEM("5dbm"));
            break;

            case 7:
            m_pSerialPort->println(PROGMEM("7dbm"));
            break;

            case 8:
            m_pSerialPort->println(PROGMEM("10dbm"));
            break;
        }
        goto ok;
    }

    //Set SOutput Power
    if (isSetCommand(AT_RADIO_OUTPUT_PWR)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
            if ((getParamNumericValue(m_strctATCommand.pcParam) >= 1) && (getParamNumericValue(m_strctATCommand.pcParam) <=8)) {
                m_pGlobalSettingsAndStatus->strctRadioSettings.uiOutputPower = getParamNumericValue(m_strctATCommand.pcParam);
                goto ok;
            }
        }
        
        goto error;
    }

    //Get Output Power
    if (isGetCommand(AT_RADIO_MAX_RETRIES)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiMaxRetries);
        goto ok;
    }

    //Set SOutput Power
    if (isSetCommand(AT_RADIO_MAX_RETRIES)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
           m_pGlobalSettingsAndStatus->strctRadioSettings.uiMaxRetries = getParamNumericValue(m_strctATCommand.pcParam);
           goto ok;
        } else {
            goto error;
        }
    }

    //Get Radio message signature
    if (isGetCommand(AT_RADIO_MESSAGE_SIGNATURE)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiMessageSignature);
        goto ok;
    }

    //Set Radio message signature
    if (isSetCommand(AT_RADIO_MESSAGE_SIGNATURE)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
           m_pGlobalSettingsAndStatus->strctRadioSettings.uiMessageSignature = getParamNumericValue(m_strctATCommand.pcParam);
           goto ok;
        } else {
            goto error;
        }
    }

    //Get Radio keep alive timeout
    if (isGetCommand(AT_RADIO_KEEP_ALIVE_TIMEOUT)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiKeepAliveTimeout);
        goto ok;
    }


    //Set Radio keep alive timeout
    if (isSetCommand(AT_RADIO_KEEP_ALIVE_TIMEOUT)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
           m_pGlobalSettingsAndStatus->strctRadioSettings.uiKeepAliveTimeout = getParamNumericValue(m_strctATCommand.pcParam);
           goto ok;
        } else {
            goto error;
        }
    }

    //Get sensor values measurementtimeout reading
    if (isGetCommand(AT_MISCELLANEOUS_SENSOR_MEASUREMENT_TIMEOUT)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctMiscellaneousSettings.uiReadSensorValuesMeasurementTimeout);
        goto ok;
    }

    //Set sensor values measurement timeout reading
    if (isSetCommand(AT_MISCELLANEOUS_SENSOR_MEASUREMENT_TIMEOUT)) {
        if (isParamNumericValue(m_strctATCommand.pcParam)) {
           m_pGlobalSettingsAndStatus->strctMiscellaneousSettings.uiReadSensorValuesMeasurementTimeout = getParamNumericValue(m_strctATCommand.pcParam);
           goto ok;
        } else {
            goto error;
        }
    }

    //Print status formatted JSON
    if (isDoCommand(AT_JSON_STATUS)) {
        m_pSerialPort->print("{");
#ifdef BRIDGE_MODE
        m_pSerialPort->print(PROGMEM("\"ap_ssid\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctWifiSettings.cSSID);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"ap_key\":\""));
        m_pSerialPort->print("");
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"ap_ip\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctWifiStatus.cIP);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"ap_gateway\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctWifiStatus.cGateway);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"ap_mask\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctWifiStatus.cMask);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"ap_mac\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctWifiStatus.cMAC);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"api_hostname\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cHostname);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"api_port\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiPort);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"api_uri\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cUri);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"api_client_id\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cClientID);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"api_authorization_token\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cAuthorizationToken);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"api_keepalive_timeout\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiKeepAliveTimeout);
        m_pSerialPort->print(PROGMEM("\","));
#endif
        m_pSerialPort->print(PROGMEM("\"radio_device_id\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctRadioSettings.uiDeviceID);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"radio_message_signature\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctRadioSettings.uiMessageSignature);
        m_pSerialPort->print(PROGMEM("\","));
#ifndef BRIDGE_MODE
        m_pSerialPort->print(PROGMEM("\"radio_server_id\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctRadioSettings.uiServerID);
        m_pSerialPort->print(PROGMEM("\","));
#endif
        m_pSerialPort->print(PROGMEM("\"radio_output_power\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctRadioSettings.uiOutputPower);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"radio_max_retries\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctRadioSettings.uiMaxRetries);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"radio_keepalive_timeout\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctRadioSettings.uiKeepAliveTimeout);

        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"sensor_values_measurement_timeout\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->strctMiscellaneousSettings.uiReadSensorValuesMeasurementTimeout);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"version\":\""));
        m_pSerialPort->print(VERSION_MAJOR);
        m_pSerialPort->print(PROGMEM("."));
        m_pSerialPort->print(VERSION_MINOR);
        m_pSerialPort->print(PROGMEM("\","));
        m_pSerialPort->print(PROGMEM("\"uid\":\""));
        m_pSerialPort->print(m_pGlobalSettingsAndStatus->cUID); 
        m_pSerialPort->print(PROGMEM("\","));


        xEventGroupSetBits(*m_pxEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SENSOR_VALUES_MEASUREMENT);

        CHTU21::STRUCT_SENSOR_VALUES l_strctSensorValues;

        if (xQueueReceive(*m_pxQueueATSettingsHandle, &l_strctSensorValues, 100 / portTICK_PERIOD_MS)) {  
            m_pSerialPort->print(PROGMEM("\"sensor_temperature_value\":\""));
            m_pSerialPort->print(l_strctSensorValues.fTemperatureValue);
            m_pSerialPort->print("\",");

            m_pSerialPort->print(PROGMEM("\"sensor_humidity_value\":\""));
            m_pSerialPort->print(l_strctSensorValues.fHumidityValue);
            m_pSerialPort->print("\",");

            m_pSerialPort->print(PROGMEM("\"sensor_partial_pressure_value\":\""));
            m_pSerialPort->print(l_strctSensorValues.fPartialPressureValue);
            m_pSerialPort->print("\",");

            m_pSerialPort->print(PROGMEM("\"sensor_dew_point_temperature_value\":\""));
            m_pSerialPort->print(l_strctSensorValues.fDewPointTemperatureValue);
            m_pSerialPort->print("\",");
        }

        m_pSerialPort->print(PROGMEM("\"device_type\":\""));
        m_pSerialPort->print(DEVICE_TYPE); 
        m_pSerialPort->println(PROGMEM("\"}"));
        goto ok;
    }

    //
    if (isSetCommand(AT_JSON_SETTINGS)) {
#ifdef BRIDGE_MODE
        if ((l_pcValue = getJsonValueFromKey(PROGMEM("ap_ssid"))) != NULL ) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctWifiSettings.cSSID, l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("ap_key"))) != NULL ) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctWifiSettings.cKey, l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("api_hostname"))) != NULL ) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cHostname, l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("api_port"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiPort = getParamNumericValue(l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("api_uri"))) != NULL ) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cUri, l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("api_client_id"))) != NULL ) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cClientID, l_pcValue);
        }

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("api_authorization_token"))) != NULL ) {
            strcpy(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cAuthorizationToken, l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("api_keepalive_timeout"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiKeepAliveTimeout = getParamNumericValue(l_pcValue);
        }  
#endif

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("radio_device_id"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctRadioSettings.uiDeviceID = getParamNumericValue(l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("radio_message_signature"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctRadioSettings.uiMessageSignature = getParamNumericValue(l_pcValue);
        } 

#ifndef BRIDGE_MODE
        if ((l_pcValue = getJsonValueFromKey(PROGMEM("radio_server_id"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctRadioSettings.uiServerID = getParamNumericValue(l_pcValue);
        } 
#endif
        if ((l_pcValue = getJsonValueFromKey(PROGMEM("radio_output_power"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctRadioSettings.uiOutputPower = getParamNumericValue(l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("radio_max_retries"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctRadioSettings.uiMaxRetries = getParamNumericValue(l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("radio_keepalive_timeout"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctRadioSettings.uiKeepAliveTimeout = getParamNumericValue(l_pcValue);
        } 

        if ((l_pcValue = getJsonValueFromKey(PROGMEM("sensor_values_measurement_timeout"))) != NULL ) {
            m_pGlobalSettingsAndStatus->strctMiscellaneousSettings.uiReadSensorValuesMeasurementTimeout = getParamNumericValue(l_pcValue);
        } 
        goto ok;
    }

    //Get current sensor values
    if (isDoCommand(AT_SENSOR_VALUES)) {

        xEventGroupSetBits(*m_pxEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SENSOR_VALUES_MEASUREMENT);

        CHTU21::STRUCT_SENSOR_VALUES l_strctSensorValues;

        if (xQueueReceive(*m_pxQueueATSettingsHandle, &l_strctSensorValues, 100 / portTICK_PERIOD_MS)) {  
            m_pSerialPort->print(PROGMEM("Temperature: "));
            m_pSerialPort->println(l_strctSensorValues.fTemperatureValue);

            m_pSerialPort->print(PROGMEM("Humidity: "));
            m_pSerialPort->println(l_strctSensorValues.fHumidityValue);

            m_pSerialPort->print(PROGMEM("Partial pressure: "));
            m_pSerialPort->println(l_strctSensorValues.fPartialPressureValue);

            m_pSerialPort->print(PROGMEM("Dew point: "));
            m_pSerialPort->println(l_strctSensorValues.fDewPointTemperatureValue);
            goto ok;
        } else {
            m_pSerialPort->println("ERROR-TIMEOUT");
        }
        goto error;
    }

    //Get Firmware version
    if (isDoCommand(AT_VERSION)) {
        m_pSerialPort->print(PROGMEM("Version: "));
        m_pSerialPort->print(VERSION_MAJOR);
        m_pSerialPort->print(".");
        m_pSerialPort->println(VERSION_MINOR);
        goto ok;
    }

    //Get Unique ID
    if (isDoCommand(AT_UID)) {
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->cUID);
        goto ok;
    }

    //Get Devive type
    if (isDoCommand(AT_DEVICE_TYPE)) {
        m_pSerialPort->println(DEVICE_TYPE);
        goto ok;
    }

    //Print status
    if (isDoCommand(AT_STATUS)) {
        m_pSerialPort->print(PROGMEM("version:"));
        m_pSerialPort->print(VERSION_MAJOR);
        m_pSerialPort->print(PROGMEM("."));
        m_pSerialPort->println(VERSION_MINOR);
        m_pSerialPort->print(PROGMEM("uid:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->cUID);
        m_pSerialPort->print(PROGMEM("device type:"));
        m_pSerialPort->println(DEVICE_TYPE);
#ifdef BRIDGE_MODE
        m_pSerialPort->println(PROGMEM("----WIFI STATUS----"));
        m_pSerialPort->print(PROGMEM("SSID:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctWifiSettings.cSSID);
        m_pSerialPort->print(PROGMEM("IP:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctWifiStatus.cIP);
        m_pSerialPort->print(PROGMEM(PROGMEM("GATEWAY:")));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctWifiStatus.cGateway);
        m_pSerialPort->print(PROGMEM("MASK:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctWifiStatus.cMask);
        m_pSerialPort->print(PROGMEM("MAC:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctWifiStatus.cMAC);
        m_pSerialPort->println(PROGMEM("----API SETTINGS----"));
        m_pSerialPort->print(PROGMEM("Hostanme:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cHostname);
        m_pSerialPort->print(PROGMEM("Port:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiPort);
        m_pSerialPort->print(PROGMEM("URI:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cUri);
        m_pSerialPort->print(PROGMEM("Client ID:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cClientID);
        m_pSerialPort->print(PROGMEM("Authorization token:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.cAuthorizationToken);
        m_pSerialPort->print(PROGMEM("Keep-alive timeout (min):"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctBridgeSettings.strctAPIServerSettings.uiKeepAliveTimeout);
#endif
        m_pSerialPort->println(PROGMEM("----RADIO SETTINGS----"));
        m_pSerialPort->print(PROGMEM("Device ID:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiDeviceID);
#ifndef BRIDGE_MODE
        m_pSerialPort->print(PROGMEM("Server ID:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiServerID);
#endif
        m_pSerialPort->print(PROGMEM("Output Power:"));
        switch (m_pGlobalSettingsAndStatus->strctRadioSettings.uiOutputPower) {
            case 1:
            m_pSerialPort->println(PROGMEM("-30dbm"));
            break;

            case 2:
            m_pSerialPort->println(PROGMEM("-20dbm"));
            break;

            case 3:
            m_pSerialPort->println(PROGMEM("-15dbm"));
            break;

            case 4:
            m_pSerialPort->println(PROGMEM("-10dbm"));
            break;

            case 5:
            m_pSerialPort->println(PROGMEM("0dbm"));
            break;

            case 6:
            m_pSerialPort->println(PROGMEM("5dbm"));
            break;

            case 7:
            m_pSerialPort->println(PROGMEM("7dbm"));
            break;

            case 8:
            m_pSerialPort->println(PROGMEM("10dbm"));
            break;

            default:
            m_pSerialPort->println(PROGMEM(""));
            break;
        }
        m_pSerialPort->print(PROGMEM("Max Retries:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiMaxRetries);
        m_pSerialPort->print(PROGMEM("Message signature:"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiMessageSignature);
        m_pSerialPort->print(PROGMEM("Keep-alive timeout (min): "));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctRadioSettings.uiKeepAliveTimeout);
        m_pSerialPort->println(PROGMEM("----MISCELLANEOUS----"));
        m_pSerialPort->print(PROGMEM("Sensor values measurement timeout (min):"));
        m_pSerialPort->println(m_pGlobalSettingsAndStatus->strctMiscellaneousSettings.uiReadSensorValuesMeasurementTimeout);
        goto ok;
    }

    if (isDoCommand(AT_SAVE_SETTINGS)) {
        xEventGroupSetBits(*m_pxEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SAVE_SETTINGS);
        goto ok;
    }

    if (isDoCommand(AT_FACTORY_RESET)) {
        xEventGroupSetBits(*m_pxEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_FACTORY_RESET);
        goto ok;
    }

    //Disable Echo
    if (isSetCommand(AT_ECHO)) {
        if (isParamEqualTo(PROGMEM("1"))) {
            m_bEchoEnabled = true;
        } else {
            if (isParamEqualTo(PROGMEM("0"))) {
                m_bEchoEnabled = false;
            } else {
                goto error;
            }
        }

        goto ok;
    }

    //Enable echo
    if (isGetCommand(AT_ECHO)) {
        m_pSerialPort->println(m_bEchoEnabled ? PROGMEM("ECHO ENABLED") : PROGMEM("ECHO DISABLED"));
        goto ok;
    }

    //AT commands usage
    if (isDoCommand(AT_USAGE)) {
        m_pSerialPort->println(PROGMEM("----GET & SET (AT+COMMAND? & AT+COMMAND=)----"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_ECHO);
        m_pSerialPort->println(PROGMEM(": 1: enable | 0: disable"));
#ifdef BRIDGE_MODE
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_API_HOSTNAME);
        m_pSerialPort->println(PROGMEM(": Host URI without 'https://' - shall be HTTPS"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_API_AUTHORIZATION);
        m_pSerialPort->println(PROGMEM(": Host token - can be empty"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_API_URI);
        m_pSerialPort->println(PROGMEM(": API URI starting with '/'"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_API_PORT);
        m_pSerialPort->println(PROGMEM(": port number"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_API_CLIENT_ID);
        m_pSerialPort->println(PROGMEM(": client ID - can't be empty"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_API_KEEP_ALIVE_TIMEOUT);
        m_pSerialPort->println(PROGMEM(": API-Server keep-alive frequency (minutes)"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_AP_SSID);
        m_pSerialPort->println(PROGMEM(": access point SSID"));
#endif
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_RADIO_DEVICE_ID);
        m_pSerialPort->println(PROGMEM(": Device ID - from 1 to 127"));
#ifndef BRIDGE_MODE
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_RADIO_SERVER_ID);
        m_pSerialPort->println(PROGMEM(": Server ID - from 1 to 200"));
#endif
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_RADIO_OUTPUT_PWR);
        m_pSerialPort->println(PROGMEM(": 1:-30dbm, 2:-20dbm, 3:-15dbm, 4:-10dbm, 5:0dbm, 6:5dbm, 7:7dbm, 8:10dbm"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_RADIO_MAX_RETRIES);
        m_pSerialPort->println(PROGMEM(":  retry attempts for unachowledged message"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_RADIO_MESSAGE_SIGNATURE);
        m_pSerialPort->println(PROGMEM(": token for radio message verification (WORD=2 bytes value only) - can't be null"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_RADIO_KEEP_ALIVE_TIMEOUT);
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->println(PROGMEM(": bridge-server keep-alive frequency (minutes)"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_MISCELLANEOUS_SENSOR_MEASUREMENT_TIMEOUT);
        m_pSerialPort->println(PROGMEM(": Sensor values measurement frequency (minutes)"));

        m_pSerialPort->println(PROGMEM("---SET ONLY (AT+COMMAND=)----"));
#ifdef BRIDGE_MODE
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_AP_KEY);
        m_pSerialPort->println(PROGMEM(": access point key"));
#endif
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_JSON_SETTINGS);
        m_pSerialPort->println(PROGMEM(": send full setting in JSON format - cf doc"));

        m_pSerialPort->println(PROGMEM("---GET ONLY (AT+COMMAND?)----"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_JSON_STATUS);
        m_pSerialPort->println(PROGMEM(": status and settings from a JSON format"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_STATUS);
        m_pSerialPort->println(PROGMEM(": status and settings"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_VERSION);
        m_pSerialPort->println(PROGMEM(": software version"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_UID);
        m_pSerialPort->println(PROGMEM(": Unique CPU ID"));
        m_pSerialPort->print(AT_DEVICE_TYPE);
        m_pSerialPort->println(PROGMEM(": Device type, bridge or sensor"));
        m_pSerialPort->print(AT_SENSOR_VALUES);
        m_pSerialPort->println(PROGMEM(": measure and return sensor values"));

        m_pSerialPort->println(PROGMEM("---DO (AT+COMMAND)---"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_SAVE_SETTINGS);
        m_pSerialPort->println(PROGMEM(": perform settings flash-saving, otherwise, new settings will be lost after reboot - reboot mandatory in order to manage with new settings"));
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(AT_FACTORY_RESET);
        m_pSerialPort->print(PROGMEM(": clear settings into flash"));
#ifdef BRIDGE_MODE
        m_pSerialPort->print(AT_PREFIXE_COMMAND);
        m_pSerialPort->print(PROGMEM(" and clear access point ssid and key into wifi module"));
#endif
        m_pSerialPort->println(PROGMEM(" - reboot mandatory"));
        goto ok;
    }

    m_pSerialPort->println(PROGMEM("UNAUTHORIZED"));
    m_pSerialPort->println("");
    return;

length_error:
    m_pSerialPort->println(PROGMEM("ERROR LENGTH"));
    m_pSerialPort->println("");
    return;

error:
    m_pSerialPort->println(PROGMEM("ERROR"));
    m_pSerialPort->println("");
    return;

ok:
    m_pSerialPort->println(PROGMEM("OK"));
    m_pSerialPort->println("");
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
*   check if method is GET
*   params: 
*       p_pcCommand:        AT COMMAND
*   return:
*       TRUE if GET command       
*/
boolean CATSettings::isGetCommand(const char *p_pcCommand) {
    return ((strcmp(m_strctATCommand.pcCommand, p_pcCommand) == 0) && (m_strctATCommand.enmMethod == CATSettings::ENM_METHOD::GET));
}

/**
*   check if method is SET
*   params: 
*       p_pcCommand:        AT COMMAND
*   return:
*       TRUE if SET command       
*/
boolean CATSettings::isSetCommand(const char *p_pcCommand) {
    return ((strcmp(m_strctATCommand.pcCommand, p_pcCommand) == 0) && (m_strctATCommand.enmMethod == CATSettings::ENM_METHOD::SET));
}

/**
*   check if method is DO
*   params: 
*       p_pcCommand:        AT COMMAND
*   return:
*       TRUE if DO command       
*/
boolean CATSettings::isDoCommand(const char *p_pcCommand) {
    return ((strcmp(m_strctATCommand.pcCommand, p_pcCommand) == 0) && (m_strctATCommand.enmMethod == CATSettings::ENM_METHOD::DO));
}

/**
*   check if AT-SET param is equal to a string
*   params: 
*       p_pcParam:        string to compare
*   return:
*       TRUE if equal      
*/
boolean CATSettings::isParamEqualTo(const char *p_pcParam) {
    return (strncmp(m_strctATCommand.pcParam, p_pcParam, strlen(m_strctATCommand.pcParam)) == 0);
}

/**
*   check if AT-SET param length is consistent, meaning equal or below to the reserved length  
*   params: 
*       p_sztMaxLength:        length to compare
*   return:
*       TRUE if param length fits the reserver array      
*/
boolean CATSettings::isParamLengthConsistent(size_t p_sztMaxLength) {
    return (strlen(m_strctATCommand.pcParam) <= p_sztMaxLength);
}

/**
*   check if a char array is numeric  
*   params: 
*       p_pcValue:        char array to check
*   return:
*       TRUE if char array is a numeric value      
*/
boolean CATSettings::isParamNumericValue(char *p_pcValue) {
    char *l_pcEnd;

    return (strtol(p_pcValue, &l_pcEnd, 10) != 0L);
}

/**
*   return the numeric value of a char array  
*   params: 
*       p_pcValue:        char array to convert to numeric
*   return:
*       numeric value      
*/
long  CATSettings::getParamNumericValue(char *p_pcValue) {
    char *l_pcEnd;

    return strtol(p_pcValue, &l_pcEnd, 10);
}

/**
*   return a value from a key inside an AT-SET param corresponding to a json
*   params: 
*       p_pcKey:                key to search
*   return:
*       pointer to a char array containing the value, otherwise NULL      
*/
char * CATSettings::getJsonValueFromKey(const char *p_pcKey) {
    char *l_pcStart, *l_pcEnd;
    char l_cKeyBuffer[40] = "\"";

    strncat(&l_cKeyBuffer[1], p_pcKey, strlen(p_pcKey));
    strncat(&l_cKeyBuffer[1 + strlen(p_pcKey)], "\":\"", strlen("\":\""));
    
    if ((l_pcStart = strstr(m_strctATCommand.pcParam, l_cKeyBuffer)) != NULL ) {
        if ((l_pcEnd = strstr(l_pcStart + strlen(l_cKeyBuffer), "\"")) != NULL) {
            memcpy(&m_cBufferMiscallaneous[0], l_pcStart + strlen(l_cKeyBuffer), l_pcEnd - (l_pcStart + strlen(l_cKeyBuffer)));
            m_cBufferMiscallaneous[l_pcEnd - (l_pcStart + strlen(l_cKeyBuffer))] = '\0';
            return &m_cBufferMiscallaneous[0];
        }
    }

    return NULL;
}
