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

#ifndef __CESP8266_H__
#define __CESP8266_H__

#include "global.h"

#ifdef BRIDGE_MODE

    #include "logging.h"
    #include "CCharBufferTool.h"

    //buffer dedicated to AT communication with ESP8266 module
    #define MAX_AT_BUFFER_RESPONSE_LENGTH           2048 
    #define MAX_BUFFER_AT_COMMANDS_LENGTH           1024 
    #define MAX_BUFFER_AT_EXPETED_RESPONSES_LENGTH  128
    #define MAX_ESP8266_TINY_MISCELLANEOUS_LENGTH    64
    #define MAX_ESP8266_LARGE_MISCELLANEOUS_LENGTH   1024

    #define MAX_CONTENT_TYPE_LENGTH                  40

    #define TIMEOUT_READ_AT_RESPONSE                30000

    #define OUTGOING_LINK_ID                         4

    //toggle this directice to trace AT reponse reponse from module
    //#define TRACE_AT_RESPONSE_FORM_MODULE
    #undef TRACE_AT_RESPONSE_FORM_MODULE

    class CESP8266 {
    public:
        enum ENM_STATUS {SUCEEDED = 0, 
                        ERROR_BUFFER_AT_COMMANDS_TOO_LARGE = -1, 
                        ERROR_BUFFER_AT_EXPECTED_RESPONSES_TOO_LARGE = -2, 
                        ERROR_AT_EXPECTED_RESPONSES = -3,
                        ERROR_AT_SEND_RESPONSE = -4,
                        ERROR_AT_WAIT_CLOSE = -10};

        enum ENM_SERVER_MODE {SERVER_MODE_ENABLED_ACTIVE = 0, SERVER_MODE_ENABLED_PASSIVE = 1, SERVER_MODE_DISABLED = -1};

        enum ENM_REST_METHOD {POST = 0, GET, PUT, DELETE, UNKNOWN};
        enum ENM_REST_STATUS_CODE {OK=200, NO_CONTENT=204, BAD_REQUEST=400, UNAUTHORIZED=401, FORBIDDEN=403, NOT_FOUND=404, METHOD_NOT_ALLOWED=405};

        struct STRCT_REQUEST_HEADER {
            ENM_REST_METHOD         enmMethod;
            char                    pcUri[MAX_URI_LENGTH];
            char                    pcAuthorization[MAX_AUTHORIZATION_LENGTH];
            char                    pcContentType[MAX_CONTENT_TYPE_LENGTH];
            char                    pcHost[MAX_HOSTNAME_LENGTH];
            int16_t                 uiContentLength;
        };

        struct STRCT_RESPONSE_HEADER {
            ENM_REST_STATUS_CODE    enmStatusCode;
            char                    pcContentType[MAX_CONTENT_TYPE_LENGTH];
            uint16_t                uiContentLength;
        };

        struct STRCT_RESPONSE {
            char                    *pData;
            uint16_t                uiLength;
            STRCT_RESPONSE_HEADER   header;
        };

        struct STRCT_REQUEST {
            int8_t                  iLinkID;
            uint16_t                uiLength;
            char                    *pData;
            STRCT_REQUEST_HEADER    header;
        };

        ENM_STATUS                  init(Uart *p_pSerialPort, unsigned long p_ulBaudeRate, STRUCT_WIFI_SETTINGS *p_pstrctWifiSettings, STRUCT_WIFI_STATUS *p_pstrctWifiStatus);
        ENM_STATUS                  enableServerMode(uint16_t p_uiListeningPort,  boolean p_bPassiveMode);
        ENM_STATUS                  disableServerMode();
        ENM_SERVER_MODE             getServerMode();
        STRCT_REQUEST               *available();
        ENM_STATUS                   sendResponse(CESP8266::STRCT_RESPONSE_HEADER p_strctHeader, const char *p_pccResponse);

        int                         indexOf(char *p_pcBuffer, const char *p_pcSearch, size_t p_sztFromIndex, size_t p_sztBufferLength);
        boolean                     getUriParam(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString);
        boolean                     getJSONArrayObject(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString);
        boolean                     getJSONObject(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString);
        boolean                     getJSONValue(char *p_pcEntryBuffer, const char *p_pcKey, char *p_pcRetString);

        STRCT_RESPONSE              *postJsonToHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL, char *p_pcData, char *p_pcAuthorization);
        STRCT_RESPONSE              *postJsonToHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL, char *p_pcData);
        STRCT_RESPONSE              *getFromHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL, char *p_pcAuthorization);
        STRCT_RESPONSE              *getFromHost(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL);
        ENM_STATUS                   restoreFactoryDefaultSettings();

    private:
        enum ENM_AT_STATUS {AT_SUCCEEDED = 0, 
                            AT_RESPONSE_REACH_TIMEOUT = 1,              //in that case, reaching timout has been requested in order to retreive all characters from the module
                            AT_NOT_AUTOCONNECTED_TO_AP = -1, 
                            AT_ERROR_RESPONSE = -2, 
                            AT_ERROR_PARSE = -3,
                            AT_ERROR_CIPRCVLEN = -4,
                            AT_ERROR_CIPRECVDATA = -5,
                            AT_ERROR_CIPSEND = -6,
                            AT_ERROR_WAIT_PROMPT = -7,
                            AT_ERROR_CIPCLOSE = -8,
                            AT_ERROR_CIPSTART = -9,
                            AT_ERROR_BUSY = -10,
                            AT_ERROR_RESPONSE_TIMEOUT = -126,
                            AT_ERROR_BUFFER_RESPONSE_FULL = -127};

        enum ENM_PARSE_BUFFER {PARSE_SUCCEEDED = 0,
                                PARSE_ERROR_START_INDEX_NOT_FOUNDED = -1,
                                PARSE_ERROR_STOP_INDEX_NOT_FOUNDED = -2,
                                PARSE_ERROR_BODY = -3,
                                PARSE_ERROR_HEADER = -4,
                                PARSE_ERROR_START_HTTP = -5,
                                PARSE_ERROR_TOTAL_LENGTH = -6,
                                PARSE_EEROR_LINKID_AND_TOTAL_LENGTH = -7,
                                PARSE_ERROR_IPD = -8,
                                PARSE_EEROR_NO_KEY = -9,
                                PARSE_ERROR_NO_VALUE = -10};

        Uart                        *m_pSerialPort;
        char                        m_cATBufferResponse[MAX_AT_BUFFER_RESPONSE_LENGTH];
        size_t                      m_sztATBufferResponseUsedLength;
        char                        m_cBufferATCommands[MAX_BUFFER_AT_COMMANDS_LENGTH];
        CCharBufferTool             m_toolBufferATCommands;
        char                        m_cBufferATExpectedResponses[MAX_BUFFER_AT_EXPETED_RESPONSES_LENGTH];
        CCharBufferTool             m_toolBufferATExpectedResponses;
        char                        m_cBufferTinyMiscellaneous[MAX_ESP8266_TINY_MISCELLANEOUS_LENGTH];
        CCharBufferTool             m_toolBufferTinyMiscellaneous;
        char                        m_cBufferLargeMiscellaneous[MAX_ESP8266_LARGE_MISCELLANEOUS_LENGTH];
        CCharBufferTool             m_toolBufferLargeMiscellaneous;

        STRUCT_WIFI_STATUS          *m_pstrctWifiStatus;
        STRUCT_WIFI_SETTINGS        *m_pstrctWifiSettings;

        ENM_SERVER_MODE             m_enmServerMode = ENM_SERVER_MODE::SERVER_MODE_DISABLED;

        STRCT_RESPONSE              m_strctResponse;
        STRCT_REQUEST               m_strctRequest;

        ENM_AT_STATUS               resetModule();
        ENM_AT_STATUS               setEchoMode(boolean p_bEnable);
        ENM_AT_STATUS               connectToAP(const char *p_pcSSID, const char *p_pcPassword);
        ENM_AT_STATUS               retreiveCurrentIP();
        ENM_AT_STATUS               retreiveMACAddr();
        ENM_AT_STATUS               setSSLBufferSize(uint16_t p_uiSSLBufferSize);
        ENM_AT_STATUS               setConnectionsMode(boolean p_bMultiple);
        ENM_AT_STATUS               pollIncommingLength();
        ENM_AT_STATUS               retreiveIncomingData(char **p_pcBuffer);
        ENM_AT_STATUS               sendData(byte p_byLink, char *p_pcData);
        ENM_AT_STATUS               closeConnection(byte p_byLink);
        ENM_AT_STATUS               sendStart(const char *p_pcHostName, uint16_t p_uiPort, const char *p_pcUri, boolean p_bSSL);

        int8_t                      sendATCommand(const char *p_pccATCommand, CCharBufferTool *p_pBufferResponseList, boolean p_bWaitLastIncoming = false);
        int8_t                      readNextRemaingATResponse(CCharBufferTool *p_pBufferResponseList);
        int8_t                      readAllRemaingATResponse();
        int8_t                      readATResponse(CCharBufferTool *p_pBufferList, boolean p_bWaitLastIncoming, boolean p_bResetIncomingBuffer);
        int8_t                      parseATResponseBufferResponse(CCharBufferTool *p_pBufferList);
        
        ENM_PARSE_BUFFER            parseBuffer(char *p_pBuffer, const char *p_pcStartString, const char *p_pcStopString, int16_t p_iLengthToRead, char **p_cBufferIndexed, uint16_t *p_puiBufferLength, size_t p_sztBufferLength);
        ENM_PARSE_BUFFER            parseUri(char *p_pcUri, const char *p_pcKey, char *p_pcRetString);
        ENM_PARSE_BUFFER            parseJSON(char *p_pcBuffer, const char *p_pcKey, const char *p_pcStartString, const char *p_pcStopString, char *p_pcRetString);
        template<typename T>
        ENM_PARSE_BUFFER            parseHeaderAndBody(T *p_strctData);
        ENM_PARSE_BUFFER            parseIncomingBuffer(char **p_pcBuffer, STRCT_REQUEST *p_pstrctRequest);
        ENM_PARSE_BUFFER            parseIncomingBuffer(char **p_pcBuffer, STRCT_RESPONSE *p_pstrctResponse);
        void                        retreiveHeader(char *p_pcBuffer, STRCT_REQUEST_HEADER *p_pstrctRequestHeader, size_t p_sztBufferLength);
        void                        retreiveHeader(char *p_pcBuffer, STRCT_RESPONSE_HEADER *p_pstrctResponseHeader, size_t p_sztBufferLength);

        char *                      searchString(const char *p_pcBuffer, const char *p_pcSearch, size_t p_sztBufferLength); 

        ENM_STATUS                  restartModule();
    };

    #endif

#endif