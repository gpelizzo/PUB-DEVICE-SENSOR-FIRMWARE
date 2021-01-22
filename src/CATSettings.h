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
#ifndef __CSETDEVICE_H__
#define __CSETDEVICE_H__

#include "global.h"
#include "logging.h"
#include "CHTU21.h"

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

#define MAX_SETTINGSDEVICE_INCOMING_BUFFER_LENGTH       512
#define MAX_SETTINGSDEVICE_OUTGOING_BUFFER_LENGTH       64

#define MAX_AT_BUFFER_MISCELLANEOUS                     32

class CATSettings {
public:
    void init(Serial_ *p_pSerialPort, STRUCT_GLOBAL_SETTINGS_AND_STATUS *p_pGlobalSettingsAndStatus, 
            EventGroupHandle_t *p_pxEventGroupMiscellaneousHandle, QueueHandle_t *p_pxQueueATSettingsHandle);
    void pool() ;

private:
    enum ENM_METHOD {GET, SET, DO, UNKNOWN};

    struct STRCT_AT_COMMAND {
        ENM_METHOD      enmMethod;
        char            *pcCommand;
        char            *pcParam;
    };

    STRUCT_GLOBAL_SETTINGS_AND_STATUS     *m_pGlobalSettingsAndStatus;

    EventGroupHandle_t  *m_pxEventGroupMiscellaneousHandle;
    QueueHandle_t       *m_pxQueueATSettingsHandle;

    Serial_             *m_pSerialPort;
    char                m_bufferIncoming[MAX_SETTINGSDEVICE_INCOMING_BUFFER_LENGTH];
    uint16_t            m_cBufferIncomingIndex;
    char                m_cBufferMiscallaneous[MAX_AT_BUFFER_MISCELLANEOUS];

    STRCT_AT_COMMAND    m_strctATCommand;
    boolean             m_bEchoEnabled = false;

    void                checkATCommand();
    int                 indexOf(char *p_pcBuffer, const char *p_pcSearch, size_t p_sztFromIndex);
    void                operateATCommand();
    boolean             isGetCommand(const char *p_pcCommand);
    boolean             isSetCommand(const char *p_pcCommand);
    boolean             isDoCommand(const char *p_pcCommand);
    long                getParamNumericValue(char *p_pcValue);
    boolean             isParamNumericValue(char *p_pcValue);
    boolean             isParamLengthConsistent(size_t p_sztMaxLength);
    boolean             isParamEqualTo(const char *p_pcParam);
    char *              getJsonValueFromKey(const char *p_pcKey);
};

#endif