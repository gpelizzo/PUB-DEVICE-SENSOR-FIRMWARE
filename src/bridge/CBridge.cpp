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
#include "CBridge.h"

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
*   Init the bridge interface in charge of communicate devices measures to the API server  
*   params: 
*       p_pSerialPort:                    pointer to the UART on which ESP module is connected
*       p_ulBaudeRate:                    baude rate of the communication
*       p_pstrctGlobalSettingsAndStatus:  pointer to the global settings
*   return:
*       CESP8266::ENM_STATUS status returns by ESP8266 driver  
*/
CESP8266::ENM_STATUS CBridge::init(Uart *p_pSerialPort, unsigned long p_ulBaudeRate, STRUCT_BRIDGE_SETTINGS *p_pstrctBridgeSettings, STRUCT_WIFI_STATUS *p_pstrWiFiStatus) {
    CESP8266::ENM_STATUS l_iRestValue;

    m_pstrctBridgeSettings = p_pstrctBridgeSettings;

    memset(&m_cBufferTinyMiscellaneous[0], 0, MAX_SERVER_TINY_MISCELLANEOUS_LENGTH);
    memset(&m_cBufferLargeMiscellaneous[0], 0, MAX_SERVER_LARGE_MISCELLANEOUS_LENGTH);
    
    m_toolBufferTinyMiscellaneous.init(&m_cBufferTinyMiscellaneous[0], MAX_SERVER_TINY_MISCELLANEOUS_LENGTH);
    m_toolBufferLargeMiscellaneous.init(&m_cBufferLargeMiscellaneous[0], MAX_SERVER_LARGE_MISCELLANEOUS_LENGTH);

    l_iRestValue = m_C8266Drv.init(p_pSerialPort, p_ulBaudeRate, &m_pstrctBridgeSettings->strctWifiSettings, p_pstrWiFiStatus);
    LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "INIT", (l_iRestValue == 0) ? "OK" : "NOK");

    LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "IP", p_pstrWiFiStatus->cIP);
    LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Gateway", p_pstrWiFiStatus->cGateway);
    LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Mask", p_pstrWiFiStatus->cMask);
    LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "MAC", p_pstrWiFiStatus->cMAC);

//#ifdef BRIDGE_SERVER
    l_iRestValue = m_C8266Drv.enableServerMode(80, true);
    LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Enable server", (l_iRestValue == 0) ? "OK" : "NOK");
//#endif

    m_bServerInit = true;
    return l_iRestValue;
}

uint8_t CBridge::factoryReset() {
  return m_C8266Drv.restoreFactoryDefaultSettings();
}

/**
*   pool ESP8266 driver in order to retreive incoming payloads when SERVER mode is enabled 
*   params: 
*       NONE
*   return:
*       TRUE if an incoming payload has been received. 
*/
boolean CBridge::poll() {
    if (!m_bServerInit) {
        return false;
    }

#ifdef BRIGE_SERVER
    //example only
    if (m_C8266Drv.getServerMode() == CESP8266::ENM_SERVER_MODE::SERVER_MODE_ENABLED_PASSIVE) {

        if ((m_pstrctRequest = m_C8266Drv.available()) != NULL) {

          CESP8266::STRCT_RESPONSE_HEADER l_strctHeader;
          l_strctHeader.enmStatusCode = CESP8266::ENM_REST_STATUS_CODE::NOT_FOUND;
          strcpy(l_strctHeader.pcContentType, "");

          LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Verb", m_pstrctRequest->header.enmMethod);
          LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "URI", m_pstrctRequest->header.pcUri);
          LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Authorization", m_pstrctRequest->header.pcAuthorization);
          LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Content-Type", m_pstrctRequest->header.pcContentType);
          LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Host", m_pstrctRequest->header.pcHost);

          LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Json ", m_pstrctRequest->pData);
          
          /////to be removed
          char l_value[128];

          switch (m_pstrctRequest->header.enmMethod) {
            case CESP8266::ENM_REST_METHOD::POST:
              if (strncmp(m_pstrctRequest->header.pcUri, "/test", MAX_URI_LENGTH) == 0) {
                l_strctHeader.enmStatusCode = CESP8266::ENM_REST_STATUS_CODE::OK;
                strcpy(l_strctHeader.pcContentType , "application/json; charset=utf-8");

                m_C8266Drv.getJSONValue(m_pstrctRequest->pData, "name", &l_value[0]);
                LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "name", l_value);

                m_C8266Drv.sendResponse(l_strctHeader, "{\"value\":\"response2\"}");
                break;
              }
            break;

            case CESP8266::ENM_REST_METHOD::GET:
              if (m_C8266Drv.indexOf(m_pstrctRequest->header.pcUri, "/test?", 0, MAX_URI_LENGTH) != -1) {
              //if (strncmp(m_pstrctRequest->header.pcUri, "/test?", MAX_URI_LENGTH) == 0) {
                l_strctHeader.enmStatusCode = CESP8266::ENM_REST_STATUS_CODE::OK;
                strcpy(l_strctHeader.pcContentType, "application/json; charset=utf-8");
               
                m_C8266Drv.getUriParam(m_pstrctRequest->header.pcUri, "value", &l_value[0]);
                LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "value", l_value);

                m_C8266Drv.getUriParam(m_pstrctRequest->header.pcUri, "name", &l_value[0]);
                LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "name", l_value);

                m_C8266Drv.sendResponse(l_strctHeader, "{\"value\":\"response2\"}");
              }
            break;

            default:
              l_strctHeader.enmStatusCode = CESP8266::ENM_REST_STATUS_CODE::NOT_FOUND;
            break;
          }

          

          return true;
        }
    }
