#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include <FreeRTOS_SAMD21.h>

#include "CFlashLed.h"

#define VERSION_MAJOR               1
#define VERSION_MINOR               0  

//-------------------------------------------------------------------------------------------------------
//toogle directive below to enable or disable development mode avoiding to proceed initial settings
//#define _DEVELOP_
#undef _DEVELOP_

//toogle directive below to enable or disable bridge mode=server mode
//#define BRIDGE_MODE
#undef BRIDGE_MODE
//-------------------------------------------------------------------------------------------------------

#ifdef BRIDGE_MODE
    //uncomment directive below in order to enable BRIDGE SERVER utilityn meaning port listening on WiFi module => some testing are required
    //#define BRIDGE_SERVER  

    #undef BRIDGE_SERVER 
    #define DEVICE_TYPE                                 "BRIDGE-SERVER-SENSOR"

    #ifdef _DEVELOP_
        //default settings to use during development 
        #define _RADIO_DEVELOP_DEVICE_ADDR_                 1
        #define _RADIO_DEVELOP_KEEPALIVE_TIMEOUT            1      //minutes
        #define _BRIDGE_DEVELOP_HOSTNAME_                   "<API_URI_WITHOUT_HTTPS>"   
        #define _BRIDGE_DEVELOP_PORT_                       443    
        #define _BRIDGE_DEVELOP_URI_                        "/device"  
        #define _BRIDGE_DEVELOP_AUTHORIZATION_              ""  
        #define _BRIDGE_DEVELOP_CLIENTID_                   "<CLIENT_ID>" 
        #define _BRIDGE_DEVELOP_AP_SSID_                    "ACCESS_POINT_SSID"        
        #define _BRIDGE_DEVELOP_AP_KEY_                     "ACCESS_POINT_KEY"     
        #define _BRIDGE_DEVELOP_KEEPALIVE_TIMEOUT_          10       //minutes    
        #define _MISCELLANEOUS_READ_SENSOR_VALUES_TIMEOUT_  10      //minutes
    #else
        #define RADIO_SERVER_ID                         1
    #endif

    #define MAX_HOSTNAME_LENGTH                         64
    #define MAX_URI_LENGTH                              64
    #define MAX_CLIENT_ID_LENGTH                        16
    #define MAX_AUTHORIZATION_LENGTH                    64
    #define MAX_WIFI_SSID_LENGTH                        16
    #define MAX_WIFI_KEY_LENGTH                         32
    #define MAX_IP_ADDR_LENGTH                          20
    #define MAX_MAC_ADDR_LENGTH                         20    

    #define API_URI_POST_SENSOR_VALUES                  "/sensor-values" 
    #define API_URI_POST_KEEP_ALIVE_DEVICE              "/keep-alive-device" 
    #define API_URI_POST_KEEP_ALIVE_SERVER              "/keep-alive-server" 
#else
    #undef BRIDGE_SERVER

    #define DEVICE_TYPE                                 "SIMPLE-SENSOR"

    #ifdef _DEVELOP_
        //default settings to use during development 
        #define _RADIO_DEVELOP_DEVICE_ADDR_                 3
        #define _RADIO_DEVELOP_SERVER_ADDR_                 1
        #define _RADIO_DEVELOP_KEEPALIVE_TIMEOUT            1       //minutes
        #define _MISCELLANEOUS_READ_SENSOR_VALUES_TIMEOUT_  1       //minutes
    #endif
#endif

#ifdef _DEVELOP_
    #define _RADIO_MSG_SIGNATURE                            0x52E3  
    #define _MAX_RADIO_RETRIES_                             3
#endif

#define STATUS_LED_PIN                                  13
#define ESP8266_RESET_PIN                               0
#define JUMPER_PIN                                      1           //must be an pin-interrupt

#define BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_FACTORY_RESET                    (1 << 0)
#define BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SAVE_SETTINGS                    (1 << 1)
#define BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_BRIDGE_ERASE_WIFI_PARAMS         (1 << 2)
#define BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SENSOR_VALUES_MEASUREMENT        (1 << 3)            //measure but does not send to API server

//FLASH settings saving signature
#define FLASH_STORAGE_SIGNATURE_ID                      0xB5E3
#define MAX_RADIO_DEVICES                               127

enum ENM_AT_CALLBACK {
    SAVE_SETTINGS,
    BRIDGE_FACTORY_RESET
};

enum ENM_RADIO_MSG_TYPE {
    POST_SENSOR_VALUES = 0,
    KEEP_ALIVE
};

#ifdef BRIDGE_MODE
    struct STRUCT_WIFI_STATUS {
        char        cIP[MAX_IP_ADDR_LENGTH];
        char        cGateway[MAX_IP_ADDR_LENGTH];
        char        cMask[MAX_IP_ADDR_LENGTH];
        char        cMAC[MAX_MAC_ADDR_LENGTH];
    } __attribute__ ((packed));     //non aligment pragma

    struct STRUCT_WIFI_SETTINGS {
        char        cSSID[MAX_WIFI_SSID_LENGTH];
        char        cKey[MAX_WIFI_KEY_LENGTH];
    } __attribute__ ((packed));     //non aligment pragma

    struct STRUCT_API_SERVER_SETTINGS {
        char        cHostname[MAX_HOSTNAME_LENGTH];
        char        cUri[MAX_URI_LENGTH];
        uint16_t    uiPort;
        char        cClientID[MAX_CLIENT_ID_LENGTH];
        char        cAuthorizationToken[MAX_AUTHORIZATION_LENGTH];
        uint8_t     uiKeepAliveTimeout;     //minutes
    } __attribute__ ((packed));     //non aligment pragma

    struct STRUCT_BRIDGE_SETTINGS {
        STRUCT_WIFI_SETTINGS        strctWifiSettings;
        STRUCT_API_SERVER_SETTINGS  strctAPIServerSettings;
    } __attribute__ ((packed));     //non aligment pragma
