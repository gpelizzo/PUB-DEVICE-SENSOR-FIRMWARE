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

/**
 * This library is largely inspired by SpaceTeddy CC1101 Raspberry and Arduino libraries. cf https://github.com/SpaceTeddy/CC1101 
 * 
 */

#include "CCC1100.h"

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
*   Init device  
*   params: 
*       p_byDeviceAdd:              address of the device
*       p_byOutputPowerLevel:       output power level
*   return:
*       true in init has been performed. Else, false       
*/
boolean CCC1100::init(byte p_byDeviceAdd, ENM_OUTPUT_POWER_DBM p_byOutputPowerLevel, uint16_t p_uiMsgSignature) {
    
    if ((p_byDeviceAdd == 0) || (p_byOutputPowerLevel == 0)) {
        return false;
    }

    m_RXCircularBuffer.init();

    pinMode(PIN_CC1100_GD02, INPUT_PULLDOWN);
    pinMode(PIN_CC1100_CS, OUTPUT);

    SPI.begin();

    digitalWrite(PIN_CC1100_CS, LOW);
    delayMicroseconds(10);
    digitalWrite(PIN_CC1100_CS, HIGH);
    delayMicroseconds(40);

    spiWriteStrobe(SRES);
    delay(1);

    spiWriteStrobe(SFTX);
    delayMicroseconds(100);
    spiWriteStrobe(SFRX);
    delayMicroseconds(100);

    setBaudeRateAndModulation(CCC1100::ENM_BAUD_RATE_MODULATION::GFSK_100_kb);
    
    setISMBand(CCC1100::ENM_ISM_BAND::ISM_868);

    setChannel(1);

    m_byDeviceAddr = p_byDeviceAdd;
    setDeviceAddr(p_byDeviceAdd);
    
    m_uiMessageSignature = p_uiMsgSignature;

    setOutputPowerLevel(p_byOutputPowerLevel);

    setReceiveMode();

    return true;
}


/**
*   Poll device in order to: 1)retreive incoming messages 2)retreive responses  
*   params: 
*       NONE
*   return:
*       TRUE if a new incoming buffer is available and pending into the cirdcular buffers     
*/
boolean CCC1100::poll() {
    boolean bRetValue = false;

    //LOG_DEBUG_PRINTLN(LOG_PREFIX_CC1100, "poll", "");
    if (digitalRead(PIN_CC1100_GD02) == 1) {
        bRetValue = getPayload();
    }
    
    if (!m_RXCircularBuffer.isEmpty()) {
        return true;
    }

    return bRetValue;
}


/**
*   Return last incoming message  
*   params: 
*       p_pstrRadioPaylodBuffer:    STRUCT_RADIO_PAYLOAD receiving the message payload
*   return:
*       address of the device which sent the payload. -1 if no payload available       
*/
int16_t CCC1100::getMessage(STRUCT_RADIO_PAYLOAD_MESSAGE *p_pstrRadioPayloadMessage) {
    if (m_RXCircularBuffer.pull(&m_unPayloadRXFIFOBuffer.byArray[0]) != -1) {
        memcpy(p_pstrRadioPayloadMessage, &m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayloadMessage, sizeof(STRUCT_RADIO_PAYLOAD_MESSAGE));
        return (int16_t)m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.bySenderAddr;
    } else {
        return -1;
    }
}


/**
*   Retreive the version of the device firmware  
*   params: 
*       NONE
*   return:
*       Version       
*/
byte CCC1100::getVersion() {
    return spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS::VERSION);
}

/**
*   Retreive the part number of the device firmware  
*   params: 
*       NONE
*   return:
*       part number       
*/
byte CCC1100::getPartNumber() {
    return spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS::PARTNUM);
}


