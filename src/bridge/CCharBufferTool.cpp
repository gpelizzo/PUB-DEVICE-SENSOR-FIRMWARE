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
#include "CCharBufferTool.h"

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
*   Init char buffer tool. This object simplify chars array manipulation, including concatenation
*   and list managements.
*   Chars buffer is set with 0xFF in order to manage list. In thatr case, items are separated by '\0'  
*   params: 
*       p_pcBuffer:                 pointer to char/byte reserved array
*       p_sztBufferMaxLength:       length of the reserved array
*   return:
*       NONE   
*/
void CCharBufferTool::init(char *p_pcBuffer, size_t p_sztBufferMaxLength) {
    m_pcBuffer = p_pcBuffer;
    m_sztBufferMaxLength = p_sztBufferMaxLength;
}

/**
*   start a simple chars handling by setting the first data
*   params: 
*       p_pccData:                  data to set as starting buffer initialization
*   return:
*       NONE   
*/
CCharBufferTool::ENM_STATUS CCharBufferTool::start(const char *p_pccData) {
    return start(p_pccData, strlen(p_pccData));
}

/**
*   start a simple chars handling by setting the first data and termination char for at a
*   specific length. Because chars array is initialized with 0xFF and not '\0', this length
*   is sometime required in order to terminate a string when data are added without '\0'.
*   params: 
*       p_pccData:                  data to set as starting buffer initialization
*       p_sztLength:                length of the string
*   return:
*       NONE   
*/
CCharBufferTool::ENM_STATUS CCharBufferTool::start(const char *p_pccData, size_t p_sztLength) {
    if ((p_sztLength + 1) > m_sztBufferMaxLength) {
        LOG_ERROR_PRINTLN(LOG_PREFIX_CHAR_BUFFER_TOOL, "Set", "BUFFER REQUESTED TOO LARGE");
        return CCharBufferTool::ENM_STATUS::ERROR_BUFFER_TOO_LARGE;
    }

    memset(m_pcBuffer, 0xFF, m_sztBufferSize);
    memcpy(m_pcBuffer, p_pccData, p_sztLength);
    *(m_pcBuffer + p_sztLength) = '\0';
    m_sztUsedLength = p_sztLength + 1;
    m_uiListCount = 1;

    return CCharBufferTool::ENM_STATUS::SUCCEEDED;
}

/**
*   concat chars array with a new chars array
*   params: 
*       p_pccData:                      new chars array item to inster
*   return: 
*       CCharBufferTool::ENM_STATUS        
*/
CCharBufferTool::ENM_STATUS CCharBufferTool::concat(const char *p_pccData) {
    return append(p_pccData, false);
}

/**
*   return a pointer to the chars array buffer
*   params: 
*       NONE
*   return: 
*       char * to the buffer with '\0' terminate char        
*/
char *CCharBufferTool::getBuffer() {
    return m_pcBuffer;
}

/**
*   start a chars list handling by setting the first item
*   params: 
*       p_pccData:                  first chars array item
*       p_enmListType:              init list behaviour, meaning AND/OR. 
*                                   if AND, all items into the list shall be founded
*                                   if OR, one item is enougth
*   return:
*       NONE   
*/
CCharBufferTool::ENM_STATUS CCharBufferTool::startList(const char *p_pccData, CCharBufferTool::ENM_LIST_TYPE p_enmListType) {
    m_enmListType = p_enmListType;
    return start(p_pccData);
}

/**
*   returns list behaviour type: AND/OR
*   params: 
*       NONE
*   return:
*       CCharBufferTool::ENM_LIST_TYPE   
*/
CCharBufferTool::ENM_LIST_TYPE CCharBufferTool::getListType() {
    return m_enmListType;
}

/**
*   insert a new item into the list
*   params: 
*       p_pccData:                      new chars array item to inster
*   return: 
*       CCharBufferTool::ENM_STATUS        
*/
CCharBufferTool::ENM_STATUS CCharBufferTool::insertToList(const char *p_pccData) {
    CCharBufferTool::ENM_STATUS l_enmstatusRetValue = append(p_pccData, true);

    if (l_enmstatusRetValue == CCharBufferTool::ENM_STATUS::SUCCEEDED) {
        m_uiListCount++;
    }

    return l_enmstatusRetValue;
}

/**
*   returns a char pointer to the corresponding item of a list
*   params: 
*       puiIndex:                       index of the item (0 based)
*   return: 
*       char * to the item with '\0' terminate char        
*/
char *CCharBufferTool::getListIndex(uint8_t puiIndex) {

    if ((puiIndex >= m_uiListCount) || (m_uiListCount == 0)) {
        return NULL;
    }

    uint8_t l_uiCounter = 0;
    byte *l_pbyPosition = (byte *)&m_pcBuffer[0];
    
    do {
        if (l_pbyPosition >= (byte *)(m_pcBuffer + m_sztUsedLength)) {
            break;
        }

        if (puiIndex == 0) {
            return m_pcBuffer;
        }

        l_pbyPosition = (byte *)strchr((char *)l_pbyPosition, '\0');
        if (l_pbyPosition != NULL) {
            l_uiCounter++;
            if (l_uiCounter == puiIndex) {
                return (char *)(l_pbyPosition + 1);
            }
            l_pbyPosition += 1;
        }
    } while (l_pbyPosition != NULL);

    return NULL;
}

/**
*   returns the number of items into a list
*   params: 
*       NONE
*   return: 
*       number of items        
*/
uint8_t CCharBufferTool::getListCount() {
    return m_uiListCount;
}

/**
*   copy a part of the chars buffer defined by a derterminated length
*   params: 
*       p_pcDestBuffer:                     buffer to copy to
*       p_sztMaxLength:                     length to copy
*   return: 
*       CCharBufferTool::ENM_STATUS        
*/
CCharBufferTool::ENM_STATUS CCharBufferTool::copy(char *p_pcDestBuffer, size_t p_sztMaxLength) {
    if (m_sztUsedLength > p_sztMaxLength) {
        return CCharBufferTool::ENM_STATUS::ERROR_BUFFER_TOO_LARGE;
    }

    memcpy(p_pcDestBuffer, m_pcBuffer, m_sztUsedLength);

    return CCharBufferTool::ENM_STATUS::SUCCEEDED;
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
*   append a chars array according to the usage: list or not.
*   if list, add a termination '\0' after each item. Otherwize, just append the the chars array. 
*   params: 
*       p_pccData:                      new chars array to append
*       p_bList:                        true if list
*   return: 
*       CCharBufferTool::ENM_STATUS        
*/
CCharBufferTool::ENM_STATUS CCharBufferTool::append(const char *p_pccData, boolean p_bList) {
     size_t l_sztSize = strlen(p_pccData);

     if ((m_sztUsedLength + l_sztSize + (p_bList ? 1 : 0)) > m_sztBufferMaxLength) {
         LOG_ERROR_PRINTLN(LOG_PREFIX_CHAR_BUFFER_TOOL, "concat", "BUFFER REQUESTED TOO LARGE");
        return CCharBufferTool::ENM_STATUS::ERROR_BUFFER_TOO_LARGE;
    }

    memcpy(m_pcBuffer + m_sztUsedLength - (p_bList ? 0 : 1), p_pccData, l_sztSize);
    *(m_pcBuffer + m_sztUsedLength + l_sztSize - (p_bList ? 0 : 1)) = '\0';
    m_sztUsedLength += ( l_sztSize + (p_bList ? 1 : 0));

    return CCharBufferTool::ENM_STATUS::SUCCEEDED;
}
#endif