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
 *	Author: Gilles PELIZZO (https://www.linkedin.com/in/pelizzo/)
 *	Date: November 17th, 2020.
 */

/**
 * refer to EXPRESSIVE datasheet. cf https://www.espressif.com/sites/default/files/documentation/0a-esp8266ex_datasheet_en.pdf
 * 
 */

#include "CESP8266.h"

#ifdef BRIDGE_MODE

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
*   Init device: start and initialize ESP8266 module  
*   params: 
*       p_pSerialPort:              pointer to the UART on which ESP module is connected
*       p_ulBaudeRate:              baude rate of the communication
*       p_pcSSID:                   SSID of the AP (access point) to connect on
*       p_pcPassword:               Password to connect to the AP
*   return:
*       cf CESP8266::ENM_STATUS   
*/
CESP8266::ENM_STATUS CESP8266::init(Uart *p_pSerialPort, unsigned long p_ulBaudeRate, STRUCT_WIFI_SETTINGS *p_pstrctWifiSettings, STRUCT_WIFI_STATUS *p_pstrctWifiStatus){
    
    m_pSerialPort = p_pSerialPort;
    m_pSerialPort->begin(p_ulBaudeRate);
    m_pstrctWifiSettings = p_pstrctWifiSettings;
    m_pstrctWifiStatus = p_pstrctWifiStatus;

    m_sztATBufferResponseUsedLength = 0;

    memset(&m_cATBufferResponse[0], 0, MAX_AT_BUFFER_RESPONSE_LENGTH);
    memset(&m_cBufferATCommands[0], 0, MAX_BUFFER_AT_COMMANDS_LENGTH);
    memset(&m_cBufferATExpectedResponses[0], 0, MAX_BUFFER_AT_EXPETED_RESPONSES_LENGTH);
    memset(&m_cBufferTinyMiscellaneous[0], 0, MAX_ESP8266_TINY_MISCELLANEOUS_LENGTH);
    memset(&m_cBufferLargeMiscellaneous[0], 0, MAX_ESP8266_LARGE_MISCELLANEOUS_LENGTH);
    
    m_toolBufferATCommands.init(&m_cBufferATCommands[0], MAX_BUFFER_AT_COMMANDS_LENGTH);
    m_toolBufferATExpectedResponses.init(&m_cBufferATExpectedResponses[0], MAX_BUFFER_AT_EXPETED_RESPONSES_LENGTH);
    m_toolBufferTinyMiscellaneous.init(&m_cBufferTinyMiscellaneous[0], MAX_ESP8266_TINY_MISCELLANEOUS_LENGTH);
    m_toolBufferLargeMiscellaneous.init(&m_cBufferLargeMiscellaneous[0], MAX_ESP8266_LARGE_MISCELLANEOUS_LENGTH);

    
    return restartModule();
    
}

/**
*   check if an incomming packet is pending. Module must have been set as a passive server, listening to a port.
*
*   params: 
*       NONE
*   return:
*       c.f. STRCT_REQUEST (including header and request data). NULL if an error occured      
*/
CESP8266::STRCT_REQUEST *CESP8266::available() {

    //poll module and check if incoming data are pending (cf AT+CIPRECVLEN?)
    CESP8266::ENM_AT_STATUS l_enmATStatusRet = pollIncommingLength();
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "available/pollIncommingLength", (l_enmATStatusRet == CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) ? "OK" : "NOK");
    if (l_enmATStatusRet != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "available/pollIncommingLength", "FAILED");
        return NULL;
    }

    if (m_strctRequest.uiLength > 0) {
        l_enmATStatusRet = retreiveIncomingData(&m_strctRequest.pData);
        if (l_enmATStatusRet != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
            LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "available/retreiveIncomingData", "FAILED");
            return NULL;
        }

        CESP8266::ENM_PARSE_BUFFER l_enmParseRetValue;
        //extract header and body and update member variable 'm_strctRequest'
        if ((l_enmParseRetValue = parseHeaderAndBody(&m_strctRequest)) != PARSE_SUCCEEDED) {
            LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "available/parseHeaderAndBody: ", (l_enmParseRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) ? "OK" : "NOK");
            if (l_enmParseRetValue != CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
                LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "available/parseHeaderAndBody => ", l_enmParseRetValue);
                return NULL;
            }
        }

        m_strctRequest.uiLength = 0;

        return &m_strctRequest;
    }

    return NULL;
}


/**
*   Enable server mode, meaning listening on defined port. 2 modes are available:
*   1) active: incoming data are passed-through the serial UART
*   2) passive: incoming data are kept stored into the module buffer. AT+CIPRECVLEN polling is required to get len of stored data before getting them back
*   IMPORTANT: multiple connections mode must be enabled; cf AT+CIPMUX
*
*   params: 
*       p_uiListeningPort:          port number to listen
*       p_bPassiveMode:             'TRUE' for passive mode. 'FALSE' for active mode
*       p_pcSSID:                   SSID of the AP (access point) to connect on
*       p_pcPassword:               Password to connect to the AP
*   return:
*       cf ENM_AT_STATUS    
*/
CESP8266::ENM_STATUS CESP8266::enableServerMode(uint16_t p_uiListeningPort,  boolean p_bPassiveMode) {
    //set receive mode
    m_toolBufferATCommands.start("AT+CIPRECVMODE=");
    m_toolBufferATCommands.concat((p_bPassiveMode ? "1" : "0"));

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), "FAILED");
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    //create server and listen port
    itoa(p_uiListeningPort, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferATCommands.start("AT+CIPSERVER=1,");
    m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), "FAILED");
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    m_enmServerMode = (p_bPassiveMode ? CESP8266::ENM_SERVER_MODE::SERVER_MODE_ENABLED_PASSIVE : CESP8266::ENM_SERVER_MODE::SERVER_MODE_ENABLED_ACTIVE);
    

    return CESP8266::ENM_STATUS::SUCEEDED;
}

/**
*   Disable server mode.
*
*   params: 
*       NONE
*   return:
*       cf ENM_AT_STATUS    
*/
CESP8266::ENM_STATUS CESP8266::disableServerMode() {
    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand("AT+CIPSERVER=0", &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPSERVER=0", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPSERVER=0", "FAILED");
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    m_enmServerMode = CESP8266::ENM_SERVER_MODE::SERVER_MODE_DISABLED;

   return CESP8266::ENM_STATUS::SUCEEDED;
}

/**
*   get the mode of the server. Check if enabled or not
*
*   params: 
*       NONE
*   return:
*       one of the 3 bellow values:
*           SERVER_MODE_ENABLED_ACTIVE
*           SERVER_MODE_ENABLED_PASSIVE 
*           SERVER_MODE_DISABLED     
*/
CESP8266::ENM_SERVER_MODE CESP8266::getServerMode() {
    return m_enmServerMode;
}

/**
*   retreive the index of a string into another one
*
*   params: 
*       p_pcBuffer:                 String to parse
*       p_pcSearch:                 String to search 
*       p_sztFromIndex:             Starting index 
*       p_sztBufferLength:          length of the buffer to search into
*   return:
*       position of the string to search. -1 if not founded  
*/
int CESP8266::indexOf(char *p_pcBuffer, const char *p_pcSearch, size_t p_sztFromIndex, size_t p_sztBufferLength) {
    size_t l_sztBufferIndex = p_sztFromIndex;
    size_t l_sztSearchIndex = 0;

    do {
        if (*(p_pcBuffer + l_sztBufferIndex + l_sztSearchIndex) == *(p_pcSearch + l_sztSearchIndex)) {
            l_sztSearchIndex++;
            if (l_sztSearchIndex >= strlen(p_pcSearch)) {
                return l_sztBufferIndex;
            }
        } else {
            l_sztBufferIndex++;
            l_sztSearchIndex = 0;
        }
    } while ((l_sztBufferIndex + l_sztSearchIndex) <= p_sztBufferLength);
    
    return -1;
}

/**
*   Tool to extract a value from a json corresponding to a key
*
*   params: 
*       p_pcEntryBuffer:            String to parse = json including or not opening and closing cUriy brackets '{' & '}' 
*       p_pcKey:                    key to parse
*       p_pcRetString:              storage buffer to return value
*   return:
*       cf CESP8266::ENM_PARSE_BUFFER 
*/
boolean CESP8266::getJSONValue(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString) {
    return (parseJSON(p_pcEntryBuffer, p_pcKey, "\"", "\"", p_pcRetString) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) ? true : false;
}