/**
*   Send a packet  
*   params: 
*       p_pstrRadioPayload:     STRUCT_RADIO_PAYLOAD containing payload informations  
*       p_byTXRetryMax:         number of retries if no acknowledge is received
*   return:
*       NONE       
*/
boolean CCC1100::postMessage(byte p_pyRecipientAddr, STRUCT_RADIO_PAYLOAD_MESSAGE *p_pstrRadioPayloadMessageRadioPayload, byte p_byTXRetryMax) {
    byte p_byTXRetryCount = 0;
    
    if (p_pstrRadioPayloadMessageRadioPayload->byDataLength > MAX_RADIO_MESSAGE_DATA_LENGTH) {
        return false;
    } else {
        do {
            memcpy(&m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayloadMessage, p_pstrRadioPayloadMessageRadioPayload, sizeof(STRUCT_RADIO_PAYLOAD_MESSAGE));
            m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.bySenderAddr = m_byDeviceAddr;
            m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byRecipientAddr = p_pyRecipientAddr;
            m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.wMessageToken = m_uiMessageSignature;
            m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadType = ENM_PAYLOAD_TYPE::MSG;

            m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadLength = 
                                            p_pstrRadioPayloadMessageRadioPayload->byDataLength + 
                                            sizeof(STRUCT_RADIO_PAYLOAD_HEADER) +
                                            sizeof(STRUCT_RADIO_PAYLOAD_MESSAGE) -
                                            MAX_RADIO_MESSAGE_DATA_LENGTH - 1;

            TXPayloadBurst(&m_unPayloadTXFIFOBuffer);
            setTransmitMode();
            setReceiveMode();

            if (p_pyRecipientAddr == BROADCAST_ADDRESS) {
                return true;
            } else {
                vTaskDelay(pdMS_TO_TICKS(300));

                poll();

                if (m_bReceivedAck) {
                    m_bReceivedAck = false;
                    return true;
                }

                LOG_DEBUG_PRINTLN(LOG_PREFIX_CC1100, "p_byTXRetryCount", p_byTXRetryCount);

                p_byTXRetryCount++;
            }
        } while (p_byTXRetryCount <= p_byTXRetryMax);

        return false;
    }
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
*   Retreive full payload. cf STRUCT_RADIO_PAYLOAD
*   RXPayloadBurst can return not only a single payload, but someting more than one.
*   this function splits returned buffer into single payload frames
*   params: 
*       NONE
*   return:
*       TRUE if a new incoming message has been added to the circular buffers., otherwise FALSE       
*/
boolean CCC1100::getPayload() {
    int8_t l_iReceivedLength;
    int8_t l_iRemainingBytes;

    memset(&m_unPayloadRXFIFOBuffer.byArray[0], 0, FIFO_BUFFER_SIZE);
    if ((l_iReceivedLength = RXPayloadBurst()) != -1) {
        l_iRemainingBytes = l_iReceivedLength;

        LOG_DEBUG_PRINTLN(LOG_PREFIX_CC1100, "l_uiReceivedLength", l_iReceivedLength);

        //check if retreived more than one frame. 
        do {
            checkUnitPayload(&m_unPayloadRXFIFOBuffer);

            //+2 include RSSI and LQI added bytes. +1 includes first length byte
            l_iRemainingBytes -= (m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadLength + 1 + 2); 
            
            if (l_iRemainingBytes > 0) {
                memmove(&m_unPayloadRXFIFOBuffer.byArray[0], 
                        &m_unPayloadRXFIFOBuffer.byArray[m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadLength + 1 + 2],
                        m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadLength + 1 + 2);
            }
            
        } while (l_iRemainingBytes > 0);
        
        return true; 
    }

    return false;
}


/**
*   Retreive full payload  following interrupt. cf STRUCT_RADIO_PAYLOAD
*   params: 
*       NONE
*   return:
*       TRUE if a new incoming message has been added to the circular buffers., otherwise FALSE       
*/
boolean CCC1100::checkUnitPayload(CCC1100::UNION_PAYLOAD *p_punionPayload) {
    //check if token is correct. Otherwise discard. 
    if ((m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.wMessageToken == m_uiMessageSignature)) {

        if (checkAcknowledge(p_punionPayload->strctPayLoad.strctPayLoadHeader.byRecipientAddr, 
                        p_punionPayload->strctPayLoad.strctPayLoadHeader.bySenderAddr, 
                        p_punionPayload->strctPayLoad.strctPayLoadHeader.byPayloadType)) {
            LOG_DEBUG_PRINTLN(LOG_PREFIX_CC1100, "getPayload", "Receive acknowledge");
            m_bReceivedAck = true;
        } else {
            if (m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byRecipientAddr != BROADCAST_ADDRESS) {
              sendAcknowledge(m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.bySenderAddr); 
            }

            if (m_unPayloadRXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadType == ENM_PAYLOAD_TYPE::MSG) {
  
                boolean m_bRetValue = m_RXCircularBuffer.push(&m_unPayloadRXFIFOBuffer.byArray[0]);

                if (m_bRetValue) {
                    return true;
                } else {
                    LOG_ERROR_PRINTLN(LOG_PREFIX_CC1100, "getPayload", "Cant't add to circular buffer");
                }
            } else {
                LOG_ERROR_PRINTLN(LOG_PREFIX_CC1100, "ENM_PAYLOAD_TYPE::MSG", "");
            }
        }
    }

    return false;
}

/**
 *   Send outgoing paylaod.  
 *   Payload format:
 *      [0]: payload length
 *      [1]: recipient address
 *      [2]: sender address
 *      [3]: code (cf ENM_PAYLOAD_TYPE)
 *      [4]: message length
 *      [5....]: message  
 *   params: 
 *       p_byRecipientAddr:  recipient device address   
 *       p_byDataArray:      byte array to transmit
 *       p_byLengthToSend:   length of bute array to transmit
 *   return:
 *       NONE       
 */
void CCC1100::TXPayloadBurst(UNION_PAYLOAD *p_pstrctunionPayload) {
    spiWriteBurst(ENM_CC1101_WRITE_BURST_COMMANDS::TXFIFO_ARRAY, &p_pstrctunionPayload->byArray[0], 
                        p_pstrctunionPayload->strctPayLoad.strctPayLoadHeader.byPayloadLength);
}


/**
 *   Retreive incoming paylaod.  
 *   params: 
 *       NONE
 *   return:
 *       length of payload. -1 if no payload available       
 */
int8_t CCC1100::RXPayloadBurst() {
    byte l_byRXLengthBuferPending;

    //reads the number of bytes in RXFIFO
    l_byRXLengthBuferPending = spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS::RXBYTES);

    LOG_DEBUG_PRINTLN(LOG_PREFIX_CC1100, "l_byRXLengthBuferPending", l_byRXLengthBuferPending);

    l_byRXLengthBuferPending &= 0x7F;

    //if bytes in buffer and no RX Overflow
    if ((l_byRXLengthBuferPending & 0x7F) && !(l_byRXLengthBuferPending & 0x80)) {
        //read payload and store into RX FIFO
        spiReadBurst(ENM_CC1101_READ_BURST_COMMANDS::RXFIFO_ARRAY, &m_unPayloadRXFIFOBuffer.byArray[0], l_byRXLengthBuferPending);
        return l_byRXLengthBuferPending;
    } else {
        LOG_DEBUG_PRINTLN(LOG_PREFIX_CC1100, "overflow", "");
        //set to IDLE
        sidle();
        //flush RX Buffer
        spiWriteStrobe(ENM_CC1101_STROBE_COMMANDS::SFRX);
        delayMicroseconds(100);
        //set to receive mode
        setReceiveMode();
        return -1;
    }
}

