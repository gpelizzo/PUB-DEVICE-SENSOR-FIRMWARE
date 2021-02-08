#include <FlashStorage.h>
#include "Global.h"
#include "Logging.h"
#include "CHTU21.h"
#include "radio\CCC1100.h"
#include "CATSettings.h"

#ifdef BRIDGE_MODE
  #include "bridge\CBridge.h"
#endif

#define TIMER_PERIOD                                     10       //TIMER5 PERIO => 10ms

//FLASH settings saving
FlashStorage(_g_flashSettings, STRUCT_FLASH_SETTINGS);           //store global settings
FlashStorage(_g_flashStorageSignatureID, uint16_t);              //store a 2 bytes signature (FLASH_SIGNATURE_ID) in order to ensure that settings above are relevant

//objects instantiation
static CHTU21 g_htu21Device;
static CCC1100 g_cc1101Device;
static CFlashLed g_statusFlashLed;
#ifdef BRIDGE_MODE
  static CBridge g_bridgeDrv;
#endif
static CATSettings g_ATSettings;

//FreeRTOS TASK - SENSOR VALUES MEASURES
#define X_BUFFER_TASK_SENSOR_VALUES_SIZE                  256
TaskHandle_t g_xHandleTaskSensorValues;
StaticTask_t g_xTCBTaskSensorValues;
StackType_t g_xBufferTaskSensorValues[X_BUFFER_TASK_SENSOR_VALUES_SIZE];

//FreeRTOS TASK - RADIO MANAGEMENT
#define X_BUFFER_TASK_RADIO                               256
TaskHandle_t g_xHandleTaskRadio;
StaticTask_t g_xTCBTaskRadio;
StackType_t g_xBufferTaskRadio[X_BUFFER_TASK_RADIO];

//FreeRTOS TASK - WIFI INTERFACE MANAGEMENT
#ifdef BRIDGE_MODE
  #define X_BUFFER_TASK_BRIDGE                            256
  TaskHandle_t g_xHandleTaskBridge;
  StaticTask_t g_xTCBTaskBridge;
  StackType_t g_xBufferTaskBridge[X_BUFFER_TASK_BRIDGE];
#endif

//FreeRTOS TASK - AT SETTINGS MANAGEMENT
#define X_BUFFER_TASK_AT_SETTINGS                         256
TaskHandle_t g_xHandleTaskATSettings;
StaticTask_t g_xTCBTaskATSettings;
StackType_t g_xBufferTaskATSettings[X_BUFFER_TASK_AT_SETTINGS];

//FreeRTOS TASK - MISCELLANEOUS MANAGEMENT
#define X_BUFFER_TASK_MISCELLANEOUS                       256
TaskHandle_t g_xHandleTaskMiscellaneous;
StaticTask_t g_xTCBTaskTasksMiscellaneous;
StackType_t g_xBufferTaskMiscellaneous[X_BUFFER_TASK_MISCELLANEOUS];

//FreeRTOS SEMAPHORE - SEMAPHORE USBSerial Access
SemaphoreHandle_t g_xSemaphoreHandleSerial;
StaticSemaphore_t g_xSemaphoreBufferSerial;

//FreeRTOS QUEUE - SENSOR VALUES DISPATCHING
#define X_QUEUE_SENSOR_VALUES_LENGTH                      1
#define X_QUEUE_SENSOR_VALUES_SIZE                        sizeof(STRUCT_X_QUEUE_POST_MSG)
uint8_t g_xQueueSensorValuesHandleBuffer[X_QUEUE_SENSOR_VALUES_LENGTH * X_QUEUE_SENSOR_VALUES_SIZE];
static StaticQueue_t g_xQueueSensorValuesHandleStatic;
QueueHandle_t g_xQueueSensorValuesHandle;

//FreeRTOS QUEUE - devices messages dispatching
#ifdef BRIDGE_MODE
  #define X_QUEUE_BRIDGE_LENGTH                     10
  #define X_QUEUE_BRIDGE_SIZE                       sizeof(STRUCT_X_QUEUE_POST_MSG)
  uint8_t g_xQueueBridgeHandleBuffer[X_QUEUE_BRIDGE_LENGTH * X_QUEUE_BRIDGE_SIZE];
  static StaticQueue_t g_xQueueBridgeHandleStatic;
  QueueHandle_t g_xQueueBridgeHandle;
#endif

//FreeRTOS QUEUE - MISCELLANEOUS MESSAGES DISPATCHING
#define X_QUEUE_MISCELLANEOUS_LENGTH                      5
#define X_QUEUE_MISCELLANEOUS_SIZE                        sizeof(STRUCT_X_QUEUE_MISCELLANEOUS)
uint8_t g_xQueueMiscellaneousHandleBuffer[X_QUEUE_MISCELLANEOUS_LENGTH * X_QUEUE_MISCELLANEOUS_SIZE];
static StaticQueue_t g_xQueueMiscellaneousHandleStatic;
QueueHandle_t g_xQueueMiscellaneousHandle;

