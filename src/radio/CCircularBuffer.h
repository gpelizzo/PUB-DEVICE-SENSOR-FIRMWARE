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
#ifndef _CCIRCULAR_BUFFER_H_
#define _CCIRCULAR_BUFFER_H_

#include <arduino.h>

//maximum count of buffers
#define MAX_CIRCULAR_BUFFER_FIXED_PAGES_COUNT       20
//size of an unique buffer
//size of CCC1100::STRUCT_RADIO_PAYLOAD_HEADER + size of CCC1100::STRUCT_RADIO_PAYLOAD_MESSAGE
#define CIRCULAR_BUFFER_FIXED_PAGE_SIZE             6 + (2 + 35)

class CCircularBuffer {
public:
    void init();
    boolean push(byte *p_pbyArray);
    int8_t pull(byte *p_pbyArray);
    boolean isEmpty();
private:
    //circular buffer unit object
    struct buffer_page {
        boolean bFree;
        byte bufferData[CIRCULAR_BUFFER_FIXED_PAGE_SIZE];
        buffer_page *pNext;
    };

    //circular buffers byte allocation
    buffer_page m_PagesBuffer[MAX_CIRCULAR_BUFFER_FIXED_PAGES_COUNT];
    //current size of the circular buffer
    byte m_byCurrentSize;

    //pointer to the head of the circular buffers
    buffer_page *mCirculartBufferHead = NULL;
    //pointer to the tail of the circular buffers
    buffer_page *mCirculartBufferTail = NULL;
    //retrieve a free circular buffer
    buffer_page *getFreeBufferPage();
};

#endif