/**
 *   check if incomming message is an acknowledge
 *   params: 
 *       p_byDeviceAddr:     this device address 
 *   return:
 *       true if incoming message is n acknowledge, else false       
 */
boolean CCC1100::checkAcknowledge(byte p_byRecipientAddr, byte p_bySenderAddr, byte p_byType) {
    if ((p_byRecipientAddr == BROADCAST_ADDRESS) || (p_byType != ENM_PAYLOAD_TYPE::ACK)) {
        return false;
    } else {
        if (p_byType == ENM_PAYLOAD_TYPE::ACK) {
            return true;
        } else {
            return false;
        }
    }
 }

/**
 *   Send acknowledge to the sender
 * *   Payload format:
 *      [0]: payload length
 *      [1]: recipient address
 *      [2]: sender address
 *      [3]: code (cf ENM_PAYLOAD_TYPE)
 *   params: 
 *       p_bySenderAddr :    sender device address
 *   return:
 *       NONE       
 */
void CCC1100::sendAcknowledge(byte p_byRecipientddr) {
    
    m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.bySenderAddr = m_byDeviceAddr;
    m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byRecipientAddr = p_byRecipientddr;
    m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadType = ENM_PAYLOAD_TYPE::ACK;
    m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.wMessageToken = m_uiMessageSignature;
    m_unPayloadTXFIFOBuffer.strctPayLoad.strctPayLoadHeader.byPayloadLength = sizeof(STRUCT_RADIO_PAYLOAD_HEADER) - 1;

    TXPayloadBurst(&m_unPayloadTXFIFOBuffer);

    setTransmitMode();
    setReceiveMode();
}