/**
*   Tool to extract a json object from a json corresponding to a key
*
*   params: 
*       p_pcEntryBuffer:            String to parse = json, must include opening and closing cUriy brackets '{' & '}' 
*       p_pcKey:                    key to parse
*       p_pcRetString:              storage buffer to return json object without opening and closing cUriy brackets
*   return:
*       cf CESP8266::ENM_PARSE_BUFFER 
*/
boolean CESP8266::getJSONObject(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString) {
   return (parseJSON(p_pcEntryBuffer, p_pcKey, "{", "}", p_pcRetString) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) ? true : false;
}

/**
*   Tool to extract an array object from a json corresponding to a key
*
*   params: 
*       p_pcEntryBuffer:            Char buffer to parse = String including opening and closing square brackets '[' & ']' 
*       p_pcKey:                    key to parse
*       p_pcRetString:              storage buffer to return array object without opening and closing square brackets
*   return:
*       cf CESP8266::ENM_PARSE_BUFFER 
*/
boolean CESP8266::getJSONArrayObject(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString) {
    return (parseJSON(p_pcEntryBuffer, p_pcKey, "[", "]", p_pcRetString) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) ? true : false;
}

/**
*   Tool to extract a param value from an Uri corresponding to a key
*
*   params: 
*       p_pcEntryBuffer:            Char buffer to parse = String including '?' and '&' 
*       p_pcKey:                    key to parse
*       p_pcRetString:              storage buffer to return value
*   return:
*       cf CESP8266::ENM_PARSE_BUFFER 
*/
boolean CESP8266::getUriParam(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString) {
    //this is a workaround to simplify parse process
    m_toolBufferTinyMiscellaneous.start(p_pcEntryBuffer);
    m_toolBufferTinyMiscellaneous.concat("&");

    return (parseUri(m_toolBufferTinyMiscellaneous.getBuffer(), p_pcKey, p_pcRetString) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) ? true : false;
}

/**
*   Send a POST request to host and wait for reply. This request passes a json to the host
*
*   params: 
*       p_pcHostName:               hostname, IP address or domain
*       p_uiPort:                   port number to send the request
*       p_pcUri;:                   Uri of the request
*       p_bSSL:                     'TRUE' to enable SSL communication (HTTPS)
*       p_pcData:                   json to pass to the host. Must include opening and closing cUriy bracket '{' & '}'         
*   return:
*       c.f. STRCT_RESPONSE (including header and response). NULL if an error occured      
*/
CESP8266::STRCT_RESPONSE *CESP8266::postJsonToHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL, char *p_pcData) {
    return postJsonToHost(p_pcHostName, p_uiPort, p_pcUri, p_bSSL, p_pcData, NULL);
}

/**
*   Send a POST request to host and wait for reply. This request passes a json to the host
*
*   params: 
*       p_pcHostName:               hostname, IP address or domain
*       p_uiPort:                   port number to send the request
*       p_pcUri;:                   Uri of the request
*       p_bSSL:                     'TRUE' to enable SSL communication (HTTPS)
*       p_pcData:                   json to pass to the host. Must include opening and closing cUriy bracket '{' & '}'         
*       p_pcAuthorization:          authorization token to pass to the host
*   return:
*       c.f. STRCT_RESPONSE (including header and response). NULL if an error occured      
*/
CESP8266::STRCT_RESPONSE *CESP8266::postJsonToHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL, char *p_pcData, char *p_pcAuthorization) {
    
    //establish TCP connection (cf AT+CIPSTART=) 
    CESP8266::ENM_AT_STATUS l_enmATStatusRetValue = sendStart(p_pcHostName, p_uiPort, p_pcUri, p_bSSL);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost: ", (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) ? "OK" : "NOK");
    
    //ugly workaround for the common ESP8266 'busy p...' issue !
    /*if ((l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_ERROR_BUSY) || (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_ERROR_CIPSTART)){
        restartModule();
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "restart module (busy) => ", l_enmATStatusRetValue);
        return NULL;
    }*/
    
    //ugly workaround for the common ESP8266 'busy p...' issue !
    if (l_enmATStatusRetValue != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        restartModule();
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost => ", l_enmATStatusRetValue);
        return NULL;
    }

    //build POST request
    m_toolBufferLargeMiscellaneous.start("POST ");
    m_toolBufferLargeMiscellaneous.concat(p_pcUri);
    m_toolBufferLargeMiscellaneous.concat(" HTTP/1.1\r\n");
    m_toolBufferLargeMiscellaneous.concat("Host: ");
    m_toolBufferLargeMiscellaneous.concat(p_pcHostName);
    m_toolBufferLargeMiscellaneous.concat("\r\n");
    if (strlen(p_pcAuthorization) != 0) {
        m_toolBufferLargeMiscellaneous.concat("Authorization: ");
        m_toolBufferLargeMiscellaneous.concat(p_pcAuthorization);
        m_toolBufferLargeMiscellaneous.concat("\r\n");
    }
    m_toolBufferLargeMiscellaneous.concat("Content-Length: ");
    itoa(strlen(p_pcData), m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat("\r\n");
    m_toolBufferLargeMiscellaneous.concat("Connection: close\r\n");
    m_toolBufferLargeMiscellaneous.concat("Content-Type: application/json\r\n\r\n");
    m_toolBufferLargeMiscellaneous.concat(p_pcData);
    m_toolBufferLargeMiscellaneous.concat("\r\n");

    //send data (cf AT+CIPSEND=): transmit length to send, wait for prompt '>',  send data, wait for 'CLOSED and wait for reply    
    l_enmATStatusRetValue = sendData(OUTGOING_LINK_ID, m_toolBufferLargeMiscellaneous.getBuffer());
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/sendData", (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) ? "OK" : "NOK");
    if (l_enmATStatusRetValue != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/sendData", l_enmATStatusRetValue);
        return NULL;
    }
  /*  
    m_toolBufferATExpectedResponses.startList("CLOSED\r\n");
    int8_t l_iRetValue = readNextRemaingATResponse(&m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "WAIt FOR CLOSE", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "WAIt FOR CLOSE", l_iRetValue);
        return NULL;
    }
*/
    CESP8266::ENM_PARSE_BUFFER l_enmParseRetValue;
    if ((l_enmParseRetValue = parseHeaderAndBody(&m_strctResponse)) != CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/parseHeaderAndBody: ", (l_enmParseRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) ? "OK" : "NOK");
        if (l_enmParseRetValue != CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
            LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/parseHeaderAndBody => ", l_enmParseRetValue);
            return NULL;
        }
    }

    return &m_strctResponse;
}

/**
*   Send a GET request to host and wait for reply. This request doesn't include data. Params must be passed by the Uri, e.g. Uri?key1=value1&key2=value2
*
*   params: 
*       p_pcHostName:               hostname, IP address or domain
*       p_uiPort:                   port number to send the request
*       p_pcUri;:                   Uri of the request
*       p_bSSL:                     'TRUE' to enable SSL communication (HTTPS)
*   return:
*       cf CESP8266::STRCT_RESPONSE    
*/
CESP8266::STRCT_RESPONSE *CESP8266::getFromHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL) {
    return getFromHost(p_pcHostName, p_uiPort, p_pcUri, p_bSSL, NULL);
}

