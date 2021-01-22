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

#ifndef __CCHAR_BUFFER_TOOL_H__
#define __CCHAR_BUFFER_TOOL_H__

#include <Arduino.h>
#include "global.h"

#ifdef BRIDGE_MODE
    #include "logging.h"

    class CCharBufferTool {
    public:
        enum ENM_STATUS {SUCCEEDED=0, ERROR_BUFFER_TOO_LARGE = -1};
        enum ENM_LIST_TYPE {AND=0, OR};

        void            init(char *p_pcBuffer, size_t p_sztBufferMaxLengt);
        ENM_STATUS      start(const char *p_pccData);
        ENM_STATUS      start(const char *p_pccData, size_t p_sztLength);
        ENM_STATUS      concat(const char *p_pccData);
        char            *getBuffer();
        ENM_STATUS      startList(const char *p_pccData, ENM_LIST_TYPE p_enmListType = ENM_LIST_TYPE::AND);
        ENM_STATUS      insertToList(const char *p_pccData);
        char            *getListIndex(uint8_t puiIndex);
        uint8_t         getListCount();
        ENM_LIST_TYPE   getListType();
        ENM_STATUS      copy(char *p_pcDestBuffer, size_t p_sztMaxLength);

    private:
        char            *m_pcBuffer = NULL;
        size_t          m_sztBufferMaxLength;
        size_t          m_sztUsedLength = 0;
        size_t          m_sztBufferSize = 0;
        uint8_t         m_uiListCount = 0;
        ENM_LIST_TYPE   m_enmListType;

        ENM_STATUS      append(const char *p_pccData, boolean p_bList);
    };

    #endif
#endif