/**
 *   Set the device into transmit mode state
 *   params: 
 *       NONE 
 *   return:
 *       NONE       
 */
void CCC1100::setTransmitMode() {
    byte l_byMarcState;

    //sets to idle first.
    sidle();
    //writes receive strobe (receive mode)
    spiWriteStrobe(ENM_CC1101_STROBE_COMMANDS::STX);

    //set unknown/dummy state value
    l_byMarcState = 0xFF;

    //read out state of cc1100 to be sure in IDLE and TX is finished
    while (l_byMarcState != ENM_CC1101_MARCSTATES::IDLE) {
        l_byMarcState = (spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS::MARCSTATE) & 0x1F);
    }

    delayMicroseconds(100);
}

/**
 *   Set the device into receive mode state
 *   params: 
 *       NONE 
 *   return:
 *       NONE       
 */
void CCC1100::setReceiveMode() {
    byte l_byMarcState;

    //sets to idle first.
    sidle();
    //writes receive strobe (receive mode)
    spiWriteStrobe(ENM_CC1101_STROBE_COMMANDS::SRX);

    //set unknown/dummy state value
    l_byMarcState = 0xFF;

    //read out state of cc1100 to be sure in RX
    while (l_byMarcState != ENM_CC1101_MARCSTATES::RX) {
        l_byMarcState = (spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS::MARCSTATE) & 0x1F);
    }

    delayMicroseconds(100);
}

/**
 *   Clear all pending command strobes until IDLE state is reached
 *   params: 
 *       NONE 
 *   return:
 *       NONE       
 */
void CCC1100::sidle() {
    byte l_byMarcState;

    //sets to idle first. 
    spiWriteStrobe(ENM_CC1101_STROBE_COMMANDS::SIDLE);

    //set unknown/dummy state machine value
    l_byMarcState = 0xFF;

    //read out state of cc1100 to be sure in IDLE
    while (l_byMarcState != ENM_CC1101_MARCSTATES::IDLE) {
        l_byMarcState = (spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS::MARCSTATE) & 0x1F);
    }

    delayMicroseconds(100);
}

/**
 *   Set the device address
 *   params: 
 *       p_byAddr:   adress 
 *   return:
 *       NONE       
 */
void CCC1100::setDeviceAddr(byte p_byAddr) {
    //stores MyAddr in the CC1100
    spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS::ADDR, p_byAddr);          
}

/**
 *   Set output power level in DB
 *   params: 
 *       p_enumDBPowerLever:   value of the output pwer signal level 
 *   return:
 *       NONE       
 */
void CCC1100::setOutputPowerLevel(ENM_OUTPUT_POWER_DBM p_enumDBPowerLever) {
    byte l_Pa = 0xC0;

    switch(p_enumDBPowerLever) {
        case ENM_OUTPUT_POWER_DBM::MINUS_30:
            l_Pa = 0x00;
            break;

        case ENM_OUTPUT_POWER_DBM::MINUS_20:
            l_Pa = 0x01;
            break;

        case ENM_OUTPUT_POWER_DBM::MINUS_15:
            l_Pa = 0x02;
            break;

        case ENM_OUTPUT_POWER_DBM::MINUS_10:
            l_Pa = 0x03;
            break;

        case ENM_OUTPUT_POWER_DBM::PLUS_0:
            l_Pa = 0x04;
            break;

        case ENM_OUTPUT_POWER_DBM::PLUS_5:
            l_Pa = 0x05;
            break;

        case ENM_OUTPUT_POWER_DBM::PLUS_7:
            l_Pa = 0x06;
            break;

        case ENM_OUTPUT_POWER_DBM::PLUS_10:
            l_Pa = 0x07;
            break;

        default:
            l_Pa = 0x04;
            break;
    }

    spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS::FREND0, l_Pa);
}

/**
 *   Set the channel number.
 *   params: 
 *       p_byChannelNumber:   channel number. maximum channel depends on the channel spacing 
 *   return:
 *       NONE       
 */
void CCC1100::setChannel(byte p_byChannelNumber) {
    //stores the new channel # in the CC1100
    spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS::CHANNR, p_byChannelNumber);   
}

/**
 *   Set the ISM band (frequency)
 *   params: 
 *       p_enumISMBand:   ISM Band value
 *   return:
 *       NONE       
 */