/**
*   Send a GET request to host and wait for reply. This request doesn't include data. Params must be passed by the Uri, e.g. Uri?key1=value1&key2=value2
*
*   params: 
*       p_pcHostName:               hostname, IP address or domain
*       p_uiPort:                   port number to send the request
*       p_pcUri;:                   Uri of the request
*       p_bSSL:                     'TRUE' to enable SSL communication (HTTPS)
*       p_pcAuthorization:          authorization token to pass to the host
*   return:
*       cf CESP8266::STRCT_RESPONSE    
*/
CESP8266::STRCT_RESPONSE *CESP8266::getFromHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL, char *p_pcAuthorization) {

    //establish TCP connection (cf AT+CIPSTART=) 
    CESP8266::ENM_AT_STATUS l_enmATStatusRetValue = sendStart(p_pcHostName, p_uiPort, p_pcUri, p_bSSL);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost: ", (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) ? "OK" : "NOK");

    //ugly workaround for the common ESP8266 'busy p...' issue !
    if (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_ERROR_BUSY) {
        restartModule();
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "restart module (busy) => ", l_enmATStatusRetValue);
        return NULL;
    }

    if (l_enmATStatusRetValue != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost => ", l_enmATStatusRetValue);
        return NULL;
    }

    //build GET request
    m_toolBufferLargeMiscellaneous.start("GET ");
    m_toolBufferLargeMiscellaneous.concat(p_pcUri);
    m_toolBufferLargeMiscellaneous.concat(" HTTP/1.1\r\n");
    m_toolBufferLargeMiscellaneous.concat("Host: ");
    m_toolBufferLargeMiscellaneous.concat(p_pcHostName);
    m_toolBufferLargeMiscellaneous.concat("\r\n");
    if (strlen(p_pcAuthorization) != 0) {
        m_toolBufferLargeMiscellaneous.concat("Authorization: ");
        m_toolBufferLargeMiscellaneous.concat(p_pcAuthorization);
        m_toolBufferLargeMiscellaneous.concat("\r\n");
    }
    m_toolBufferLargeMiscellaneous.concat("Connection: close\r\n");

    //send data (cf AT+CIPSEND=): transmit length to send, wait for prompt '>',  send data, wait for 'CLOSED and wait for reply    
    l_enmATStatusRetValue = sendData(OUTGOING_LINK_ID, m_toolBufferLargeMiscellaneous.getBuffer());
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/sendData", (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) ? "OK" : "NOK");
    if (l_enmATStatusRetValue != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/sendData", l_enmATStatusRetValue);
        return NULL;
    }
    
    m_toolBufferATExpectedResponses.startList("CLOSED\r\n");
    int8_t l_iRetValue = readNextRemaingATResponse(&m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "WAIt FOR CLOSE", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "WAIt FOR CLOSE", l_iRetValue);
        return NULL;
    }

    CESP8266::ENM_PARSE_BUFFER l_enmParseRetValue;
    if ((l_enmParseRetValue = parseHeaderAndBody(&m_strctResponse)) != CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/parseHeaderAndBody: ", (l_enmParseRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) ? "OK" : "NOK");
        if (l_enmParseRetValue != CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
            LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "postJsonToHost/parseHeaderAndBody => ", l_enmParseRetValue);
            return NULL;
        }
    }

    return &m_strctResponse;
}