#endif

    return false;
}

/**
*   post a keep-alive message to the API server 
*   params: 
*       NONE
*   return:
*       TRUE if succeeded to post  
*/
boolean CBridge::postKeepaliveServer() {
  CESP8266::STRCT_RESPONSE *l_pstrctResponse;
  uint8_t l_uiRetriesCounter = 0;

  m_toolBufferLargeMiscellaneous.start("{\"api_client_id\":\"");
  m_toolBufferLargeMiscellaneous.concat(m_pstrctBridgeSettings->strctAPIServerSettings.cClientID);
  m_toolBufferLargeMiscellaneous.concat("\"}");

  m_toolBufferTinyMiscellaneous.start(m_pstrctBridgeSettings->strctAPIServerSettings.cUri);
  m_toolBufferTinyMiscellaneous.concat(API_URI_POST_KEEP_ALIVE_SERVER);

  do {
      if ((l_pstrctResponse = m_C8266Drv.postJsonToHost(m_pstrctBridgeSettings->strctAPIServerSettings.cHostname, 
                                                      m_pstrctBridgeSettings->strctAPIServerSettings.uiPort, 
                                                      m_toolBufferTinyMiscellaneous.getBuffer(),
                                                      true, m_toolBufferLargeMiscellaneous.getBuffer(),
                                                      m_pstrctBridgeSettings->strctAPIServerSettings.cAuthorizationToken)) != NULL) {
        return true;
      }
  } while (l_uiRetriesCounter++ < MAX_BRIDGE_SEND_RETRIES);

  return false;
}