#endif  

struct STRUCT_MISCELLANEOUS_SETTINGS {
    uint16_t    uiReadSensorValuesMeasurementTimeout;       //min
} __attribute__ ((packed));     //non aligment pragma
    
struct STRUCT_RADIO_SETTINGS {
    uint8_t     uiDeviceID;
#ifndef BRIDGE_MODE
    uint8_t     uiServerID;
#endif
    uint8_t     uiMaxRetries;
    uint8_t     uiOutputPower;              //cf CCC1100::ENM_OUTPUT_POWER_DBM
    uint8_t     uiKeepAliveTimeout;         //minutes
    uint16_t    uiMessageSignature;
} __attribute__ ((packed));     //non aligment pragma

struct STRUCT_GLOBAL_SETTINGS_AND_STATUS {
    char                            cUID[32 + 3 + 1]; //4xuint32 => 32 quartets = 32 hex chars + 3 separators chars + 1termination char
#ifdef BRIDGE_MODE
    STRUCT_WIFI_STATUS              strctWifiStatus;
    STRUCT_BRIDGE_SETTINGS          strctBridgeSettings;
#endif
    STRUCT_RADIO_SETTINGS           strctRadioSettings;
    STRUCT_MISCELLANEOUS_SETTINGS   strctMiscellaneousSettings;
} __attribute__ ((packed));     //non aligment pragma

struct STRUCT_FLASH_SETTINGS {
#ifdef BRIDGE_MODE
    STRUCT_BRIDGE_SETTINGS          strctBridgeSettings;
#endif
    STRUCT_RADIO_SETTINGS           strctRadioSettings;
    STRUCT_MISCELLANEOUS_SETTINGS   strctMiscellaneousSettings;
} __attribute__ ((packed));     //non aligment pragma

enum ENM_X_QUEUE_POST_MSG_TYPE {
    POST_DEVICE_SENSOR_VALUES = 0,
    POST_BRIDGE_KEEP_ALIVE,
    POST_DEVICE_KEEP_ALIVE
};

struct STRUCT_X_QUEUE_SENSOR_VALUES {
    uint8_t     uiDeviceId;
    byte        byTemperatureQuotientValue;
    byte        byTemperatureRemaindertValue;
    byte        byHumidityQuotientValue;
    byte        byHumidityRemaindertValue;
    byte        byPartialPressureQuotientValue;
    byte        byPartialPressureRemaindertValue;   
    byte        byDewPointeQuotientValue;
    byte        byDewPointRemaindertValue;  
       
} __attribute__ ((packed));     //non aligment pragma

struct STRUCT_X_QUEUE_DEVICE_KEEP_ALIVE {
    uint8_t     uiDeviceId;
    //aligment with STRUCT_X_QUEUE_SENSOR_VALUES Size
    uint8_t     uiDummy[sizeof(STRUCT_X_QUEUE_SENSOR_VALUES) - sizeof(uint8_t)];       
};

struct STRUCT_X_QUEUE_DUMMY {
    //aligment with STRUCT_X_QUEUE_SENSOR_VALUES Size
    uint8_t     uiDummy[sizeof(STRUCT_X_QUEUE_SENSOR_VALUES)];       
} __attribute__ ((packed));     //non aligment pragma

struct STRUCT_X_QUEUE_POST_MSG {
    ENM_X_QUEUE_POST_MSG_TYPE                   enmMsgType;
    union {
        STRUCT_X_QUEUE_SENSOR_VALUES            strctSensorValues;
        STRUCT_X_QUEUE_DEVICE_KEEP_ALIVE        strctDeviceKeepAlive;
        STRUCT_X_QUEUE_DUMMY                    strctDummy;
        uint8_t                                 uiData[sizeof(STRUCT_X_QUEUE_SENSOR_VALUES)];
    };          
} __attribute__ ((packed));     //non aligment pragma

enum ENM_X_QUEUE_MISCELLANEOUS_ACTION_TYPE {
    UPDATE_STATUS_FLASH_LED_SCHEMA,
    SENSOR_VALUES_MEASUREMENT,
#ifdef BRIDGE_MODE
    UPDATE_WIFI_STATUS
#endif
};

struct STRUCT_FLASH_LED_SCHEMA_UPDATE {
    CFlashLed::FLASH_BUILTIN   flashLedBuiltinSchema;
#ifdef BRIDGE_MODE
    //aligment to STRUCT_WIFI_STATUS size assuming size of STRUCT_WIFI_STATUS is bigger than STRUCT_SENSOR_VALUES
    u_int8_t                    uiDummy[sizeof(STRUCT_WIFI_STATUS) - sizeof(CFlashLed::FLASH_BUILTIN)];  
#endif 
};                                      

struct STRUCT_X_QUEUE_MISCELLANEOUS {
    ENM_X_QUEUE_MISCELLANEOUS_ACTION_TYPE       enmActionType;
    union {
#ifdef BRIDGE_MODE
        STRUCT_WIFI_STATUS                      strctWwifiStatus;
#endif
        STRUCT_FLASH_LED_SCHEMA_UPDATE          strctFlashLedSchemaUpdate;  
    };                                 
} __attribute__ ((packed));     //non aligment pragma

#endif