/**
*   Restore module default factory settings
*
*   params: 
*       NONE
*   return:
*       cf ENM_STATUS 
*/
CESP8266::ENM_STATUS CESP8266::restoreFactoryDefaultSettings() { 
    //clear module FLASH and former parameters, incl. SSID, password, etc.
    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand("AT+RESTORE", &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "AT+RESTORE", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+RESTORE", "FAILED");
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    return CESP8266::ENM_STATUS::SUCEEDED;
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

CESP8266::ENM_STATUS CESP8266::restartModule() {
    CESP8266::ENM_AT_STATUS l_ATCmdStatus;
    boolean l_bWifiConnectedAndGotIP = true;

    digitalWrite(ESP8266_RESET_PIN, LOW);
    delay(500);
    digitalWrite(ESP8266_RESET_PIN, HIGH);

    /*
    l_ATCmdStatus = restoreFactoryDefaultSettings();
    if (l_ATCmdStatus != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }
    */

    //start with a soft-reset of the module and check if module can auto-connect to AP
    l_ATCmdStatus = resetModule();
    if (l_ATCmdStatus != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        if (l_ATCmdStatus == CESP8266::ENM_AT_STATUS::AT_NOT_AUTOCONNECTED_TO_AP) {
            l_bWifiConnectedAndGotIP = false;
        } else {
            return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
        }
    }

    //disable echo 
    if ((l_ATCmdStatus = setEchoMode(false)) != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    //connect to WIFI access point
    if (!l_bWifiConnectedAndGotIP) {
        if ((l_ATCmdStatus = connectToAP(m_pstrctWifiSettings->cSSID, m_pstrctWifiSettings->cKey)) != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
            return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
        }
    }
    
    //retreive current IP; including workaround when module reply by "busy..." to the 1rs request
    byte l_byMaxRetry = 3;
    while (true) {
        if ((l_ATCmdStatus = retreiveCurrentIP()) != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
            if (l_byMaxRetry-- <= 0) {
                return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
            } else {
                delay(2000);
            }
        } else {
            break;
        }
    }

    //retreive module MAC Addr
    if ((l_ATCmdStatus = retreiveMACAddr()) != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    //Set SSL Buffer size: range value [2048, 4096]. In that case, vecause all resquested are performed
    //using SSL, Buffer must be increased to 4096
    if ((l_ATCmdStatus = setSSLBufferSize(4096)) != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    //set connection mode: true = multiple connections (IMPORTANT: multilple connection mode is required for server mode)
    if ((l_ATCmdStatus = setConnectionsMode(true)) != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        return CESP8266::ENM_STATUS::ERROR_AT_EXPECTED_RESPONSES;
    }

    return CESP8266::ENM_STATUS::SUCEEDED;
}


/**
*   Set connection mode, meaning enabling or disabling multiple connection. 
*   IMPORTANT: multilple connections is required for server mode
*
*   params: 
*       p_bMultiple:                'TRUE' to enable multiple connections 
*   return:
*       cf ENM_AT_STATUS 
*/
CESP8266::ENM_AT_STATUS CESP8266::setConnectionsMode(boolean p_bMultiple) {
    m_toolBufferATCommands.start("AT+CIPMUX=");
    m_toolBufferATCommands.concat((p_bMultiple ? "1" : "0"));

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

   return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Set SSL buffer size. Must be increase in order to hot SSL certificat
*
*   params: 
*       p_uiSSLBufferSize:          SSL Buffer size, 2048 or 4096 
*   return:
*       cf ENM_AT_STATUS 
*/
CESP8266::ENM_AT_STATUS CESP8266::setSSLBufferSize(uint16_t p_uiSSLBufferSize) {
    itoa(p_uiSSLBufferSize, m_toolBufferTinyMiscellaneous.getBuffer(), 10);

    m_toolBufferATCommands.start("AT+CIPSSLSIZE=");
    m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

   return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}


/**
*   Retreive MAC address and fill variable member 'm_strctModuleSettings'
*
*   params: 
*       NONE
*   return:
*       cf ENM_AT_STATUS
*/
CESP8266::ENM_AT_STATUS CESP8266::retreiveMACAddr() { 

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand("AT+CIPSTAMAC?", &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPSTAMAC?", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPSTAMAC?", "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

    char * l_pcRetData = NULL;
    uint16_t l_uiLengthData;
    CESP8266::ENM_PARSE_BUFFER l_paseBufferRetValue;

    l_paseBufferRetValue = parseBuffer(&m_cATBufferResponse[0], "+CIPSTAMAC:", "\r", -1, &l_pcRetData, &l_uiLengthData, m_sztATBufferResponseUsedLength);
    if (l_paseBufferRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcRetData+1, l_uiLengthData-2);
        strcpy(&m_pstrctWifiStatus->cMAC[0], m_toolBufferTinyMiscellaneous.getBuffer());
    } else {
        strcpy(&m_pstrctWifiStatus->cMAC[0], "");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_PARSE;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Retreive IP address and other params provided by the Access Point, and fill variable member 'm_strctModuleSettings'
*
*   params: 
*       NONE
*   return:
*       cf ENM_AT_STATUS
*/
CESP8266::ENM_AT_STATUS CESP8266::retreiveCurrentIP() {
    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand("AT+CIPSTA?", &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPSTA?", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPSTA?", "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

    char * l_pcRetData = NULL;
    uint16_t l_uiLengthData;
    CESP8266::ENM_PARSE_BUFFER l_paseBufferRetValue;

    l_paseBufferRetValue = parseBuffer(&m_cATBufferResponse[0], "+CIPSTA:ip:", "\r", -1, &l_pcRetData, &l_uiLengthData, m_sztATBufferResponseUsedLength);
    if (l_paseBufferRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcRetData+1, l_uiLengthData-2);
        strcpy(&m_pstrctWifiStatus->cIP[0], m_toolBufferTinyMiscellaneous.getBuffer());
    } else {
        strcpy(&m_pstrctWifiStatus->cIP[0], "");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_PARSE;
    }

    l_paseBufferRetValue = parseBuffer(&m_cATBufferResponse[0], "+CIPSTA:gateway:", "\r", -1, &l_pcRetData, &l_uiLengthData, m_sztATBufferResponseUsedLength);
    if (l_paseBufferRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcRetData+1, l_uiLengthData-2);
        strcpy(&m_pstrctWifiStatus->cGateway[0], m_toolBufferTinyMiscellaneous.getBuffer());
    } else {
        strcpy(&m_pstrctWifiStatus->cGateway[0], "");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_PARSE;
    }

    l_paseBufferRetValue = parseBuffer(&m_cATBufferResponse[0], "+CIPSTA:netmask:", "\r", -1, &l_pcRetData, &l_uiLengthData, m_sztATBufferResponseUsedLength);
    if (l_paseBufferRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcRetData+1, l_uiLengthData-2);
        strcpy(&m_pstrctWifiStatus->cMask[0], m_toolBufferTinyMiscellaneous.getBuffer());
    } else {
        strcpy(&m_pstrctWifiStatus->cMask[0], "");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_PARSE;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Reset module and wait for access point automatic connection if credentials have been stored
*
*   params: 
*       NONE
*   return:
*       cf ENM_AT_STATUS
*/
CESP8266::ENM_AT_STATUS CESP8266::resetModule() { 
    m_toolBufferATExpectedResponses.startList("ready\r\n", CCharBufferTool::ENM_LIST_TYPE::AND);
    m_toolBufferATExpectedResponses.insertToList("WIFI CONNECTED\r\n");
    m_toolBufferATExpectedResponses.insertToList("WIFI GOT IP\r\n");
    int8_t l_iRetValue = sendATCommand("AT+RST", &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "AT+RST", (l_iRetValue > 0) ? "RST + WIFI CONNECTED + WIFI GOT IP" : ((l_iRetValue < - 1) ? "RST" : ((l_iRetValue < - 2) ? "RST + WIFI CONNECTED" : "RST FAILED")));
    if (l_iRetValue <= 0) {
        if (l_iRetValue < -1) {
            if (l_iRetValue < -2) {
                LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+RST", "RST OK, WIFI CONNECETED BUT DIDN'T GET IP");
            } else {
                LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+RST", "RST OK BUT WIFI NOT CONNECTED AND DIDN'T GET IP");
            }
            return CESP8266::ENM_AT_STATUS::AT_NOT_AUTOCONNECTED_TO_AP;
        } else {
            LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+RST", "READY NOT RECEIVED");
            return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
        }
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Enable or disable echo from the module
*
*   params: 
*       p_bEnable:  'TRUE' to enable echo'
*   return:
*       cf ENM_AT_STATUS 
*/
CESP8266::ENM_AT_STATUS CESP8266::setEchoMode(boolean p_bEnable) {
    //enable or disable echo 
    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand(p_bEnable ? "ATE1" :"ATE0", &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, p_bEnable ? "ATE1" :"ATE0", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, p_bEnable ? "ATE1" :"ATE0", "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Connect to Access Point and wait for connection
*
*   params: 
*       p_pcSSID:               AP SSID 
*       p_pcPassword:           WPA key/password 
*   return:
*       cf ENM_AT_STATUS 
*/
CESP8266::ENM_AT_STATUS CESP8266::connectToAP(const char *p_pcSSID, const char *p_pcPassword) {
    //set STATION Mode
    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand("AT+CWMODE_DEF=1", &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "AT+CWMODE_DEF=1", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+CWMODE_DEF=1", "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

    m_toolBufferATCommands.start("AT+CWJAP_DEF=\"");
    m_toolBufferATCommands.concat(p_pcSSID);
    m_toolBufferATCommands.concat("\",\"");
    m_toolBufferATCommands.concat(p_pcPassword);
    m_toolBufferATCommands.concat("\"");

    m_toolBufferATExpectedResponses.startList("OK\r\n", CCharBufferTool::ENM_LIST_TYPE::OR);
    m_toolBufferATExpectedResponses.insertToList("FAIL\r\n");
    l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

    m_toolBufferATExpectedResponses.startList("WIFI CONNECTED\r\n", CCharBufferTool::ENM_LIST_TYPE::AND);
    m_toolBufferATExpectedResponses.insertToList("WIFI GOT IP\r\n");
    l_iRetValue = readNextRemaingATResponse(&m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "WIFI COONECTED AND GOT IP", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "WIFI COONECTED AND GOT IP", "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_RESPONSE;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Read remaining AT reponse from the module and wait for expected response(s)
*
*   params: 
*       p_bufferResponseList:   CCharBufferTool object containg the list of expected response(s) after the AT command
*                               has been sent. This object is a list including a CCharBufferTool::CCharBufferToolENM_LIST_TYPE
*                               parameter telling if expected response(s) must have been all received (AND) or at least one (OR)
*   return:
*       A passed-through SIGNED INT from readATResponse().cf this fontion comment header
*/
int8_t CESP8266::readNextRemaingATResponse(CCharBufferTool *p_pbufferResponseList) {
    return readATResponse(p_pbufferResponseList, false, false);
}


/**
*   Read all remaining AT reponse from the module
*
*   params: 
*       NONE
*   return:
*       AT_RESPONSE_REACH_TIMEOUT or AT_ERROR_BUFFER_RESPONSE_FULL
*/
int8_t CESP8266::readAllRemaingATResponse() {
    return readATResponse(NULL, true, false);
}

/**
*   Send (or not) an AT command and read AT reponse from the module
*
*   params: 
*       p_pccATCommand:         AT command to send. If NULL, function will only perform AT reading from module
*                               This last case (NULL) is used in order to complete Serial reading after an AT command
**                              has been sent and this response(s) retreived 
*       p_bufferResponseList:   CCharBufferTool object containg the list of expected response(s) after the AT command
*                               has been sent. This object is a list including a CCharBufferTool::CCharBufferToolENM_LIST_TYPE
*                               parameter telling if expected response(s) must have been all received (AND) or at least one (OR)
*       p_bWaitLastIncoming:    FALSE to return immediatly after an expected responsed has been received according of 
*                               CCharBufferTool::CCharBufferToolENM_LIST_TYPE parameter (AND / OR), TRUE to continue reading all
*                               remaining and pending AT characters
*   return:
*       A passed-through SIGNED INT from readATResponse().cf this fontion comment header
*/
int8_t CESP8266::sendATCommand(const char *p_pccATCommand, CCharBufferTool *p_pBufferResponseList, boolean p_bWaitLastIncoming) {
    
    //if command is NULL, it means that onlyy readATResponse() will be called without any transmission. COmmand has been
    //sent previoulsy and such request consists on reading all remaining and pending characters from the module
    if (p_pccATCommand != NULL) {
        m_sztATBufferResponseUsedLength = 0;
        m_pSerialPort->flush();
        m_pSerialPort->println(p_pccATCommand);
    }

    //if AT command has to be sent (p_pccATCommand != NULL), tell the function to clear the AT response buffer (m_cATBufferResponse).
    //othserwise, complete the buffer with the pending and remaining characters
    return readATResponse(p_pBufferResponseList, p_bWaitLastIncoming, (p_pccATCommand == NULL) ? false : true);
}

/**
*   Read AT response from the module
*
*   params: 
*       p_bufferResponseList:   CCharBufferTool object containg the list of expected response(s) after the AT command
*                               has been sent. This object is a list including a CCharBufferTool::CCharBufferToolENM_LIST_TYPE
*                               parameter telling if expected response(s) must have been all received (AND) or at least one (OR)
*       p_bWaitLastIncoming:    FALSE to return immediatly after an expected responsed has been received according of 
*                               CCharBufferTool::CCharBufferToolENM_LIST_TYPE parameter (AND / OR), TRUE to continue reading all
*                               remaining and pending AT characters
*       m_cATBufferResponse:   TRUE to clear the response buffer (m_cATBufferResponse). FALSE to keep it entire with previous data    
*                               in order to complete it with remaing and pending characters   
*   return:
*       A passed-through SIGNED INT from parseATResponseBufferResponse().cf this fontion comment header.
*       if value is AT_ERROR_BUFFER_RESPONSE_FULL, it means that m_cATBufferResponse is full and can not 
*       receive remaining characters. If value is AT_ERROR_RESPONSE_TIMEOUT reponse, it means that timeout has been reached
*/
int8_t CESP8266::readATResponse(CCharBufferTool *p_pBufferList, boolean p_bWaitLastIncoming, boolean p_bResetIncomingBuffer) {
    unsigned long   l_ulTimeoutCounter;
    boolean l_bContinueUntilLastTransmisison = p_bWaitLastIncoming;
    int8_t l_iRetValue = AT_RESPONSE_REACH_TIMEOUT;

    l_ulTimeoutCounter = millis();
    if (p_bResetIncomingBuffer) {
        memset(m_cATBufferResponse, 0, sizeof(m_cATBufferResponse));
    }

    while (true) { 
        if (millis() > (l_ulTimeoutCounter + TIMEOUT_READ_AT_RESPONSE)) {
            if (p_bWaitLastIncoming) {
                return l_iRetValue;
            } else {
                return AT_ERROR_RESPONSE_TIMEOUT;
            }
        }

        if (m_pSerialPort->available()) {

            if ((m_sztATBufferResponseUsedLength + 1) > MAX_AT_BUFFER_RESPONSE_LENGTH) {
                return AT_ERROR_BUFFER_RESPONSE_FULL;
            }

            m_cATBufferResponse[m_sztATBufferResponseUsedLength] = m_pSerialPort->read();

#ifdef TRACE_AT_RESPONSE_FORM_MODULE
            LOG_OUTPUT.print(m_cATBufferResponse[m_sztATBufferResponseUsedLength]);
#endif          
            m_sztATBufferResponseUsedLength++;
            if (!l_bContinueUntilLastTransmisison) {
                if ((l_iRetValue = parseATResponseBufferResponse(p_pBufferList)) > 0) {
                    if (!p_bWaitLastIncoming) {
                        return l_iRetValue;
                    } else {
                        l_bContinueUntilLastTransmisison = true;
                    }
                }
            }
        }
    }

    return l_iRetValue;
}

/**
*   Parse the AT response buffer (m_cATBufferResponse) in order to verify if expected response(s) has(have) been received.
*   CCharBufferTool object contains the list of expected responses (obviously, can contain only one).
*   CCharBufferTool object contains also CCharBufferTool::CCharBufferToolENM_LIST_TYPE parameter telling if expected response(s)
*   must has/have been all received (AND) or at least one (OR)
*
*   params: 
*       p_bufferResponseList:   CCharBufferTool object containg the list of expected response(s) after the AT command
*                               has been sent. This object is a list including a CCharBufferTool::CCharBufferToolENM_LIST_TYPE
*                               parameter telling if expected response(s) must have been all received (AND) or at least one (OR)
*   return:
*       The index (starting from 1) of the response into the list. If this value > 0, expected response(s) has/have been received
*       If this value < 0, expected response(s) has/have not been received. Value correspond to the index (starting from 1) into 
*       the list, of the failed response
*/
int8_t CESP8266::parseATResponseBufferResponse(CCharBufferTool *p_pBufferList) {
    char *p_pcPosition = &m_cATBufferResponse[0];

    uint8_t l_uiCounterList;
    for (l_uiCounterList = 0; l_uiCounterList < p_pBufferList->getListCount(); l_uiCounterList++) {

        //p_pcPosition = p_pBufferList->searchString(m_cATBufferResponse, p_pBufferList->getListIndex(l_uiCounterList), m_sztATBufferResponseUsedLength);

        p_pcPosition = searchString(m_cATBufferResponse, p_pBufferList->getListIndex(l_uiCounterList), m_sztATBufferResponseUsedLength);

        //AND expectation. At least one expected command string is not yet into the reponse buffer, then return the negative
        //index (starting from 1) of the reponse string into the list
        if ((p_pcPosition == NULL) && (p_pBufferList->getListType() == CCharBufferTool::ENM_LIST_TYPE::AND)) {
            //return the number 
            return -(l_uiCounterList+1);
        } 

        //OR expectation. AT least one expected command string has been received into the response buffer. Then, return
        //the positive index (starting from 1) of the reponse string into the list
        if ((p_pcPosition != NULL) && (p_pBufferList->getListType() == CCharBufferTool::ENM_LIST_TYPE::OR)) {
            //return the number 
            return (l_uiCounterList+1);
        } 
    }
      
    if (p_pBufferList->getListType() == CCharBufferTool::ENM_LIST_TYPE::OR) {
        //OR expectation. No expected response(s) received. Then, return  negative value
        return -(l_uiCounterList+1);
    } else {
        //AND expectation. All expected response(s) received. Then, return a positive value
        return l_uiCounterList+1;
    }
}

/**
*   search a string into a buffer
*
*   params:    
*       p_pcBuffer:                 buffer to search into
*       p_pcSearch:                 string to search
*       p_sztBufferLength:          length of buffer to search into
*   return:
*       pointer to the string founded, otherwise return NULL
*/
char *CESP8266::searchString(const char *p_pcBuffer, const char *p_pcSearch, size_t p_sztBufferLength) {
    size_t l_sztBufferIndex = 0;
    size_t l_sztSearchIndex = 0;

    do {
        if (*(p_pcBuffer + l_sztBufferIndex + l_sztSearchIndex) == *(p_pcSearch + l_sztSearchIndex)) {
            l_sztSearchIndex++;
            if (l_sztSearchIndex >= strlen(p_pcSearch)) {
                return (char *)p_pcBuffer + l_sztBufferIndex;
            }
        } else {
            l_sztBufferIndex++;
            l_sztSearchIndex = 0;
        }
    } while ((l_sztBufferIndex + l_sztSearchIndex) <= p_sztBufferLength);
    
    return NULL;
}

/**
*   Parse a buffer in order to check for some characters and eventualy retreive the position and length of characters between a 
*   starting a ending position 
*
*   params:    
*       p_pBuffer:                  Char buffer to parse
*       p_pcStartString:            Characters to first search for. If NULL, start parsing from the begining of p_pBuffer
*       p_pcStopString:             Characters to next search for. position returned will be the one immediatly after p_pcStopString. If NULL, parse
*                                   from p_pcStartString to the p_iLengthToRead, assuming value is not negative
*       p_iLengthToRead:            length to read if p_pcStopString is NULL
*       p_cBufferIndexed            updated address of the immediat position after p_pcStartString. 
*       p_puiBufferLength:          update length of data available between p_pcStartString and p_pcStopString
*
*       Example:    p_pBuffer: "+IPD:2,250:HTTP/1.1....." and looking for the link id (2)
*                   p_pcStartString: "+IPD:"     
*                   p_pcStopString: ","      
*                   p_iLengthToRead: -1 (meaning not used) 
*                   => p_cBufferIndexed = 5
*                   => p_puiBufferLength = 1
*   return:
*       cf ENM_PARSE_BUFFER
*/
CESP8266::ENM_PARSE_BUFFER CESP8266::parseBuffer(char *p_pBuffer, const char *p_pcStartString, const char *p_pcStopString, int16_t p_iLengthToRead, char **p_cBufferIndexed, uint16_t *p_puiBufferLength, size_t p_sztBufferLength) {
    int l_iStartIndex = 0;
    int l_iStopIndex = 0;

    if (strlen(p_pcStartString) == 0) {
        l_iStartIndex = 0;
    } else {
        l_iStartIndex = indexOf(p_pBuffer, p_pcStartString, 0, p_sztBufferLength);
    }

    if (l_iStartIndex != -1) {
        if ((strlen(p_pcStopString) == 0) && (p_iLengthToRead != -1)) {
            l_iStopIndex = p_iLengthToRead + l_iStartIndex;
        } else {
            l_iStopIndex = indexOf(p_pBuffer, p_pcStopString, l_iStartIndex + strlen(p_pcStartString), p_sztBufferLength);
        }

        if (l_iStopIndex != -1) {
            if (p_cBufferIndexed != NULL) {
                *p_cBufferIndexed = (char *)(p_pBuffer + l_iStartIndex + strlen(p_pcStartString));
            }

            if (p_puiBufferLength != NULL) {
                *p_puiBufferLength = l_iStopIndex - (l_iStartIndex + strlen(p_pcStartString));
            }   

            return ENM_PARSE_BUFFER::PARSE_SUCCEEDED;
        } else {
            return ENM_PARSE_BUFFER::PARSE_ERROR_STOP_INDEX_NOT_FOUNDED;
        }
    } else {
        return ENM_PARSE_BUFFER::PARSE_ERROR_START_INDEX_NOT_FOUNDED;
    }
}

/**
*   Poll module in order to check if incoming data are pending into buffer. If data are pending, update 'm_strctRequest' member variable
*   with length and link ID data are waiting on
*
*   params:  
*       NONE
*   return:
*       cf ENM_AT_STATUS 
*/
CESP8266::ENM_AT_STATUS CESP8266::pollIncommingLength() {
    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand("AT+CIPRECVLEN?", &m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPRECVLEN?", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPRECVLEN?", l_iRetValue);
        //return ESP8266_AT_ERROR_CIPRCVLEN;
        m_strctRequest.iLinkID = -1;
        m_strctRequest.uiLength = 0;
        return CESP8266::ENM_AT_STATUS::AT_ERROR_CIPRCVLEN;
    }

    char *l_pcRetData;
    uint16_t l_uiLengthData;
    CESP8266::ENM_PARSE_BUFFER l_paseBufferRetValue = parseBuffer(&m_cATBufferResponse[0], "+CIPRECVLEN:", "\r", -1, &l_pcRetData, &l_uiLengthData, m_sztATBufferResponseUsedLength);
    if (l_paseBufferRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcRetData, l_uiLengthData);
        char *l_pcStroke = strtok(m_toolBufferTinyMiscellaneous.getBuffer(), ",");
        byte l_byLinkCounter = 0;
        while(l_pcStroke != NULL) {
            if (atoi(l_pcStroke) != 0) {
                m_strctRequest.uiLength = atoi(l_pcStroke);
                m_strctRequest.iLinkID = l_byLinkCounter;
                break;
            }
            l_pcStroke = strtok(NULL, ",");
            l_byLinkCounter++;
        }
    } else {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "AT+CIPRECVLEN? PARSE", l_paseBufferRetValue);
        return CESP8266::ENM_AT_STATUS::AT_ERROR_PARSE;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Retreive incoming data from the module
*
*   params:    
*       p_pcBuffer                  Pointer to a pointer of the incomming buffer. Address will be update with the location of the begining
*                                   of the reponse data. 'm_strctRequest' variable member is updated with full length of data
*   return:
*       cf ENM_AT_STATUS 
*/
CESP8266::ENM_AT_STATUS CESP8266::retreiveIncomingData(char **p_pcBuffer) {

    m_toolBufferATCommands.start("AT+CIPRECVDATA=");
    itoa(m_strctRequest.iLinkID, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferATCommands.concat(",");
    itoa(m_strctRequest.uiLength, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), "FAILED");
        return CESP8266::ENM_AT_STATUS::AT_ERROR_CIPRECVDATA;
    }

    char * l_pcRetData = NULL;
    uint16_t l_uiLengthData;
    CESP8266::ENM_PARSE_BUFFER l_paseBufferRetValue = parseBuffer(&m_cATBufferResponse[0], "+CIPRECVDATA,", "\r", -1, &l_pcRetData, &l_uiLengthData, m_sztATBufferResponseUsedLength);
    if (l_paseBufferRetValue == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcRetData, l_uiLengthData);
        m_strctRequest.uiLength = atoi(m_toolBufferTinyMiscellaneous.getBuffer());
    } else {
        return CESP8266::ENM_AT_STATUS::AT_ERROR_PARSE;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Parse the incoming buffer and extract the header and eventually the data. This function is a template in order to identify
*   if the operation regards a request or a reponse. In one case data follows +CIPRECVDATA and one the other one +IPD.
*   Then, according to the type of p_strctData (STRCT_RESPONSE or STRCT_REQUEST), the correct parseIncomingBuffer() function is called
*
*   params: 
*       p_strctData:                pointer to m_strctResponse (RESPONSE) or m_strctRequest (REQUEST). the corresponding member variable
*                                   will be udpated with header and eventually data            
*   return:
*       cf ENM_PARSE_BUFFER
*/
template<typename T>
CESP8266::ENM_PARSE_BUFFER CESP8266::parseHeaderAndBody(T *p_strctData) {

    char *l_pcBuffer;
    uint16_t l_uiLength;
    CESP8266::ENM_PARSE_BUFFER l_enmParseRetValue;

    if ((l_enmParseRetValue = parseIncomingBuffer(&l_pcBuffer, p_strctData)) == PARSE_SUCCEEDED) {

        if (parseBuffer(l_pcBuffer, ":", "", p_strctData->uiLength, &l_pcBuffer, &l_uiLength, m_sztATBufferResponseUsedLength) == PARSE_SUCCEEDED) {

            if (parseBuffer(l_pcBuffer, "", "\r\n\r\n", -1, &l_pcBuffer, &l_uiLength, p_strctData->uiLength) == PARSE_SUCCEEDED) {
                
                retreiveHeader(l_pcBuffer, &p_strctData->header, l_uiLength);

                if (p_strctData->header.uiContentLength != 0) {
                    if (parseBuffer(l_pcBuffer, "\r\n\r\n", "", p_strctData->header.uiContentLength, &l_pcBuffer, &l_uiLength, p_strctData->uiLength) == PARSE_SUCCEEDED) {
                        *(l_pcBuffer + p_strctData->header.uiContentLength) = '\0';
                        p_strctData->pData = l_pcBuffer;
                    } else {
                        return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_BODY;
                    }
                } else {
                    p_strctData->pData = NULL;
                }
            } else {
                return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_HEADER;
            }
        } else {
            return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_START_HTTP;
        }
    } else {
        return l_enmParseRetValue;
    }

    return CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED;
}

/**
*   Parse the incoming buffer (m_cBufferIncoming) in order to extract total length and data following a response
*
*   params: 
*       p_bMultiple:                'TRUE' to enable multiple connections 
*       p_pcBuffer                  pointer to a pointer to the correct position of the incoming buffer. This address
*                                   will be update and set to the position of the HHTP response.
*       p_pstrctResponse            pointer to the member variable 'm_strctResponse' to update with the total length              
*   return:
*       cf ENM_PARSE_BUFFER
*/
CESP8266::ENM_PARSE_BUFFER CESP8266::parseIncomingBuffer(char **p_pcBuffer, CESP8266::STRCT_RESPONSE *p_pstrctResponse) {
    char *l_pcBuffer;
    uint16_t l_uiLength;

    itoa(OUTGOING_LINK_ID, m_toolBufferTinyMiscellaneous.getBuffer(), 10);

    if (parseBuffer(&m_cATBufferResponse[0], "+IPD,", ":", -1, &l_pcBuffer, &l_uiLength, m_sztATBufferResponseUsedLength) == PARSE_SUCCEEDED) {
        if (parseBuffer(l_pcBuffer, m_toolBufferTinyMiscellaneous.getBuffer(), ",", -1, &l_pcBuffer, &l_uiLength, m_sztATBufferResponseUsedLength) == PARSE_SUCCEEDED) {
            if (parseBuffer(l_pcBuffer, ",", ":", -1, &l_pcBuffer, &l_uiLength, m_sztATBufferResponseUsedLength) == PARSE_SUCCEEDED) {

                m_toolBufferTinyMiscellaneous.start(l_pcBuffer, l_uiLength);
                p_pstrctResponse->uiLength = atoi(m_toolBufferTinyMiscellaneous.getBuffer());
                *p_pcBuffer = l_pcBuffer;

                return CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED;
            } else {
                return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_TOTAL_LENGTH;
            }
        } else {
            return CESP8266::ENM_PARSE_BUFFER::PARSE_EEROR_LINKID_AND_TOTAL_LENGTH;
        }
    } else {
        return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_IPD;
    }
}

/**
*   Parse the incoming buffer (m_cBufferIncoming) in order to extract total length and data following a request
*
*   params: 
*       p_bMultiple:                'TRUE' to enable multiple connections 
*       p_pcBuffer                  pointer to a pointer to the correct position of the incoming buffer. This address
*                                   will be update and set to the position of the HHTP response.
*       p_pstrctResponse            pointer to the member variable 'm_strctRequest' to update with the total length              
*   return:
*       cf ENM_PARSE_BUFFER 
*/
CESP8266::ENM_PARSE_BUFFER CESP8266::parseIncomingBuffer(char **p_pcBuffer, CESP8266::STRCT_REQUEST *p_pstrctRequest) {
    char *l_pcBuffer;
    uint16_t l_uiLength;

    if (parseBuffer(&m_cATBufferResponse[0], "+CIPRECVDATA,", ":", -1, &l_pcBuffer, &l_uiLength, m_sztATBufferResponseUsedLength) == PARSE_SUCCEEDED) {
        
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer, l_uiLength);
        p_pstrctRequest->uiLength = atoi(m_toolBufferTinyMiscellaneous.getBuffer());
        
        *p_pcBuffer = l_pcBuffer;

        return CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED;
    } else {
        return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_TOTAL_LENGTH;
    }
}

/**
*   Extract header of incoming response
*
*   params:    
*       p_pcBuffer:                 char buffer containingthe response
*       p_pstrctResponseHeader:     pointer to the STRCT_RESPONSE_HEADER to fill-out    
*   return:
*       NONE
*/
void CESP8266::retreiveHeader(char *p_pcBuffer, CESP8266::STRCT_RESPONSE_HEADER *p_pstrctResponseHeader, size_t p_sztBufferLength) {
    char *l_pcBuffer;;
    uint16_t l_uiLength;

    if (parseBuffer(p_pcBuffer, "HTTP/1.1 ", " ", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer - 1, l_uiLength + 1);
        p_pstrctResponseHeader->enmStatusCode = (ENM_REST_STATUS_CODE)(atoi(m_toolBufferTinyMiscellaneous.getBuffer()));
    } else {
        p_pstrctResponseHeader->enmStatusCode = (ENM_REST_STATUS_CODE)(-1);
    }

    if (parseBuffer(p_pcBuffer, "Content-Type: ", "\r", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer, l_uiLength);
        m_toolBufferTinyMiscellaneous.copy(&p_pstrctResponseHeader->pcContentType[0], MAX_CONTENT_TYPE_LENGTH);
    } else {
        memset(p_pstrctResponseHeader->pcContentType, '\0', MAX_CONTENT_TYPE_LENGTH);
    }

    if (parseBuffer(p_pcBuffer, "Content-Length: ", "\r", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer, l_uiLength);
        p_pstrctResponseHeader->uiContentLength = atoi(m_toolBufferTinyMiscellaneous.getBuffer());
    } else {
        p_pstrctResponseHeader->uiContentLength = -1;
    }
}

/**
*   Extract header of incoming request
*
*   params:    
*       p_pcBuffer:                 char buffer containingthe response
*       p_pstrctResponseHeader:     pointer to the STRCT_REQUEST_HEADER to fill-out    
*   return:
*       NONE
*/
void CESP8266::retreiveHeader(char *p_pcBuffer, CESP8266::STRCT_REQUEST_HEADER *p_pstrctRequestHeader, size_t p_sztBufferLength) {
    char *l_pcBuffer;;
    uint16_t l_uiLength;

    if (parseBuffer(p_pcBuffer, "", " /", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer, l_uiLength);

        if (strncmp(m_toolBufferTinyMiscellaneous.getBuffer(), "POST", strlen("POST")) == 0) {
            p_pstrctRequestHeader->enmMethod = CESP8266::ENM_REST_METHOD::POST;
        } else {
            if (strncmp(m_toolBufferTinyMiscellaneous.getBuffer(), "GET", strlen("GET")) == 0) {
                p_pstrctRequestHeader->enmMethod = CESP8266::ENM_REST_METHOD::GET;
            } else {
                if (strncmp(m_toolBufferTinyMiscellaneous.getBuffer(), "OUT", strlen("OUT")) == 0) {
                    p_pstrctRequestHeader->enmMethod = CESP8266::ENM_REST_METHOD::PUT;
                } else {
                    if (strncmp(m_toolBufferTinyMiscellaneous.getBuffer(), "DELETE", strlen("DELETE")) == 0) {
                        p_pstrctRequestHeader->enmMethod = CESP8266::ENM_REST_METHOD::DELETE;
                    }
                }
            }
        }
    } else {
         p_pstrctRequestHeader->enmMethod = CESP8266::ENM_REST_METHOD::UNKNOWN;
    }

    if (parseBuffer(p_pcBuffer, " /", " HTTP/", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer - 1, l_uiLength + 1);
        m_toolBufferTinyMiscellaneous.copy(&p_pstrctRequestHeader->pcUri[0], MAX_URI_LENGTH);
    } else {
        memset(&p_pstrctRequestHeader->pcUri[0], '\0', MAX_URI_LENGTH);
    }

    if (parseBuffer(p_pcBuffer, "Host: ", "\r", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer , l_uiLength);
        m_toolBufferTinyMiscellaneous.copy(&p_pstrctRequestHeader->pcHost[0], MAX_HOSTNAME_LENGTH);
    } else {
        memset(p_pstrctRequestHeader->pcHost, '\0', MAX_HOSTNAME_LENGTH);
    }

    if (parseBuffer(p_pcBuffer, "Authorization: ", "\r", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer , l_uiLength);
        m_toolBufferTinyMiscellaneous.copy(&p_pstrctRequestHeader->pcAuthorization[0], MAX_AUTHORIZATION_LENGTH);
    } else {
       memset(p_pstrctRequestHeader->pcAuthorization, '\0', MAX_AUTHORIZATION_LENGTH);
    }

    if (parseBuffer(p_pcBuffer, "Content-Type: ", "\r", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer , l_uiLength);
        m_toolBufferTinyMiscellaneous.copy(&p_pstrctRequestHeader->pcContentType[0], MAX_CONTENT_TYPE_LENGTH);
    } else {
        memset(p_pstrctRequestHeader->pcContentType, '\0', MAX_CONTENT_TYPE_LENGTH);
    }

    if (parseBuffer(p_pcBuffer, "Content-Length: ", "\r", -1, &l_pcBuffer, &l_uiLength, p_sztBufferLength) == PARSE_SUCCEEDED) {
        m_toolBufferTinyMiscellaneous.start(l_pcBuffer , l_uiLength);
        p_pstrctRequestHeader->uiContentLength = atoi(m_toolBufferTinyMiscellaneous.getBuffer());
    } else {
        p_pstrctRequestHeader->uiContentLength = 0;
    }
}

/**
*   Send response to host. Host has not to be provided because response is automatically sent following a request (cf available() function).
*   'm_strctRequest' member variable has been updated with link ID (betwwen 0 to 5) which will be used for the reply
*
*   params: 
*       p_strctHeader:              header to send
*       p_strResponse:              response to send
*   return:
*       cf CESP8266::ENM_STATUS   
*/
CESP8266::ENM_STATUS CESP8266::sendResponse(CESP8266::STRCT_RESPONSE_HEADER p_strctHeader, const char *p_pccResponse) {
    //build full reponse including header
    itoa(p_strctHeader.enmStatusCode, m_toolBufferTinyMiscellaneous.getBuffer(), 10);

    m_toolBufferLargeMiscellaneous.start("HTTP/1.1 ");
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat("\r\n");
    
    if (strlen(p_strctHeader.pcContentType) != 0) {
        m_toolBufferLargeMiscellaneous.concat("Content-Type: " );
        m_toolBufferLargeMiscellaneous.concat(p_strctHeader.pcContentType);
        m_toolBufferLargeMiscellaneous.concat("\r\n");
    }

    if (strlen(p_pccResponse) != 0) {
        m_toolBufferLargeMiscellaneous.concat("Content-Length: ");
        itoa(strlen(p_pccResponse), m_toolBufferTinyMiscellaneous.getBuffer(), 10);
        m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
        m_toolBufferLargeMiscellaneous.concat("\r\n");
    }

    m_toolBufferLargeMiscellaneous.concat("Connection: close\r\n");

    if (strlen(p_pccResponse) != 0) {
        m_toolBufferLargeMiscellaneous.concat("\r\n");
        m_toolBufferLargeMiscellaneous.concat(p_pccResponse);
        m_toolBufferLargeMiscellaneous.concat("\r\n");
    }

    //send data (cf AT+CIPSEND=): transmit length to send, wait for prompt '>',  send data, doesn't wait for 'CLOSED and for for reply   
    CESP8266::ENM_AT_STATUS l_enmATStatusRetValue = sendData(m_strctRequest.iLinkID, m_toolBufferLargeMiscellaneous.getBuffer());
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "sendResponse/sendData", (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) ? "OK" : "NOK");
    if (l_enmATStatusRetValue != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        return CESP8266::ENM_STATUS::ERROR_AT_SEND_RESPONSE;
    }


    //close the connection
    l_enmATStatusRetValue = closeConnection(m_strctRequest.iLinkID);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "sendResponse/closeConnection", (l_enmATStatusRetValue == CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) ? "OK" : "NOK");
    if (l_enmATStatusRetValue != CESP8266::ENM_AT_STATUS::AT_SUCCEEDED) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "sendResponse/closeConnection", l_enmATStatusRetValue);
        return CESP8266::ENM_STATUS::ERROR_AT_SEND_RESPONSE;
    }

    return CESP8266::ENM_STATUS::SUCEEDED;
}

/**
*   Send data to the module and wait for reply
*
*   params:  
*       p_byLink:                   Link IDS to use            
*       p_pcData:                   Data to send       
*   return:
*       cf CESP8266::ENM_AT_STATUS
*/
CESP8266::ENM_AT_STATUS CESP8266::sendData(byte p_byLink, char *p_pcData) {

    m_toolBufferATCommands.start("AT+CIPSEND=");
    if (m_enmServerMode != CESP8266::ENM_SERVER_MODE::SERVER_MODE_DISABLED) {
        itoa(p_byLink, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
        m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());
        m_toolBufferATCommands.concat(",");
    }
    itoa(strlen(p_pcData) + 2, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), l_iRetValue);
        return CESP8266::ENM_AT_STATUS::AT_ERROR_CIPSEND;
    }

    m_toolBufferATExpectedResponses.startList("> ");
    l_iRetValue = readNextRemaingATResponse(&m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "WAIT >", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "WAIT >", l_iRetValue);
        return CESP8266::ENM_AT_STATUS::AT_ERROR_WAIT_PROMPT;
    }


    m_toolBufferATExpectedResponses.startList("SEND OK\r\n", CCharBufferTool::ENM_LIST_TYPE::OR);
    m_toolBufferATExpectedResponses.insertToList("ERROR");
    l_iRetValue = sendATCommand(p_pcData, &m_toolBufferATExpectedResponses);
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "NOT ERROR: WAIt SEND OK", l_iRetValue);
    }

    m_toolBufferATExpectedResponses.startList("CLOSED\r\n", CCharBufferTool::ENM_LIST_TYPE::OR);
    m_toolBufferATExpectedResponses.insertToList("ERROR");
    l_iRetValue = readNextRemaingATResponse(&m_toolBufferATExpectedResponses);
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "NOT ERROR: WAIT CLOSED", l_iRetValue);
    }
    
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, "WAIT END OF TRANSMISSION", (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, "WAIT END OF TRANSMISSION", l_iRetValue);
        return CESP8266::ENM_AT_STATUS::AT_ERROR_WAIT_PROMPT;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

/**
*   Close a connection, typically a request connection according to the link ID
*
*   params:  
*       p_byLink:                   Link ID to close        
*   return:
*       cf CESP8266::ENM_AT_STATUS
*/
CESP8266::ENM_AT_STATUS CESP8266::closeConnection(byte p_byLink) {

    itoa(p_byLink, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferATCommands.start("AT+CIPCLOSE=");
    m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferATExpectedResponses.startList("OK\r\n");
    int8_t l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue <= 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), l_iRetValue);
        return CESP8266::ENM_AT_STATUS::AT_ERROR_CIPCLOSE;
    }

    m_strctRequest.iLinkID = -1;
    m_strctRequest.uiLength = 0;

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}


/**
*   Parse an Uri in order to extract a value corresponding to a key passed as, e.g. /my_Uri?key1=value1&key2=value2...
*   Calling function added character '&' to the end of p_pcUri in order to simplify the process
*
*   params:    
*       p_pcUri:                    char buffer containing the Uri to parse
*       p_pcKey:                    key to search for  
*       p_pcRetString               pointer of the char array to populate with the value  
*   return:
*       cf CESP8266::ENM_PARSE_BUFFER
*/
CESP8266::ENM_PARSE_BUFFER CESP8266::parseUri(char *p_pcUri, const char *p_pcKey, char *p_pcRetString) {

    char *l_pcBuffer = p_pcUri;
    uint16_t l_uiLength;
    if (parseBuffer(l_pcBuffer, p_pcKey, "=", -1, &l_pcBuffer, &l_uiLength, strlen(l_pcBuffer)) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        if (parseBuffer(l_pcBuffer, "", "&", -1, &l_pcBuffer, &l_uiLength, strlen(l_pcBuffer)) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
            m_toolBufferTinyMiscellaneous.start(l_pcBuffer + 1, l_uiLength - 1);
            strcpy(p_pcRetString, m_toolBufferTinyMiscellaneous.getBuffer());
            
            return CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED;
        } else {
            return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_NO_VALUE;
        }
    } else {
        return CESP8266::ENM_PARSE_BUFFER::PARSE_EEROR_NO_KEY;
    }
}

/**
*   Parse an JSON object in order to extract a value corresponding to a key. Object can be a STRING, a JSON {} or an array []
*
*   params:    
*       p_pcBuffer:                 char buffer containingthe the json object to parse
*       p_pcKey:                    key to search for  
*       p_pcStartString:            starting character to search for in order to isolate the value, e.g. "\"", "{" or "["
*       p_pcStopString:             ending character to search for in order to isolate the value, e.g. "\"", "}" or "]"
*       p_pcRetString               pointer of the char array to populate with the value  
*   return:
*       cf CESP8266::ENM_PARSE_BUFFER
*/
CESP8266::ENM_PARSE_BUFFER CESP8266::parseJSON(char *p_pcBuffer, const char *p_pcKey, const char *p_pcStartString, const char *p_pcStopString, char *p_pcRetString) {

    m_toolBufferTinyMiscellaneous.start("\"");
    m_toolBufferTinyMiscellaneous.concat(p_pcKey);
    m_toolBufferTinyMiscellaneous.concat("\"");

    char *l_pcBuffer = p_pcBuffer;
    uint16_t l_uiLength;
    if (parseBuffer(l_pcBuffer, m_toolBufferTinyMiscellaneous.getBuffer(), ":", -1, &l_pcBuffer, &l_uiLength, strlen(l_pcBuffer)) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
        if (parseBuffer(l_pcBuffer, (char *)p_pcStartString, (char *)p_pcStopString, -1, &l_pcBuffer, &l_uiLength, strlen(l_pcBuffer)) == CESP8266::ENM_PARSE_BUFFER::PARSE_SUCCEEDED) {
            m_toolBufferTinyMiscellaneous.start(l_pcBuffer, l_uiLength);
            strcpy(p_pcRetString, m_toolBufferTinyMiscellaneous.getBuffer());
            return PARSE_SUCCEEDED;
        } else {
            return CESP8266::ENM_PARSE_BUFFER::PARSE_ERROR_NO_VALUE;
        }
    } else {
        return CESP8266::ENM_PARSE_BUFFER::PARSE_EEROR_NO_KEY;
    }
}

/**
*   Initiate a data sending to the module by starting to establishing a TCP connection
*
*   params:  
*       p_pcHostName:               Hostname to connect to, IP address or domain name            
*       p_uiPort:                   Port number    
*       p_pcUri                     Uri    
*       p_bSSL                      'TRUE' to enable SSL
*   return:
*       cf CESP8266::ENM_AT_STATUS
*/
CESP8266::ENM_AT_STATUS CESP8266::sendStart(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL) {

    m_toolBufferATCommands.start("AT+CIPSTART=");

    if (m_enmServerMode != CESP8266::ENM_SERVER_MODE::SERVER_MODE_DISABLED) {
        itoa(OUTGOING_LINK_ID, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
        m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());
        m_toolBufferATCommands.concat(",");
    }

    m_toolBufferATCommands.concat("\"");
    m_toolBufferATCommands.concat((p_bSSL ? "SSL" : "TCP"));
    m_toolBufferATCommands.concat("\",\"");
    m_toolBufferATCommands.concat(p_pcHostName);
    m_toolBufferATCommands.concat("\",");

    itoa(p_uiPort, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferATCommands.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferATExpectedResponses.startList("OK\r\n", CCharBufferTool::ENM_LIST_TYPE::OR);
    m_toolBufferATExpectedResponses.insertToList("ALREADY CONNECTED\r\n");
    m_toolBufferATExpectedResponses.insertToList("busy p...");

    int8_t l_iRetValue = sendATCommand(m_toolBufferATCommands.getBuffer(), &m_toolBufferATExpectedResponses);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), (l_iRetValue > 0) ? "OK" : "NOK");
    if (l_iRetValue == 3) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), l_iRetValue);
        return CESP8266::ENM_AT_STATUS::AT_ERROR_BUSY;
    }

    if (l_iRetValue < 0) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_ESP8266, m_toolBufferATCommands.getBuffer(), l_iRetValue);
        return CESP8266::ENM_AT_STATUS::AT_ERROR_CIPSTART;
    }

    return CESP8266::ENM_AT_STATUS::AT_SUCCEEDED;
}

#endif