//FreeRTOS QUEUE - AT-SETTINGS MESSAGES DISPATCHING
#define X_QUEUE_AT_SETTINGS_LENGTH                      1
#define X_QUEUE_AT_SETTINGS_SIZE                        sizeof(CHTU21::STRUCT_SENSOR_VALUES)
uint8_t g_xQueueATSettingsHandleBuffer[X_QUEUE_AT_SETTINGS_LENGTH * X_QUEUE_AT_SETTINGS_SIZE];
static StaticQueue_t g_xQueueATSettingsHandleStatic;
QueueHandle_t g_xQueueATSettingsHandle;

//FreeRTOS GROUP EVENT - TIMERS FLAGS
EventGroupHandle_t  g_xEventGroupTimersHandle;
StaticEventGroup_t  g_xEventGroupTimersStatic;

#define BIT_EVENT_GROUP_TIMERS__READ_SENSOR_VALUES_TIMER_EXPIRES      (1 << 0)
#define BIT_EVENT_GROUP_TIMERS__SEND_DEVICE_KEEP_ALIVE_TIMER_EXPIRES  (1 << 1)
#define BIT_EVENT_GROUP_TIMERS__SEND_API_KEEP_ALIVE_TIMER_EXPIRES     (1 << 2)

//FreeRTOS GROUP EVENT - MISCELLANEOUS FLAGS
EventGroupHandle_t  g_xEventGroupMiscellaneousHandle;
StaticEventGroup_t  g_xEventGroupMiscellaneousStatic;

#ifdef BRIDGE_MODE
  TimerHandle_t g_xTimerAPIKeepAliveHandle;
  StaticTimer_t g_xTimerAPIKeepAliveStatic;
#endif
TimerHandle_t g_xTimerDeviceKeepAliveHandle;
StaticTimer_t g_xTimerDeviceKeepAliveStatic;

TimerHandle_t g_xTimerStatusFlashLedHandle;
StaticTimer_t g_xTimerStatusFlashLedStatic;
TimerHandle_t g_xTimerReadSensorValuesHandle;
StaticTimer_t g_xTimerReadSensrValuesStatic;

STRUCT_GLOBAL_SETTINGS_AND_STATUS g_globalSettingsAndStatus;

int g_iJumberRebounceLastISRTimeMillis = 0;

Adafruit_USBD_WebUSB g_USBWeb;
Adafruit_USBD_CDC g_USBSerial;

