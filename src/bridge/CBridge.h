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

#ifndef __CSERVER_H__
#define __CSERVER_H__

#include "global.h"

#ifdef BRIDGE_MODE

    #include "CESP8266.h"
    #include "logging.h"
    #include "CCharBufferTool.h"

    #define MAX_SERVER_TINY_MISCELLANEOUS_LENGTH    64
    #define MAX_SERVER_LARGE_MISCELLANEOUS_LENGTH   1024

    #define MAX_BRIDGE_SEND_RETRIES                 2

    class CBridge {
    public:
        CESP8266::ENM_STATUS                init(Uart *p_pSerialPort, unsigned long p_ulBaudeRate, STRUCT_BRIDGE_SETTINGS *p_pstrctBridgeSettings, STRUCT_WIFI_STATUS *p_pstrWiFiStatus);
        boolean                             poll();
        boolean                             postSensorValues(STRUCT_X_QUEUE_SENSOR_VALUES pstrctQueuePostMessageSensorValues);
        boolean                             postKeepaliveServer();
        boolean                             postKeepaliveDevice(uint8_t p_uiDeviceAddr);
        uint8_t                             factoryReset();
    private:
        char                                m_cBufferTinyMiscellaneous[MAX_SERVER_TINY_MISCELLANEOUS_LENGTH];
        CCharBufferTool                     m_toolBufferTinyMiscellaneous;
        char                                m_cBufferLargeMiscellaneous[MAX_SERVER_LARGE_MISCELLANEOUS_LENGTH];
        CCharBufferTool                     m_toolBufferLargeMiscellaneous;
        CESP8266::STRCT_REQUEST             *m_pstrctRequest;
        STRUCT_BRIDGE_SETTINGS              *m_pstrctBridgeSettings;
        CESP8266                            m_C8266Drv;
        boolean                             m_bServerInit = false;
    };

    #endif

#endif
