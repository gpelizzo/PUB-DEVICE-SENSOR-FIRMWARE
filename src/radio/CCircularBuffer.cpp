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
#include "CCircularBuffer.h"
#include "logging.h"

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
*   Init FIFO circular buffers  
*   params: 
*       NONE
*   return:
*       NONE       
*/
void CCircularBuffer::init() {
    m_byCurrentSize = 0;
    int l_iPageCounter;

    //clear all reserved buffers
    for (l_iPageCounter=0; l_iPageCounter < MAX_CIRCULAR_BUFFER_FIXED_PAGES_COUNT; l_iPageCounter++) {
        m_PagesBuffer[l_iPageCounter].bFree = true;
        memset(&m_PagesBuffer[l_iPageCounter].bufferData, 0, CIRCULAR_BUFFER_FIXED_PAGE_SIZE);
        m_PagesBuffer[l_iPageCounter].pNext = NULL;
    }
}

/**
*   push bytes array into a free circular buffer  
*   params: 
*       p_pbyArray:                 pointer to the first byte of the arry. Size is fixed and defined into header. 
*                                   cf CIRCULAR_BUFFER_FIXED_PAGE_SIZE  
*   return:
*       true if array has been added, otherwise return false, meaning that no free buffer available       
*/
boolean CCircularBuffer::push(byte *p_pbyArray) {
    //check if a free circulat buffer is available 
    if (m_byCurrentSize++ <= MAX_CIRCULAR_BUFFER_FIXED_PAGES_COUNT) {

        CCircularBuffer::buffer_page *l_pFreeBufferPage;
        
        //retreive a pointer to a free circular buffer
        l_pFreeBufferPage = getFreeBufferPage();

        //extra checking, but should not occured
        if (l_pFreeBufferPage == NULL) {
            return false;
        } else {
            //copy byte array to circular buffer
            memcpy(&l_pFreeBufferPage->bufferData[0], p_pbyArray, CIRCULAR_BUFFER_FIXED_PAGE_SIZE);
            l_pFreeBufferPage->bFree = false;
        }

        //add circular buffer to the circular object, depending on the situation: first buffer or new one
        if (mCirculartBufferHead == NULL) {
            mCirculartBufferHead = l_pFreeBufferPage;
            mCirculartBufferTail = l_pFreeBufferPage;
            l_pFreeBufferPage->pNext = mCirculartBufferHead;
        } else {
            mCirculartBufferTail->pNext = l_pFreeBufferPage;
            mCirculartBufferTail = l_pFreeBufferPage;
            l_pFreeBufferPage->pNext = mCirculartBufferHead;
        }

        return true;
    } else {
        return false;
    }
}

/**
*   retreive a byte array from the circular buffers. FIFO  
*   params: 
*       p_pbyArray:                 pointer to the first byte of the arry to receive the circular buffer
*   return:
*       true if byte array has been retreive, otherwise return false, meaning that circular buffer is empty       
*/
int8_t CCircularBuffer::pull(byte *p_pObject) {

    //check if circular buffer is empty
    if (mCirculartBufferHead == NULL) {
        return -1;
    } else {
        //copy FIFO circular buffer into the destination byte array
        memcpy(p_pObject, &mCirculartBufferHead->bufferData[0], CIRCULAR_BUFFER_FIXED_PAGE_SIZE);
        //free circular buffer
        mCirculartBufferHead->bFree = true;

        //disconnect circular buffer from the circular buffers
        if (mCirculartBufferHead == mCirculartBufferHead->pNext) {
            mCirculartBufferHead = NULL;
            mCirculartBufferTail = NULL;
        } else {
            mCirculartBufferHead = mCirculartBufferHead ->pNext;
            mCirculartBufferTail->pNext = mCirculartBufferHead;
        }

        m_byCurrentSize--;

        return CIRCULAR_BUFFER_FIXED_PAGE_SIZE;
    }
}

/**
*   Check if circular buffers contains at least 1 buffer with available byte data array
*   params: 
*       NONE
*   return:
*       true if circular buffers is empty. Otherwise false.       
*/
boolean CCircularBuffer::isEmpty() {
    return (m_byCurrentSize == 0) ? true : false;
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
*   retreive a free circular buffer
*   params: 
*       NONE
*   return:
*       buffer_page:        pointer to a free cirdular buffer. Otherwise NULL meaning that circular buffers is full       
*/
CCircularBuffer::buffer_page *CCircularBuffer::getFreeBufferPage() {
    int l_iPageCounter;

    //look for a circular buffer
    for (l_iPageCounter=0; l_iPageCounter < MAX_CIRCULAR_BUFFER_FIXED_PAGES_COUNT; l_iPageCounter++) {
        if (m_PagesBuffer[l_iPageCounter].bFree) {
            return &m_PagesBuffer[l_iPageCounter];
        }
    }

    return NULL;
}