/**
*   RADIO THREAD: manage RADIO incoming and outgoing message 
*
*   params: 
*     pvParameters:     FREERTOS Thread parameters
*   return:
*       NONE       
*/
static void thread_Radio( void *pvParameters ) {
  STRUCT_X_QUEUE_POST_MSG l_strctPostMessage;
  CCC1100::STRUCT_RADIO_PAYLOAD_MESSAGE l_strctRadioBuffer;
#ifdef BRIDGE_MODE
  int16_t l_iSenderAddr;
#endif
  STRUCT_RADIO_SETTINGS l_readioSettings;

#ifndef BRIDGE_MODE
    boolean l_bStatus;
#endif

  memcpy(&l_readioSettings, pvParameters, sizeof(STRUCT_RADIO_SETTINGS));

  //delete current thread if initalization failed
  if (!g_cc1101Device.init(l_readioSettings.uiDeviceID, 
                          (CCC1100::ENM_OUTPUT_POWER_DBM)l_readioSettings.uiOutputPower, 
                          l_readioSettings.uiMessageSignature)) {
    vTaskDelete( NULL );
  }

  xTimerStart(g_xTimerDeviceKeepAliveHandle, 0);

  while (1) {
#ifdef BRIDGE_MODE
  //pool RADIO in order to check if an incoming payload is pending
  if (g_cc1101Device.poll()) {
    l_iSenderAddr = g_cc1101Device.getMessage(&l_strctRadioBuffer);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_MAIN, "receive radio", l_iSenderAddr);

    if (l_iSenderAddr != -1) {
      switch(l_strctRadioBuffer.byMessageType) {
        case ENM_RADIO_MSG_TYPE::POST_SENSOR_VALUES:
          l_strctPostMessage.enmMsgType = ENM_X_QUEUE_POST_MSG_TYPE::POST_DEVICE_SENSOR_VALUES;
          l_strctPostMessage.strctSensorValues.uiDeviceId = l_iSenderAddr;
          l_strctPostMessage.strctSensorValues.byTemperatureQuotientValue = l_strctRadioBuffer.abyData[0];
          l_strctPostMessage.strctSensorValues.byTemperatureRemaindertValue = l_strctRadioBuffer.abyData[1];
          l_strctPostMessage.strctSensorValues.byHumidityQuotientValue = l_strctRadioBuffer.abyData[2];
          l_strctPostMessage.strctSensorValues.byHumidityRemaindertValue = l_strctRadioBuffer.abyData[3];
          l_strctPostMessage.strctSensorValues.byPartialPressureQuotientValue = l_strctRadioBuffer.abyData[4];
          l_strctPostMessage.strctSensorValues.byPartialPressureRemaindertValue = l_strctRadioBuffer.abyData[5];
          l_strctPostMessage.strctSensorValues.byDewPointeQuotientValue = l_strctRadioBuffer.abyData[6];
          l_strctPostMessage.strctSensorValues.byDewPointRemaindertValue = l_strctRadioBuffer.abyData[7];


          xQueueSendToBack(g_xQueueBridgeHandle, ( void * )&l_strctPostMessage, 0/*portMAX_DELAY*/);
        break;

        case ENM_RADIO_MSG_TYPE::KEEP_ALIVE:
          l_strctPostMessage.enmMsgType = ENM_X_QUEUE_POST_MSG_TYPE::POST_DEVICE_KEEP_ALIVE;
          l_strctPostMessage.strctDeviceKeepAlive.uiDeviceId = l_iSenderAddr;

          xQueueSendToBack(g_xQueueBridgeHandle, ( void * )&l_strctPostMessage, 0/*portMAX_DELAY*/);
      
        default:
        break;
      }
    }
  }
#else
  //send sensor values to device bridge-server
  if (xQueueReceive(g_xQueueSensorValuesHandle, &l_strctPostMessage, 0)) {
    l_strctRadioBuffer.byMessageType = ENM_RADIO_MSG_TYPE::POST_SENSOR_VALUES;
    l_strctRadioBuffer.byDataLength = 8;
    l_strctRadioBuffer.abyData[0] = l_strctPostMessage.strctSensorValues.byTemperatureQuotientValue;
    l_strctRadioBuffer.abyData[1] = l_strctPostMessage.strctSensorValues.byTemperatureRemaindertValue;
    l_strctRadioBuffer.abyData[2] = l_strctPostMessage.strctSensorValues.byHumidityQuotientValue;
    l_strctRadioBuffer.abyData[3] = l_strctPostMessage.strctSensorValues.byTemperatureQuotientValue;

    l_strctRadioBuffer.abyData[4] = l_strctPostMessage.strctSensorValues.byPartialPressureQuotientValue;
    l_strctRadioBuffer.abyData[5] = l_strctPostMessage.strctSensorValues.byPartialPressureRemaindertValue;
    l_strctRadioBuffer.abyData[6] = l_strctPostMessage.strctSensorValues.byDewPointeQuotientValue;
    l_strctRadioBuffer.abyData[7] = l_strctPostMessage.strctSensorValues.byDewPointRemaindertValue;

    l_bStatus = g_cc1101Device.postMessage(l_readioSettings.uiServerID, &l_strctRadioBuffer, l_readioSettings.uiMaxRetries);
    //if sending failed, restart immediatly by simulating a event timer expiration
    if (!l_bStatus) {
      xEventGroupSetBits(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__READ_SENSOR_VALUES_TIMER_EXPIRES);
    }

    LOG_DEBUG_PRINTLN(LOG_PREFIX_MAIN, "POST SENSOR VALUES to SERVER", l_bStatus ? "OK" : "NOK");
  }
#endif

 //wait for BIT_EVENT_GROUP_TIMERS__SEND_API_KEEP_ALIVE_TIMER_EXPIRES to bet set, meaning timer g_timerMinDeviceKeepAlive has expired
  if (xEventGroupWaitBits(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__SEND_DEVICE_KEEP_ALIVE_TIMER_EXPIRES, pdTRUE, pdFALSE, 0) & BIT_EVENT_GROUP_TIMERS__SEND_DEVICE_KEEP_ALIVE_TIMER_EXPIRES) {
#ifdef BRIDGE_MODE
    l_strctPostMessage.enmMsgType = ENM_X_QUEUE_POST_MSG_TYPE::POST_DEVICE_KEEP_ALIVE;
    l_strctPostMessage.strctSensorValues.uiDeviceId = l_readioSettings.uiDeviceID;
    
    xQueueSendToBack(g_xQueueBridgeHandle, ( void * )&l_strctPostMessage, 0/*portMAX_DELAY*/);
#else
    l_strctRadioBuffer.byMessageType = ENM_RADIO_MSG_TYPE::KEEP_ALIVE;
    l_strctRadioBuffer.byDataLength = 0;

    l_bStatus = g_cc1101Device.postMessage(l_readioSettings.uiServerID, &l_strctRadioBuffer, l_readioSettings.uiMaxRetries);
    LOG_DEBUG_PRINTLN(LOG_PREFIX_MAIN, "POST DEVICE KEEP ALIVE", l_bStatus ? "OK" : "NOK");
#endif
  }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


/**
*   SENSOR VALUES MEASUREMENT THREAD: Read Temperature, Humididy, partial pressure and dew point values from devices and send either to
*                                            device bridge-server or API server
*
*   params: 
*     pvParameters:     FREERTOS Thread parameters
*   return:
*       NONE       
*/
static void thread_SensorValues( void *pvParameters ) {

  CHTU21::STRUCT_SENSOR_VALUES l_strctSensorValues;

  STRUCT_RADIO_SETTINGS l_readioSettings;

  memcpy(&l_readioSettings, pvParameters, sizeof(STRUCT_RADIO_SETTINGS));

  STRUCT_X_QUEUE_POST_MSG l_strctPostMessage = {ENM_X_QUEUE_POST_MSG_TYPE::POST_DEVICE_SENSOR_VALUES, 0, 0};

  //init sensor chip
  if (!g_htu21Device.init()) {
    vTaskDelete( NULL );
  }

  xTimerStart(g_xTimerReadSensorValuesHandle, 0);

  while (1)
  {
    //wait for BIT_EVENT_GROUP_TIMERS__READ_SENSOR_VALUES_TIMER_EXPIRES to bet set, meaning timer has expired
    if (xEventGroupWaitBits(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__READ_SENSOR_VALUES_TIMER_EXPIRES, pdTRUE, pdFALSE, 0) & BIT_EVENT_GROUP_TIMERS__READ_SENSOR_VALUES_TIMER_EXPIRES) {

      //retreive sensor values
      l_strctSensorValues = g_htu21Device.getSensorValues();

      l_strctPostMessage.enmMsgType = ENM_X_QUEUE_POST_MSG_TYPE::POST_DEVICE_SENSOR_VALUES;
      l_strctPostMessage.strctSensorValues.uiDeviceId = l_readioSettings.uiDeviceID;
      l_strctPostMessage.strctSensorValues.byTemperatureQuotientValue = (byte)l_strctSensorValues.fTemperatureValue;
      l_strctPostMessage.strctSensorValues.byTemperatureRemaindertValue = (byte)((l_strctSensorValues.fTemperatureValue - (byte)l_strctSensorValues.fTemperatureValue) * 100);
      l_strctPostMessage.strctSensorValues.byHumidityQuotientValue = (byte)l_strctSensorValues.fHumidityValue;
      l_strctPostMessage.strctSensorValues.byTemperatureRemaindertValue = (byte)((l_strctSensorValues.fHumidityValue - (byte)l_strctSensorValues.fHumidityValue) * 100);
      l_strctPostMessage.strctSensorValues.byPartialPressureQuotientValue = (byte)l_strctSensorValues.fPartialPressureValue;
      l_strctPostMessage.strctSensorValues.byPartialPressureRemaindertValue = (byte)((l_strctSensorValues.fPartialPressureValue - (byte)l_strctSensorValues.fPartialPressureValue) * 100);
      l_strctPostMessage.strctSensorValues.byDewPointeQuotientValue = (byte)l_strctSensorValues.fDewPointTemperatureValue;
      l_strctPostMessage.strctSensorValues.byDewPointRemaindertValue = (byte)((l_strctSensorValues.fDewPointTemperatureValue - (byte)l_strctSensorValues.fDewPointTemperatureValue) * 100);

#ifdef BRIDGE_MODE
      //send values to API Server via Bridge Thread
      xQueueSendToBack(g_xQueueBridgeHandle, ( void * )&l_strctPostMessage, 0/*portMAX_DELAY*/);
#else
      //send values to device bridge-server via Radio Thread 
      xQueueOverwrite(g_xQueueSensorValuesHandle, ( void * )&l_strctPostMessage);
#endif
      }

    //perform a simple sensor values measurement without sending to server-bridge and API server; Internal usage only, e.g. AT command requestiong info
    if (xEventGroupWaitBits(g_xEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SENSOR_VALUES_MEASUREMENT, pdTRUE, pdFALSE, 0) & BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SENSOR_VALUES_MEASUREMENT) {
        l_strctSensorValues = g_htu21Device.getSensorValues();
        xQueueOverwrite(g_xQueueATSettingsHandle, ( void * )&l_strctSensorValues);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}


#ifdef BRIDGE_MODE
/**
*   BRIDGE THREAD: Manage API Server outgoing (and eventualy incoming) messages
*
*   params: 
*     pvParameters:     FREERTOS Thread parameters
*   return:
*       NONE       
*/
static void thread_Bridge( void *pvParameters ) {
  STRUCT_X_QUEUE_POST_MSG l_strctQueuePostMsg;

  STRUCT_X_QUEUE_MISCELLANEOUS l_strcMiscellaneous;
  STRUCT_BRIDGE_SETTINGS l_strctBridgeSettings;

  memcpy(&l_strctBridgeSettings, pvParameters, sizeof(STRUCT_BRIDGE_SETTINGS));

  if (g_bridgeDrv.init(&Serial1, 115200, &l_strctBridgeSettings, &l_strcMiscellaneous.strctWwifiStatus) != CESP8266::ENM_STATUS::SUCEEDED) {
    vTaskDelete( NULL );
  }

  l_strcMiscellaneous.enmActionType = ENM_X_QUEUE_MISCELLANEOUS_ACTION_TYPE::UPDATE_WIFI_STATUS;
  xQueueSendToBack(g_xQueueMiscellaneousHandle, ( void * )&l_strcMiscellaneous, 0);

  xTimerStart(g_xTimerAPIKeepAliveHandle, 0);

  while (1) {
    if (xQueueReceive(g_xQueueBridgeHandle, &l_strctQueuePostMsg, 0)) {
      switch (l_strctQueuePostMsg.enmMsgType) {
        case ENM_X_QUEUE_POST_MSG_TYPE::POST_DEVICE_SENSOR_VALUES:
          /*g_bridgeDrv.postTemperatureAndHumidity(l_strctQueuePostMsg.strctTemperatureHumidity.uiDeviceId, 
                                              l_strctQueuePostMsg.strctTemperatureHumidity.byTemperatureQuotientValue,
                                              l_strctQueuePostMsg.strctTemperatureHumidity.byTemperatureRemaindertValue,
                                              l_strctQueuePostMsg.strctTemperatureHumidity.byHumidityQuotientValue,
                                              l_strctQueuePostMsg.strctTemperatureHumidity.byHumidityRemaindertValue);*/
            g_bridgeDrv.postSensorValues(l_strctQueuePostMsg.strctSensorValues);
        break;

        case ENM_X_QUEUE_POST_MSG_TYPE::POST_DEVICE_KEEP_ALIVE:
          g_bridgeDrv.postKeepaliveDevice(l_strctQueuePostMsg.strctSensorValues.uiDeviceId);
        break;

        default:
        break;
      }
    }

    //wait for BIT_EVENT_GROUP_TIMERS__SEND_API_KEEP_ALIVE_TIMER_EXPIRES to bet set, meaning timer g_timerMinAPIKeepAlive has expired
    if (xEventGroupWaitBits(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__SEND_API_KEEP_ALIVE_TIMER_EXPIRES, pdTRUE, pdFALSE, 0) & BIT_EVENT_GROUP_TIMERS__SEND_API_KEEP_ALIVE_TIMER_EXPIRES) {
      g_bridgeDrv.postKeepaliveServer();
    }

    if (xEventGroupWaitBits(g_xEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_BRIDGE_ERASE_WIFI_PARAMS, pdTRUE, pdFALSE, 0) & BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_FACTORY_RESET) {
      g_bridgeDrv.factoryReset();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
#endif

/**
*   AT SETTINGS THREAD: Manage AT Settings via Serial
*
*   params: 
*     pvParameters:     FREERTOS Thread parameters
*   return:
*       NONE       
*/
static void thread_ATSettings( void *pvParameters ) {

  g_ATSettings.init(&g_USBSerial, (STRUCT_GLOBAL_SETTINGS_AND_STATUS *)pvParameters, &g_xEventGroupMiscellaneousHandle, &g_xQueueATSettingsHandle);

  while (1) {
    g_ATSettings.pool();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
 }

/**
*   THREAD TAKS MONITOR THREAD: Return THREAD STATUS
*
*   params: 
*     pvParameters:     FREERTOS Thread parameters
*   return:
*       NONE       
*/
/*
static void thread_TasksMonitor( void *pvParameters ) {
  UBaseType_t uxHighWaterMark;

  while (1) {
    uxHighWaterMark = uxTaskGetStackHighWaterMark( g_xHandleTaskSensorValues );
    LOG_INFO_PRINTLN(LOG_PREFIX_MAIN, "Thread Sensor High Water Mark", uxHighWaterMark);
    uxHighWaterMark = uxTaskGetStackHighWaterMark( g_xHandleTaskRadio );
    LOG_INFO_PRINTLN(LOG_PREFIX_MAIN, "Thread Radio High Water Mark", uxHighWaterMark);
#ifdef BRIDGE_MODE
    uxHighWaterMark = uxTaskGetStackHighWaterMark( g_xHandleTaskBridge );
    LOG_INFO_PRINTLN(LOG_PREFIX_MAIN, "Thread Bridge High Water Mark", uxHighWaterMark);
#endif
    uxHighWaterMark = uxTaskGetStackHighWaterMark( g_xHandleTaskATSettings );
    LOG_INFO_PRINTLN(LOG_PREFIX_MAIN, "Thread AT Settings High Water Mark", uxHighWaterMark);

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}*/

/**
*   MISCELLANEOUS THREAD: Perform miscellaneous actions
*
*   params: 
*     pvParameters:     FREERTOS Thread parameters
*   return:
*       NONE       
*/
static void thread_Miscellaneous( void *pvParameters ) {
  STRUCT_X_QUEUE_MISCELLANEOUS l_strctMiscellaneous;

  while (1) {
    if (xQueueReceive(g_xQueueMiscellaneousHandle, &l_strctMiscellaneous, 0)) {
      switch (l_strctMiscellaneous.enmActionType) {
#ifdef BRIDGE_MODE
        case ENM_X_QUEUE_MISCELLANEOUS_ACTION_TYPE::UPDATE_WIFI_STATUS:
          memcpy(&g_globalSettingsAndStatus.strctWifiStatus, &l_strctMiscellaneous.strctWwifiStatus, sizeof(STRUCT_WIFI_STATUS));
        break;
#endif

        case ENM_X_QUEUE_MISCELLANEOUS_ACTION_TYPE::UPDATE_STATUS_FLASH_LED_SCHEMA:
          g_statusFlashLed.updateModel(l_strctMiscellaneous.strctFlashLedSchemaUpdate.flashLedBuiltinSchema);
        break;

        default:
        break;
      }
    }

    if (xEventGroupWaitBits(g_xEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SAVE_SETTINGS, pdTRUE, pdFALSE, 0) & BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_SAVE_SETTINGS) {
      _g_flashStorageSignatureID.write(FLASH_STORAGE_SIGNATURE_ID);
#ifdef BRIDGE_MODE
      _g_flashSettings.write(*(STRUCT_FLASH_SETTINGS *)(&g_globalSettingsAndStatus.strctBridgeSettings));
#else  
      _g_flashSettings.write(*(STRUCT_FLASH_SETTINGS *)(&g_globalSettingsAndStatus.strctRadioSettings));
#endif
    }

    if (xEventGroupWaitBits(g_xEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_FACTORY_RESET, pdTRUE, pdFALSE, 0) & BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_FACTORY_RESET) {
      _g_flashStorageSignatureID.write(0x0000);
      memset(&g_globalSettingsAndStatus, 0, sizeof(STRUCT_GLOBAL_SETTINGS_AND_STATUS));
#ifdef BRIDGE_MODE
      _g_flashSettings.write(*(STRUCT_FLASH_SETTINGS *)(&g_globalSettingsAndStatus.strctBridgeSettings));
      xEventGroupSetBits(g_xEventGroupMiscellaneousHandle, BIT_EVENT_GROUP_MISCELLANEOUS__PERFORM_BRIDGE_ERASE_WIFI_PARAMS);
#else  
      _g_flashSettings.write(*(STRUCT_FLASH_SETTINGS *)(&g_globalSettingsAndStatus.strctRadioSettings));
#endif
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

/**
*   timer handling callback
*   params: 
*     NONE
*   return:
*       NONE       
*/
void vTimerCallback( TimerHandle_t xTimer ) {
  if (xTimer == g_xTimerStatusFlashLedHandle) {
    g_statusFlashLed.poll();
  }

  if (xTimer == g_xTimerReadSensorValuesHandle) {
    xEventGroupSetBits(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__READ_SENSOR_VALUES_TIMER_EXPIRES);
  }

  if (xTimer == g_xTimerDeviceKeepAliveHandle) {
    xEventGroupSetBits(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__SEND_DEVICE_KEEP_ALIVE_TIMER_EXPIRES);
  }
#ifdef BRIDGE_MODE
  if (xTimer == g_xTimerAPIKeepAliveHandle) {
    xEventGroupSetBits(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__SEND_API_KEEP_ALIVE_TIMER_EXPIRES);
  }
#endif
}

/**
*   pin jumper interrupt handling callback: when pull-down, perform immediat Sensor values measurement broacdast
*   params: 
*     NONE
*   return:
*       NONE       
*/
void jumperInterruptPinCallback(void) {
  BaseType_t l_xHigherPriorityTaskWoken, l_xResult;

  //perform rebounce detection - 1sec min between 2 short-cuts
  if ((millis() - g_iJumberRebounceLastISRTimeMillis) > 1000) {
    l_xHigherPriorityTaskWoken = pdFALSE;

    l_xResult = xEventGroupSetBitsFromISR(g_xEventGroupTimersHandle, BIT_EVENT_GROUP_TIMERS__READ_SENSOR_VALUES_TIMER_EXPIRES, &l_xHigherPriorityTaskWoken);
    if (l_xResult != pdFAIL ) {
      portYIELD_FROM_ISR( l_xHigherPriorityTaskWoken );
    }

    g_iJumberRebounceLastISRTimeMillis = millis();
  }
}

/**
*   Callback when WEBUSB is connected or disconnected.
*   This method switch AT Settings serial port from USBSERIAL to WEBUSB if available
*   params: 
*     p_bConnected: true if connected
*   return:
*       NONE       
*/
void USBWebLineStateCallback(bool p_bConnected) {
  LOG_DEBUG_PRINTLN(LOG_PREFIX_MAIN, "WEBUSP", p_bConnected);
  if (p_bConnected) {
    g_ATSettings.updateSerialPort(&g_USBWeb);
  } else {
    g_ATSettings.updateSerialPort(&g_USBSerial);
  }
}

/**
*   setting-up. Operate as a MAIN
*   params: 
*     NONE
*   return:
*       NONE       
*/
void setup() {
  boolean l_bDeviceHasSettings = false;

  //software reset of the ESP8266 module which has to be reinitialized when the famous "busy p.." error occures.
  //Expressif (https://www.espressif.com/) didn't found useful to fix the-more-than 5 years old bug !! My posts and resquests have been baned !
  pinMode(ESP8266_RESET_PIN, OUTPUT);

  //jumper usage: cut-short in order to force sensor values measurement & Sending
  pinMode(JUMPER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(JUMPER_PIN), jumperInterruptPinCallback, FALLING);

  //set USB serial
  g_USBSerial.begin(115200);
  //while (!g_USBSerial) ;  //shall be commented
  vSetErrorSerial(&g_USBSerial);

  //set WEB serial
  WEBUSB_URL_DEF(l_landingPage, 1 /*https*/, _WEB_USB_LANDING_PAGE);
  g_USBWeb.setLineStateCallback(USBWebLineStateCallback);
  g_USBWeb.setLandingPage(&l_landingPage);
  g_USBWeb.begin();

  //init status flashing led
  g_statusFlashLed.init(STATUS_LED_PIN, LOW, TIMER_PERIOD);

  //retreive unique ID from the SAMD and push it into the global settings and status
  sprintf(&g_globalSettingsAndStatus.cUID[0], "%02X", *(unsigned int *)0x0080A00C);
  g_globalSettingsAndStatus.cUID[8] = '-';
  sprintf(&g_globalSettingsAndStatus.cUID[9], "%02X", *(unsigned int *)0x0080A040);
  g_globalSettingsAndStatus.cUID[17] = '-';
  sprintf(&g_globalSettingsAndStatus.cUID[18], "%02X", *(unsigned int *)0x0080A044);
  g_globalSettingsAndStatus.cUID[26] = '-';
  sprintf(&g_globalSettingsAndStatus.cUID[27], "%02X", *(unsigned int *)0x0080A048);
  g_globalSettingsAndStatus.cUID[35] = '\0';

#ifndef _DEVELOP_
  //ensure that settings have already been registered by checkinh the flash signature, otherwise, ignore and wait for settings
  if (_g_flashStorageSignatureID.read() != FLASH_STORAGE_SIGNATURE_ID) {
    g_statusFlashLed.updateModel(CFlashLed::FLASH_BUILTIN::not_config);
    l_bDeviceHasSettings = false;
  } else {
    g_statusFlashLed.updateModel(CFlashLed::FLASH_BUILTIN::alive);
    l_bDeviceHasSettings = true;

    //retreive settings from flash
  #ifdef BRIDGE_MODE
    _g_flashSettings.read((STRUCT_FLASH_SETTINGS *)&g_globalSettingsAndStatus.strctBridgeSettings);
  #else
    _g_flashSettings.read((STRUCT_FLASH_SETTINGS *)&g_globalSettingsAndStatus.strctRadioSettings);
  #endif
  }
#else
  l_bDeviceHasSettings = true;
  g_statusFlashLed.updateModel(CFlashLed::FLASH_BUILTIN::alive);

  g_globalSettingsAndStatus.strctRadioSettings.uiDeviceID = _RADIO_DEVELOP_DEVICE_ADDR_;
  g_globalSettingsAndStatus.strctRadioSettings.uiMaxRetries = _MAX_RADIO_RETRIES_;
  g_globalSettingsAndStatus.strctRadioSettings.uiKeepAliveTimeout = _RADIO_DEVELOP_KEEPALIVE_TIMEOUT;
  g_globalSettingsAndStatus.strctRadioSettings.uiMessageSignature = _RADIO_MSG_SIGNATURE;
  g_globalSettingsAndStatus.strctRadioSettings.uiOutputPower = CCC1100::ENM_OUTPUT_POWER_DBM::PLUS_7;

  g_globalSettingsAndStatus.strctMiscellaneousSettings.uiReadSensorValuesMeasurementTimeout = _MISCELLANEOUS_READ_SENSOR_VALUES_TIMEOUT_;

  _WEB_USB_LANDING_PAGE

  #ifdef BRIDGE_MODE 
    strcpy(g_globalSettingsAndStatus.strctBridgeSettings.strctAPIServerSettings.cHostname, _BRIDGE_DEVELOP_HOSTNAME_);
    strcpy(g_globalSettingsAndStatus.strctBridgeSettings.strctAPIServerSettings.cUri, _BRIDGE_DEVELOP_URI_);
    g_globalSettingsAndStatus.strctBridgeSettings.strctAPIServerSettings.uiPort = _BRIDGE_DEVELOP_PORT_;
    strcpy(g_globalSettingsAndStatus.strctBridgeSettings.strctAPIServerSettings.cAuthorizationToken, _BRIDGE_DEVELOP_AUTHORIZATION_);
    strcpy(g_globalSettingsAndStatus.strctBridgeSettings.strctAPIServerSettings.cClientID, _BRIDGE_DEVELOP_CLIENTID_);
    g_globalSettingsAndStatus.strctBridgeSettings.strctAPIServerSettings.uiKeepAliveTimeout = _BRIDGE_DEVELOP_KEEPALIVE_TIMEOUT_;

    strcpy(g_globalSettingsAndStatus.strctBridgeSettings.strctWifiSettings.cSSID, _BRIDGE_DEVELOP_AP_SSID_);
    strcpy(g_globalSettingsAndStatus.strctBridgeSettings.strctWifiSettings.cKey, _BRIDGE_DEVELOP_AP_KEY_);
  #else
    g_globalSettingsAndStatus.strctRadioSettings.uiServerID = _RADIO_DEVELOP_SERVER_ADDR_;
  #endif
#endif

//-----------------------------------------------------------------------------
  g_xTimerStatusFlashLedHandle = xTimerCreateStatic("", pdMS_TO_TICKS(10), pdTRUE, ( void * ) 0, vTimerCallback, &g_xTimerStatusFlashLedStatic);
  xTimerStart(g_xTimerStatusFlashLedHandle, 0);

  g_statusFlashLed.start();

  g_xSemaphoreHandleSerial = xSemaphoreCreateMutexStatic(&g_xSemaphoreBufferSerial);
  configASSERT(g_xSemaphoreHandleSerial);

  g_xQueueMiscellaneousHandle = xQueueCreateStatic(X_QUEUE_MISCELLANEOUS_LENGTH, X_QUEUE_MISCELLANEOUS_SIZE, g_xQueueMiscellaneousHandleBuffer, &g_xQueueMiscellaneousHandleStatic);
  configASSERT(g_xQueueMiscellaneousHandle);

  g_xQueueATSettingsHandle = xQueueCreateStatic(X_QUEUE_AT_SETTINGS_LENGTH, X_QUEUE_AT_SETTINGS_SIZE, g_xQueueATSettingsHandleBuffer, &g_xQueueATSettingsHandleStatic);
  configASSERT(g_xQueueATSettingsHandle);

  g_xEventGroupMiscellaneousHandle = xEventGroupCreateStatic(&g_xEventGroupMiscellaneousStatic);
  configASSERT(g_xEventGroupMiscellaneousHandle); 

  if (l_bDeviceHasSettings) {
#ifdef BRIDGE_MODE 
    g_xTimerAPIKeepAliveHandle = xTimerCreateStatic("", pdMS_TO_TICKS(g_globalSettingsAndStatus.strctBridgeSettings.strctAPIServerSettings.uiKeepAliveTimeout * 60000), pdTRUE, ( void * ) 0, vTimerCallback, &g_xTimerAPIKeepAliveStatic);
#endif
    g_xTimerDeviceKeepAliveHandle = xTimerCreateStatic("", pdMS_TO_TICKS(g_globalSettingsAndStatus.strctRadioSettings.uiKeepAliveTimeout * 60000), pdTRUE, ( void * ) 0, vTimerCallback, &g_xTimerDeviceKeepAliveStatic);

    g_xTimerReadSensorValuesHandle = xTimerCreateStatic("", pdMS_TO_TICKS(g_globalSettingsAndStatus.strctMiscellaneousSettings.uiReadSensorValuesMeasurementTimeout * 60000), pdTRUE, ( void * ) 0, vTimerCallback, &g_xTimerReadSensrValuesStatic);
 
    g_xQueueSensorValuesHandle = xQueueCreateStatic(X_QUEUE_SENSOR_VALUES_LENGTH, X_QUEUE_SENSOR_VALUES_SIZE, g_xQueueSensorValuesHandleBuffer, &g_xQueueSensorValuesHandleStatic);
    configASSERT(g_xQueueSensorValuesHandle);

#ifdef BRIDGE_MODE
    g_xQueueBridgeHandle = xQueueCreateStatic(X_QUEUE_BRIDGE_LENGTH, X_QUEUE_BRIDGE_SIZE, g_xQueueBridgeHandleBuffer, &g_xQueueBridgeHandleStatic);
    configASSERT(g_xQueueBridgeHandle);
#endif

  g_xEventGroupTimersHandle = xEventGroupCreateStatic(&g_xEventGroupTimersStatic);
  configASSERT(g_xEventGroupTimersHandle); 

  g_xHandleTaskSensorValues = xTaskCreateStatic(thread_SensorValues, "", X_BUFFER_TASK_SENSOR_VALUES_SIZE, (void *)&g_globalSettingsAndStatus.strctRadioSettings, tskIDLE_PRIORITY + 3, g_xBufferTaskSensorValues, &g_xTCBTaskSensorValues);
  g_xHandleTaskRadio = xTaskCreateStatic(thread_Radio, "", X_BUFFER_TASK_RADIO, (void *)&g_globalSettingsAndStatus.strctRadioSettings, tskIDLE_PRIORITY + 3, g_xBufferTaskRadio, &g_xTCBTaskRadio);
#ifdef BRIDGE_MODE
  g_xHandleTaskBridge = xTaskCreateStatic(thread_Bridge, "", X_BUFFER_TASK_BRIDGE, (void *)&g_globalSettingsAndStatus.strctBridgeSettings, tskIDLE_PRIORITY + 3, g_xBufferTaskBridge, &g_xTCBTaskBridge);
#endif
  }
  
  g_xHandleTaskMiscellaneous = xTaskCreateStatic(thread_Miscellaneous, "", X_BUFFER_TASK_MISCELLANEOUS, NULL, tskIDLE_PRIORITY + 2, g_xBufferTaskMiscellaneous, &g_xTCBTaskTasksMiscellaneous);
  g_xHandleTaskATSettings = xTaskCreateStatic(thread_ATSettings, "", X_BUFFER_TASK_AT_SETTINGS,  (void *)&g_globalSettingsAndStatus, tskIDLE_PRIORITY + 2, g_xBufferTaskATSettings, &g_xTCBTaskATSettings);

  vTaskStartScheduler();

  for( ; ; );
  }

void loop() {
  //Serial.flush();	
}