/**
*   post a keep-alive message to the API server concerning a device
*   params: 
*       p_uiDeviceAddr:         device address
*   return:
*       TRUE if succeeded to post  
*/
boolean CBridge::postKeepaliveDevice(uint8_t p_uiDeviceAddr) {
  CESP8266::STRCT_RESPONSE *l_pstrctResponse;
  uint8_t l_uiRetriesCounter = 0;

  m_toolBufferLargeMiscellaneous.start("{\"api_client_id\":\"");
  m_toolBufferLargeMiscellaneous.concat(m_pstrctBridgeSettings->strctAPIServerSettings.cClientID);
  m_toolBufferLargeMiscellaneous.concat("\",\"device_addr\":\"");

  itoa(p_uiDeviceAddr, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
  m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
  m_toolBufferLargeMiscellaneous.concat("\"}");

  m_toolBufferTinyMiscellaneous.start(m_pstrctBridgeSettings->strctAPIServerSettings.cUri);
  m_toolBufferTinyMiscellaneous.concat(API_URI_POST_KEEP_ALIVE_DEVICE);

  do {
      if ((l_pstrctResponse = m_C8266Drv.postJsonToHost(m_pstrctBridgeSettings->strctAPIServerSettings.cHostname, 
                                                      m_pstrctBridgeSettings->strctAPIServerSettings.uiPort, 
                                                      m_toolBufferTinyMiscellaneous.getBuffer(),
                                                      true, m_toolBufferLargeMiscellaneous.getBuffer(),
                                                      m_pstrctBridgeSettings->strctAPIServerSettings.cAuthorizationToken)) != NULL) {
        return true;
      }
  } while (l_uiRetriesCounter++ < MAX_BRIDGE_SEND_RETRIES);

  return false;
}


/**
*   post sensor values to the API server
*   params: 
*       cf STRUCT_X_QUEUE_SENSOR_VALUES
*   return:
*       TRUE if succeeded to post  
*/
boolean CBridge::postSensorValues(STRUCT_X_QUEUE_SENSOR_VALUES pstrctQueuePostMessageSensorValues) {

    CESP8266::STRCT_RESPONSE *l_pstrctResponse;
    uint8_t l_uiRetriesCounter = 0;

    m_toolBufferLargeMiscellaneous.start("{\"temperature_value\":\"");
    itoa(pstrctQueuePostMessageSensorValues.byTemperatureQuotientValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat(".");
    itoa(pstrctQueuePostMessageSensorValues.byTemperatureRemaindertValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat("\",\"humidity_value\":\"");
    itoa(pstrctQueuePostMessageSensorValues.byHumidityQuotientValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat(".");
    itoa(pstrctQueuePostMessageSensorValues.byHumidityRemaindertValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferLargeMiscellaneous.concat("\",\"partial_pressure_value\":\"");
    itoa(pstrctQueuePostMessageSensorValues.byPartialPressureQuotientValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat(".");
    itoa(pstrctQueuePostMessageSensorValues.byPartialPressureRemaindertValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferLargeMiscellaneous.concat("\",\"dew_point_value\":\"");
    itoa(pstrctQueuePostMessageSensorValues.byDewPointeQuotientValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat(".");
    itoa(pstrctQueuePostMessageSensorValues.byDewPointRemaindertValue, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());

    m_toolBufferLargeMiscellaneous.concat("\",\"api_client_id\":\"");
    m_toolBufferLargeMiscellaneous.concat(m_pstrctBridgeSettings->strctAPIServerSettings.cClientID);
    m_toolBufferLargeMiscellaneous.concat("\",\"device_addr\":\"");
    itoa(pstrctQueuePostMessageSensorValues.uiDeviceId, m_toolBufferTinyMiscellaneous.getBuffer(), 10);
    m_toolBufferLargeMiscellaneous.concat(m_toolBufferTinyMiscellaneous.getBuffer());
    m_toolBufferLargeMiscellaneous.concat("\"}");

    m_toolBufferTinyMiscellaneous.start(m_pstrctBridgeSettings->strctAPIServerSettings.cUri);
    m_toolBufferTinyMiscellaneous.concat(API_URI_POST_SENSOR_VALUES);

    //ugly workaround for the common ESP8266 'busy p...' issue !
    do {
      if ((l_pstrctResponse = m_C8266Drv.postJsonToHost(m_pstrctBridgeSettings->strctAPIServerSettings.cHostname, 
                                                      m_pstrctBridgeSettings->strctAPIServerSettings.uiPort, 
                                                      m_toolBufferTinyMiscellaneous.getBuffer(),
                                                      true, m_toolBufferLargeMiscellaneous.getBuffer(), 
                                                      m_pstrctBridgeSettings->strctAPIServerSettings.cAuthorizationToken)) != NULL) {
                            
        //received response
        /*
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Status code", l_pstrctResponse->header.enmStatusCode);
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Content Type", l_pstrctResponse->header.pcContentType);
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Content Length", l_pstrctResponse->header.uiContentLength);

        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "Json", l_pstrctResponse->pData);

        m_C8266Drv.getJSONValue(l_pstrctResponse->pData, "status", m_toolBufferTinyMiscellaneous.getBuffer());
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "status", m_toolBufferTinyMiscellaneous.getBuffer());

        m_C8266Drv.getJSONValue(l_pstrctResponse->pData, "value2", m_toolBufferTinyMiscellaneous.getBuffer());
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "value2", m_toolBufferTinyMiscellaneous.getBuffer());

        m_C8266Drv.getJSONObject(l_pstrctResponse->pData, "value", m_toolBufferTinyMiscellaneous.getBuffer());
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "value", m_toolBufferTinyMiscellaneous.getBuffer());

        m_C8266Drv.getJSONValue(m_toolBufferTinyMiscellaneous.getBuffer(), "myvalue", m_toolBufferTinyMiscellaneous.getBuffer());
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "myvalue", m_toolBufferTinyMiscellaneous.getBuffer());

        m_C8266Drv.getJSONArrayObject(l_pstrctResponse->pData, "array", m_toolBufferTinyMiscellaneous.getBuffer());
        LOG_INFO_PRINTLN(LOG_PREFIX_BRIDGE, "array", m_toolBufferTinyMiscellaneous.getBuffer());
        */
        return true;
      } 
  } while (l_uiRetriesCounter++ < MAX_BRIDGE_SEND_RETRIES);

  return false;
}

#endif