void CCC1100::setISMBand(ENM_ISM_BAND p_enumISMBand) {
    byte l_byFreq2, l_byFreq1, l_byFreq0;

    //loads the RF freq which is defined in cc1100_freq_select
    switch (p_enumISMBand)                                                       
    {

        //315MHz
        case ENM_ISM_BAND::ISM_315:                                                          
                    l_byFreq2 = 0x0C;
                    l_byFreq1 = 0x1D;
                    l_byFreq0 = 0x89;
                    memcpy(&m_unPayloadTXFIFOBuffer.byArray[0], (byte *)patable_power_315, 8);
                    break;

        //433.92MHz
        case ENM_ISM_BAND::ISM_433:                                                          
                    l_byFreq2 = 0x10;
                    l_byFreq1 = 0xB0;
                    l_byFreq0 = 0x71;
                    memcpy(&m_unPayloadTXFIFOBuffer.byArray[0], (byte *)patable_power_433, 8);
                    break;
        //868.3MHz
        case ENM_ISM_BAND::ISM_868:                                                          
                    l_byFreq2 = 0x21;
                    l_byFreq1 = 0x65;
                    l_byFreq0 = 0x6A;
                    memcpy(&m_unPayloadTXFIFOBuffer.byArray[0], (byte *)patable_power_868, 8);
                    break;

        //915MHz
        case ENM_ISM_BAND::ISM_915:                                                          
                    l_byFreq2 = 0x23;
                    l_byFreq1 = 0x31;
                    l_byFreq0 = 0x3B;
                    memcpy(&m_unPayloadTXFIFOBuffer.byArray[0], (byte *)patable_power_915, 8);
                    break;

        //default is 868.3MHz
        default:                                                             
                    l_byFreq2 = 0x21;
                    l_byFreq1 = 0x65;
                    l_byFreq0 = 0x6A;
                    memcpy(&m_unPayloadTXFIFOBuffer.byArray[0], (byte *)patable_power_868, 8);
                    break;
    }

    spiWriteBurst(ENM_CC1101_WRITE_BURST_COMMANDS::WRITE_PATABLE_ARRAY, &m_unPayloadTXFIFOBuffer.byArray[0], 8);

    //stores the new freq setting for defined ISM band
    spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS::FREQ2, l_byFreq2);                                         
    spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS::FREQ1, l_byFreq1);
    spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS::FREQ0, l_byFreq0);

}

/**
 *   Set Baud Rate including modulation mode 
 *   params: 
 *       p_enumBaudRateModulation:   Baud-rate and modulation value
 *   return:
 *       NONE       
 */
void CCC1100::setBaudeRateAndModulation(ENM_BAUD_RATE_MODULATION p_enumBaudRateModulation) {

    byte *l_pCFGRegister;
    
    switch (p_enumBaudRateModulation)
    {
        case ENM_BAUD_RATE_MODULATION::GFSK_1_2_kb:
            l_pCFGRegister = (byte *)cc1100_GFSK_1_2_kb;
            break;

        case ENM_BAUD_RATE_MODULATION::GFSK_38_4_kb:
            l_pCFGRegister = (byte *)cc1100_GFSK_38_4_kb;
            break;

        case ENM_BAUD_RATE_MODULATION::GFSK_100_kb:
            l_pCFGRegister = (byte *)cc1100_GFSK_100_kb;
            break;

        case ENM_BAUD_RATE_MODULATION::MSK_250_kb:
            l_pCFGRegister = (byte *)cc1100_MSK_250_kb;
            break;

        case ENM_BAUD_RATE_MODULATION::MSK_500_kb:
            l_pCFGRegister = (byte *)cc1100_MSK_500_kb;
            break;

        case ENM_BAUD_RATE_MODULATION::OOK_4_8_kb:
            l_pCFGRegister = (byte *)cc1100_OOK_4_8_kb;
            break;

        default:
            l_pCFGRegister = (byte *)cc1100_GFSK_100_kb;
            break;
    }

    memcpy(&m_unPayloadTXFIFOBuffer.byArray[0], l_pCFGRegister, CFG_REGISTER_SIZE);
    spiWriteBurst(ENM_CC1101_WRITE_BURST_COMMANDS::WRITE_ARRAY, &m_unPayloadTXFIFOBuffer.byArray[0], CFG_REGISTER_SIZE);

    //store value of IOCFG2 ([0]) register used later when an interrupt occurs
    m_byRegisterIOCFG2Settings = *l_pCFGRegister;
}
/**
 *   Read an single byte from a register 
 *   params: 
 *       p_byRegister:   byte register.
 *   return:
 *       byte readed from register       
 */
