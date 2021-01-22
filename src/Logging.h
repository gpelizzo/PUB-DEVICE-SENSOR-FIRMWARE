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
#ifndef __LOGGING_H__
#define __LOGGING_H__

#include "Global.h"

#define LOG_OUTPUT                                      Serial

#define LOG_LEVEL_SILENT                                0
#define LOG_LEVEL_ERROR                                 1
#define LOG_LEVEL_WARN                                  2
#define LOG_LEVEL_INFO                                  3
#define LOG_LEVEL_DEBUG                                 4

#define LOG_PREFIX_ESP8266                              "ESP8266"
#define LOG_PREFIX_MAIN                                 "MAIN"
#define LOG_PREFIX_CC1100                               "CC1100"
#define LOG_PREFIX_CHAR_BUFFER_TOOL                     "CHAR-BUFFER-TOOL"
#define LOG_PREFIX_BRIDGE                               "BRIDGE"
#define LOG_PREFIX_AT_SETTINGS                          "AT-SETTINGS"     
#define LOG_PREFIX_CHTU21                               "CHTU21"                  


#define LOG_LEVEL                                       LOG_LEVEL_SILENT


#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR_PRINTLN(origin, msg, value)           log_println("LOG-ERROR [", origin, msg, value)
#else
#define LOG_ERROR_PRINTLN(origin, msg, value)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN_PRINTLN(origin, msg, value)            log_println("LOG-WARN [", origin, msg, value)
#else
#define LOG_WARN_PRINTLN(origin, msg, value)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO_PRINTLN(origin, msg, value)            log_println("LOG-INFO [", origin, msg, value)
#else
#define LOG_INFO_PRINTLN(origin, msg, value)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG_PRINTLN(origin, msg, value)           log_println("LOG-DEBUG [", origin, msg, value)
#else
#define LOG_DEBUG_PRINTLN(origin, msg, value)
#endif


extern SemaphoreHandle_t g_xSemaphoreHandleSerial;

template<typename S, typename T, typename U, typename V> 
void log_println(S p_type, T p_origin, U p_msg, V p_value) {
    if (LOG_OUTPUT) {
        if (xSemaphoreTake(g_xSemaphoreHandleSerial, portMAX_DELAY) == pdTRUE ) {

        ////to be commented
        //if ((strncmp(p_origin, LOG_PREFIX_CC1100, strlen(LOG_PREFIX_CC1100)) == 0) || (strncmp(p_origin, LOG_PREFIX_MAIN, strlen(LOG_PREFIX_MAIN)) == 0) 
        //            || ((strncmp(p_origin, LOG_PREFIX_ESP8266, strlen(LOG_PREFIX_ESP8266)) == 0) && (strncmp(p_type, "LOG-ERROR [", strlen("LOG-ERROR [")) == 0))) {
        ////
            LOG_OUTPUT.print(p_type);
            LOG_OUTPUT.print(p_origin);
            LOG_OUTPUT.print("] => ");
            LOG_OUTPUT.print(p_msg);
            LOG_OUTPUT.print(": ");
            LOG_OUTPUT.println(p_value);
        ////
        //}
        ////
        }

        xSemaphoreGive(g_xSemaphoreHandleSerial);
    }
}

#endif