byte CCC1100::spiReadRegister(ENM_CC1101_READ_WRITE_REGISTERS p_enumRegister) {
    byte l_byDataAray[2];
    l_byDataAray[0] = p_enumRegister;

    spiTransaction(l_byDataAray, 2);

    return l_byDataAray[1];
}

/**
 *   Read an single byte from a register 
 *   params: 
 *       p_byRegister:   byte register.
 *   return:
 *       byte readed from register       
 */
byte CCC1100::spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS p_enumRegister) {
    byte l_byDataAray[2];
    l_byDataAray[0] = p_enumRegister;

    spiTransaction(l_byDataAray, 2);

    return l_byDataAray[1];
}

/**
 *   Write an single byte to a register 
 *   params: 
 *       p_enumRegister: byte register.
 *       p_byData:       byte to write to the register
 *   return:
 *       NONE      
 */
void CCC1100::spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS p_enumRegister, byte p_byData) {
    byte l_byDataAray[2];
    l_byDataAray[0] = p_enumRegister;
    l_byDataAray[1] = p_byData;

    spiTransaction(l_byDataAray, 2);
}

/**
 *   Read an array of bytes from a register
 *   params: 
 *       p_enumBurstCOmmande:    byte register.
 *       p_pbyDataArray:         bytes' array filled with data readed from the register
 *       p_byLengthToRead:       length of bytes to read
 *   return:
 *       NONE       
 */
void CCC1100::spiReadBurst(ENM_CC1101_READ_BURST_COMMANDS p_enumBurstCommande, byte *p_pbyDataArray, byte p_byLengthToRead) {
    *p_pbyDataArray = p_enumBurstCommande;
    spiTransaction(p_pbyDataArray, p_byLengthToRead + 1);
    memmove(p_pbyDataArray, p_pbyDataArray+1, p_byLengthToRead);
}

/**
 *   Write an array of bytes to a register
 *   params: 
 *       p_enumBustCommand:  burst write command.
 *       p_pbyDataArray:     bytes' array to write to the register
 *       p_byLengthToWrite:  length of bytes' array to write
 *   return:
 *       NONE      
 */
void CCC1100::spiWriteBurst(ENM_CC1101_WRITE_BURST_COMMANDS p_enumBustCommand, byte *p_pbyDataArray, byte p_byLengthToWrite) {
    memmove(&m_unPayloadTXFIFOBuffer.byArray[1], &m_unPayloadTXFIFOBuffer.byArray[0], p_byLengthToWrite + 1);
    m_unPayloadTXFIFOBuffer.byArray[0] = p_enumBustCommand;
    spiTransaction(&m_unPayloadTXFIFOBuffer.byArray[0], p_byLengthToWrite + 2);
}


/**
 *   Write a single byte instruction to the device
 *   params: 
 *       p_enumStrobeCommand:  the byte instruction to write.
 *   return:
 *       NONE      
 */
void CCC1100::spiWriteStrobe(ENM_CC1101_STROBE_COMMANDS p_enumStrobeCommand) {
    byte l_byDataAray[1];
    l_byDataAray[0] = p_enumStrobeCommand;

    spiTransaction(l_byDataAray, 1);
}

/**
 *   Perform SPI transaction, as much as for Read and Write
 *   params: 
 *       p_pbyData:  pointer to the bytes array containing the data to write.
 *                   If the transaction expects some incoming data (read), then 
 *                   the bytes array is refilled with these data. First byte ([0])
 *                   contains the length of incoming data strating at [1]
 *       p_byLength: length of data (bytes) to write
 *   return:
 *       NONE (readed data are returned into p_pbyData)      
 */
void CCC1100::spiTransaction(byte *p_pbyData, byte p_byLength) {
    SPI.beginTransaction (SPISettings (SPI_CLOCK, SPI_DATA_ORDER, SPI_MODE));  
    digitalWrite(PIN_CC1100_CS, LOW);
    delayMicroseconds(10);
    SPI.transfer(p_pbyData, p_byLength);
    delayMicroseconds(10);
    digitalWrite(PIN_CC1100_CS, HIGH);
    SPI.endTransaction ();  
}
