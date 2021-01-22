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
#ifndef _CCC1100_H_
#define _CCC1100_H_

#include "global.h"
#include "CCircularBuffer.h"
#include <SPI.h>

#include "logging.h"

#define PIN_CC1100_CS                       2   
#define PIN_CC1100_GD02                     3 

#define SPI_CLOCK                           4000000
#define SPI_DATA_ORDER                      MSBFIRST
#define SPI_MODE                            SPI_MODE0

#define MAX_RADIO_MESSAGE_DATA_LENGTH       32
#define MAX_RADIO_MESSAGE_LENGTH            sizeof(CCC1100::STRUCT_RADIO_PAYLOAD_HEADER) + sizeof(CCC1100::STRUCT_RADIO_PAYLOAD_MESSAGE)

//----------------------[CC1100 - misc]---------------------------------------
#define CRYSTAL_FREQUENCY                   26000000
#define CFG_REGISTER_SIZE                   0x2F  //47 registers
#define FIFO_BUFFER_SIZE                    0x42  //size of Fifo Buffer
#define RSSI_OFFSET_868MHZ                  0x4E  //dec = 74
#define TX_RETRIES_MAX                      0x05  //tx_retries_max
#define ACK_TIMEOUT                         200  //ACK timeout in ms
#define CC1100_COMPARE_REGISTER             0x00  //register compare 0=no compare 1=compare
#define BROADCAST_ADDRESS                   0x00  //broadcast address
#define CC1100_FREQ_315MHZ                  0x01
#define CC1100_FREQ_434MHZ                  0x02
#define CC1100_FREQ_868MHZ                  0x03
#define CC1100_FREQ_915MHZ                  0x04
//#define CC1100_FREQ_2430MHZ               0x05
#define CC1100_TEMP_ADC_MV                  3.225 //3.3V/1023 . mV pro digit
#define CC1100_TEMP_CELS_CO                 2.47  //Temperature coefficient 2.47mV per Grad Celsius

//-------------------[global EEPROM default settings 868 Mhz]-------------------
const byte cc1100_GFSK_1_2_kb[CFG_REGISTER_SIZE] PROGMEM = {
                    0x07,  // IOCFG2        GDO2 Output Pin Configuration
                    0x2E,  // IOCFG1        GDO1 Output Pin Configuration
                    0x80,  // IOCFG0        GDO0 Output Pin Configuration
                    0x07,  // FIFOTHR       RX FIFO and TX FIFO Thresholds
                    0x57,  // SYNC1         Sync Word, High Byte
                    0x43,  // SYNC0         Sync Word, Low Byte
                    0x3E,  // PKTLEN        Packet Length
                    0x0E,  // PKTCTRL1      Packet Automation Control
                    0x45,  // PKTCTRL0      Packet Automation Control
                    0xFF,  // ADDR          Device Address
                    0x00,  // CHANNR        Channel Number
                    0x08,  // FSCTRL1       Frequency Synthesizer Control
                    0x00,  // FSCTRL0       Frequency Synthesizer Control
                    0x21,  // FREQ2         Frequency Control Word, High Byte
                    0x65,  // FREQ1         Frequency Control Word, Middle Byte
                    0x6A,  // FREQ0         Frequency Control Word, Low Byte
                    0xF5,  // MDMCFG4       Modem Configuration
                    0x83,  // MDMCFG3       Modem Configuration
                    0x13,  // MDMCFG2       Modem Configuration
                    0xA0,  // MDMCFG1       Modem Configuration
                    0xF8,  // MDMCFG0       Modem Configuration
                    0x15,  // DEVIATN       Modem Deviation Setting
                    0x07,  // MCSM2         Main Radio Control State Machine Configuration
                    0x0C,  // MCSM1         Main Radio Control State Machine Configuration
                    0x18,  // MCSM0         Main Radio Control State Machine Configuration
                    0x16,  // FOCCFG        Frequency Offset Compensation Configuration
                    0x6C,  // BSCFG         Bit Synchronization Configuration
                    0x03,  // AGCCTRL2      AGC Control
                    0x40,  // AGCCTRL1      AGC Control
                    0x91,  // AGCCTRL0      AGC Control
                    0x02,  // WOREVT1       High Byte Event0 Timeout
                    0x26,  // WOREVT0       Low Byte Event0 Timeout
                    0x09,  // WORCTRL       Wake On Radio Control
                    0x56,  // FREND1        Front End RX Configuration
                    0x17,  // FREND0        Front End TX Configuration
                    0xA9,  // FSCAL3        Frequency Synthesizer Calibration
                    0x0A,  // FSCAL2        Frequency Synthesizer Calibration
                    0x00,  // FSCAL1        Frequency Synthesizer Calibration
                    0x11,  // FSCAL0        Frequency Synthesizer Calibration
                    0x41,  // RCCTRL1       RC Oscillator Configuration
                    0x00,  // RCCTRL0       RC Oscillator Configuration
                    0x59,  // FSTEST        Frequency Synthesizer Calibration Control,
                    0x7F,  // PTEST         Production Test
                    0x3F,  // AGCTEST       AGC Test
                    0x81,  // TEST2         Various Test Settings
                    0x3F,  // TEST1         Various Test Settings
                    0x0B   // TEST0         Various Test Settings
                };

const byte cc1100_GFSK_38_4_kb[CFG_REGISTER_SIZE] PROGMEM = {
                    0x07,  // IOCFG2        GDO2 Output Pin Configuration
                    0x2E,  // IOCFG1        GDO1 Output Pin Configuration
                    0x80,  // IOCFG0        GDO0 Output Pin Configuration
                    0x07,  // FIFOTHR       RX FIFO and TX FIFO Thresholds
                    0x57,  // SYNC1         Sync Word, High Byte
                    0x43,  // SYNC0         Sync Word, Low Byte
                    0x3E,  // PKTLEN        Packet Length
                    0x0E,  // PKTCTRL1      Packet Automation Control
                    0x45,  // PKTCTRL0      Packet Automation Control
                    0xFF,  // ADDR          Device Address
                    0x00,  // CHANNR        Channel Number
                    0x06,  // FSCTRL1       Frequency Synthesizer Control
                    0x00,  // FSCTRL0       Frequency Synthesizer Control
                    0x21,  // FREQ2         Frequency Control Word, High Byte
                    0x65,  // FREQ1         Frequency Control Word, Middle Byte
                    0x6A,  // FREQ0         Frequency Control Word, Low Byte
                    0xCA,  // MDMCFG4       Modem Configuration
                    0x83,  // MDMCFG3       Modem Configuration
                    0x13,  // MDMCFG2       Modem Configuration
                    0xA0,  // MDMCFG1       Modem Configuration
                    0xF8,  // MDMCFG0       Modem Configuration
                    0x34,  // DEVIATN       Modem Deviation Setting
                    0x07,  // MCSM2         Main Radio Control State Machine Configuration
                    0x0C,  // MCSM1         Main Radio Control State Machine Configuration
                    0x18,  // MCSM0         Main Radio Control State Machine Configuration
                    0x16,  // FOCCFG        Frequency Offset Compensation Configuration
                    0x6C,  // BSCFG         Bit Synchronization Configuration
                    0x43,  // AGCCTRL2      AGC Control
                    0x40,  // AGCCTRL1      AGC Control
                    0x91,  // AGCCTRL0      AGC Control
                    0x02,  // WOREVT1       High Byte Event0 Timeout
                    0x26,  // WOREVT0       Low Byte Event0 Timeout
                    0x09,  // WORCTRL       Wake On Radio Control
                    0x56,  // FREND1        Front End RX Configuration
                    0x17,  // FREND0        Front End TX Configuration
                    0xA9,  // FSCAL3        Frequency Synthesizer Calibration
                    0x0A,  // FSCAL2        Frequency Synthesizer Calibration
                    0x00,  // FSCAL1        Frequency Synthesizer Calibration
                    0x11,  // FSCAL0        Frequency Synthesizer Calibration
                    0x41,  // RCCTRL1       RC Oscillator Configuration
                    0x00,  // RCCTRL0       RC Oscillator Configuration
                    0x59,  // FSTEST        Frequency Synthesizer Calibration Control,
                    0x7F,  // PTEST         Production Test
                    0x3F,  // AGCTEST       AGC Test
                    0x81,  // TEST2         Various Test Settings
                    0x3F,  // TEST1         Various Test Settings
                    0x0B   // TEST0         Various Test Settings
                };

const byte cc1100_GFSK_100_kb[CFG_REGISTER_SIZE] PROGMEM = {
                    0x07,  // IOCFG2        GDO2 Output Pin Configuration
                    0x2E,  // IOCFG1        GDO1 Output Pin Configuration
                    0x80,  // IOCFG0        GDO0 Output Pin Configuration
                    0x07,  // FIFOTHR       RX FIFO and TX FIFO Thresholds
                    0x57,  // SYNC1         Sync Word, High Byte
                    0x43,  // SYNC0         Sync Word, Low Byte
                    0x3E,  // PKTLEN        Packet Length
                    0x0F,  // PKTCTRL1      Packet Automation Control
                    0x45,  // PKTCTRL0      Packet Automation Control
                    0xFF,  // ADDR          Device Address
                    0x00,  // CHANNR        Channel Number
                    0x08,  // FSCTRL1       Frequency Synthesizer Control
                    0x00,  // FSCTRL0       Frequency Synthesizer Control
                    0x21,  // FREQ2         Frequency Control Word, High Byte
                    0x65,  // FREQ1         Frequency Control Word, Middle Byte
                    0x6A,  // FREQ0         Frequency Control Word, Low Byte
                    0x5B,  // MDMCFG4       Modem Configuration
                    0xF8,  // MDMCFG3       Modem Configuration
                    0x13,  // MDMCFG2       Modem Configuration
                    0xA0,  // MDMCFG1       Modem Configuration
                    0xF8,  // MDMCFG0       Modem Configuration
                    0x47,  // DEVIATN       Modem Deviation Setting
                    0x07,  // MCSM2         Main Radio Control State Machine Configuration
                    0x0C,  // MCSM1         Main Radio Control State Machine Configuration
                    0x18,  // MCSM0         Main Radio Control State Machine Configuration
                    0x1D,  // FOCCFG        Frequency Offset Compensation Configuration
                    0x1C,  // BSCFG         Bit Synchronization Configuration
                    0xC7,  // AGCCTRL2      AGC Control
                    0x00,  // AGCCTRL1      AGC Control
                    0xB2,  // AGCCTRL0      AGC Control
                    0x02,  // WOREVT1       High Byte Event0 Timeout
                    0x26,  // WOREVT0       Low Byte Event0 Timeout
                    0x09,  // WORCTRL       Wake On Radio Control
                    0xB6,  // FREND1        Front End RX Configuration
                    0x17,  // FREND0        Front End TX Configuration
                    0xEA,  // FSCAL3        Frequency Synthesizer Calibration
                    0x0A,  // FSCAL2        Frequency Synthesizer Calibration
                    0x00,  // FSCAL1        Frequency Synthesizer Calibration
                    0x11,  // FSCAL0        Frequency Synthesizer Calibration
                    0x41,  // RCCTRL1       RC Oscillator Configuration
                    0x00,  // RCCTRL0       RC Oscillator Configuration
                    0x59,  // FSTEST        Frequency Synthesizer Calibration Control,
                    0x7F,  // PTEST         Production Test
                    0x3F,  // AGCTEST       AGC Test
                    0x81,  // TEST2         Various Test Settings
                    0x3F,  // TEST1         Various Test Settings
                    0x0B   // TEST0         Various Test Settings
                };

const byte cc1100_MSK_250_kb[CFG_REGISTER_SIZE] PROGMEM = {
                    0x07,  // IOCFG2        GDO2 Output Pin Configuration
                    0x2E,  // IOCFG1        GDO1 Output Pin Configuration
                    0x80,  // IOCFG0        GDO0 Output Pin Configuration
                    0x07,  // FIFOTHR       RX FIFO and TX FIFO Thresholds
                    0x57,  // SYNC1         Sync Word, High Byte
                    0x43,  // SYNC0         Sync Word, Low Byte
                    0x3E,  // PKTLEN        Packet Length
                    0x0E,  // PKTCTRL1      Packet Automation Control
                    0x45,  // PKTCTRL0      Packet Automation Control
                    0xFF,  // ADDR          Device Address
                    0x00,  // CHANNR        Channel Number
                    0x0B,  // FSCTRL1       Frequency Synthesizer Control
                    0x00,  // FSCTRL0       Frequency Synthesizer Control
                    0x21,  // FREQ2         Frequency Control Word, High Byte
                    0x65,  // FREQ1         Frequency Control Word, Middle Byte
                    0x6A,  // FREQ0         Frequency Control Word, Low Byte
                    0x2D,  // MDMCFG4       Modem Configuration
                    0x3B,  // MDMCFG3       Modem Configuration
                    0x73,  // MDMCFG2       Modem Configuration
                    0xA0,  // MDMCFG1       Modem Configuration
                    0xF8,  // MDMCFG0       Modem Configuration
                    0x00,  // DEVIATN       Modem Deviation Setting
                    0x07,  // MCSM2         Main Radio Control State Machine Configuration
                    0x0C,  // MCSM1         Main Radio Control State Machine Configuration
                    0x18,  // MCSM0         Main Radio Control State Machine Configuration
                    0x1D,  // FOCCFG        Frequency Offset Compensation Configuration
                    0x1C,  // BSCFG         Bit Synchronization Configuration
                    0xC7,  // AGCCTRL2      AGC Control
                    0x00,  // AGCCTRL1      AGC Control
                    0xB2,  // AGCCTRL0      AGC Control
                    0x02,  // WOREVT1       High Byte Event0 Timeout
                    0x26,  // WOREVT0       Low Byte Event0 Timeout
                    0x09,  // WORCTRL       Wake On Radio Control
                    0xB6,  // FREND1        Front End RX Configuration
                    0x17,  // FREND0        Front End TX Configuration
                    0xEA,  // FSCAL3        Frequency Synthesizer Calibration
                    0x0A,  // FSCAL2        Frequency Synthesizer Calibration
                    0x00,  // FSCAL1        Frequency Synthesizer Calibration
                    0x11,  // FSCAL0        Frequency Synthesizer Calibration
                    0x41,  // RCCTRL1       RC Oscillator Configuration
                    0x00,  // RCCTRL0       RC Oscillator Configuration
                    0x59,  // FSTEST        Frequency Synthesizer Calibration Control,
                    0x7F,  // PTEST         Production Test
                    0x3F,  // AGCTEST       AGC Test
                    0x81,  // TEST2         Various Test Settings
                    0x3F,  // TEST1         Various Test Settings
                    0x0B   // TEST0         Various Test Settings
                };

const byte cc1100_MSK_500_kb[CFG_REGISTER_SIZE] PROGMEM = {
                    0x07,  // IOCFG2        GDO2 Output Pin Configuration
                    0x2E,  // IOCFG1        GDO1 Output Pin Configuration
                    0x80,  // IOCFG0        GDO0 Output Pin Configuration
                    0x07,  // FIFOTHR       RX FIFO and TX FIFO Thresholds
                    0x57,  // SYNC1         Sync Word, High Byte
                    0x43,  // SYNC0         Sync Word, Low Byte
                    0x3E,  // PKTLEN        Packet Length
                    0x0E,  // PKTCTRL1      Packet Automation Control
                    0x45,  // PKTCTRL0      Packet Automation Control
                    0xFF,  // ADDR          Device Address
                    0x00,  // CHANNR        Channel Number
                    0x0C,  // FSCTRL1       Frequency Synthesizer Control
                    0x00,  // FSCTRL0       Frequency Synthesizer Control
                    0x21,  // FREQ2         Frequency Control Word, High Byte
                    0x65,  // FREQ1         Frequency Control Word, Middle Byte
                    0x6A,  // FREQ0         Frequency Control Word, Low Byte
                    0x0E,  // MDMCFG4       Modem Configuration
                    0x3B,  // MDMCFG3       Modem Configuration
                    0x73,  // MDMCFG2       Modem Configuration
                    0xA0,  // MDMCFG1       Modem Configuration
                    0xF8,  // MDMCFG0       Modem Configuration
                    0x00,  // DEVIATN       Modem Deviation Setting
                    0x07,  // MCSM2         Main Radio Control State Machine Configuration
                    0x0C,  // MCSM1         Main Radio Control State Machine Configuration
                    0x18,  // MCSM0         Main Radio Control State Machine Configuration
                    0x1D,  // FOCCFG        Frequency Offset Compensation Configuration
                    0x1C,  // BSCFG         Bit Synchronization Configuration
                    0xC7,  // AGCCTRL2      AGC Control
                    0x40,  // AGCCTRL1      AGC Control
                    0xB2,  // AGCCTRL0      AGC Control
                    0x02,  // WOREVT1       High Byte Event0 Timeout
                    0x26,  // WOREVT0       Low Byte Event0 Timeout
                    0x09,  // WORCTRL       Wake On Radio Control
                    0xB6,  // FREND1        Front End RX Configuration
                    0x17,  // FREND0        Front End TX Configuration
                    0xEA,  // FSCAL3        Frequency Synthesizer Calibration
                    0x0A,  // FSCAL2        Frequency Synthesizer Calibration
                    0x00,  // FSCAL1        Frequency Synthesizer Calibration
                    0x19,  // FSCAL0        Frequency Synthesizer Calibration
                    0x41,  // RCCTRL1       RC Oscillator Configuration
                    0x00,  // RCCTRL0       RC Oscillator Configuration
                    0x59,  // FSTEST        Frequency Synthesizer Calibration Control,
                    0x7F,  // PTEST         Production Test
                    0x3F,  // AGCTEST       AGC Test
                    0x81,  // TEST2         Various Test Settings
                    0x3F,  // TEST1         Various Test Settings
                    0x0B   // TEST0         Various Test Settings
                };

const byte cc1100_OOK_4_8_kb[CFG_REGISTER_SIZE] PROGMEM = {
                    0x06,  // IOCFG2        GDO2 Output Pin Configuration
                    0x2E,  // IOCFG1        GDO1 Output Pin Configuration
                    0x06,  // IOCFG0        GDO0 Output Pin Configuration
                    0x47,  // FIFOTHR       RX FIFO and TX FIFO Thresholds
                    0x57,  // SYNC1         Sync Word, High Byte
                    0x43,  // SYNC0         Sync Word, Low Byte
                    0xFF,  // PKTLEN        Packet Length
                    0x04,  // PKTCTRL1      Packet Automation Control
                    0x05,  // PKTCTRL0      Packet Automation Control
                    0x00,  // ADDR          Device Address
                    0x00,  // CHANNR        Channel Number
                    0x06,  // FSCTRL1       Frequency Synthesizer Control
                    0x00,  // FSCTRL0       Frequency Synthesizer Control
                    0x21,  // FREQ2         Frequency Control Word, High Byte
                    0x65,  // FREQ1         Frequency Control Word, Middle Byte
                    0x6A,  // FREQ0         Frequency Control Word, Low Byte
                    0x87,  // MDMCFG4       Modem Configuration
                    0x83,  // MDMCFG3       Modem Configuration
                    0x3B,  // MDMCFG2       Modem Configuration
                    0x22,  // MDMCFG1       Modem Configuration
                    0xF8,  // MDMCFG0       Modem Configuration
                    0x15,  // DEVIATN       Modem Deviation Setting
                    0x07,  // MCSM2         Main Radio Control State Machine Configuration
                    0x30,  // MCSM1         Main Radio Control State Machine Configuration
                    0x18,  // MCSM0         Main Radio Control State Machine Configuration
                    0x14,  // FOCCFG        Frequency Offset Compensation Configuration
                    0x6C,  // BSCFG         Bit Synchronization Configuration
                    0x07,  // AGCCTRL2      AGC Control
                    0x00,  // AGCCTRL1      AGC Control
                    0x92,  // AGCCTRL0      AGC Control
                    0x87,  // WOREVT1       High Byte Event0 Timeout
                    0x6B,  // WOREVT0       Low Byte Event0 Timeout
                    0xFB,  // WORCTRL       Wake On Radio Control
                    0x56,  // FREND1        Front End RX Configuration
                    0x17,  // FREND0        Front End TX Configuration
                    0xE9,  // FSCAL3        Frequency Synthesizer Calibration
                    0x2A,  // FSCAL2        Frequency Synthesizer Calibration
                    0x00,  // FSCAL1        Frequency Synthesizer Calibration
                    0x1F,  // FSCAL0        Frequency Synthesizer Calibration
                    0x41,  // RCCTRL1       RC Oscillator Configuration
                    0x00,  // RCCTRL0       RC Oscillator Configuration
                    0x59,  // FSTEST        Frequency Synthesizer Calibration Control
                    0x7F,  // PTEST         Production Test
                    0x3F,  // AGCTEST       AGC Test
                    0x81,  // TEST2         Various Test Settings
                    0x35,  // TEST1         Various Test Settings
                    0x09,  // TEST0         Various Test Settings
                };

//Patable index:                          -30  -20- -15  -10   0    5    7    10 dBm
const byte patable_power_315[] PROGMEM = {0x17,0x1D,0x26,0x69,0x51,0x86,0xCC,0xC3};
const byte patable_power_433[] PROGMEM = {0x6C,0x1C,0x06,0x3A,0x51,0x85,0xC8,0xC0};
const byte patable_power_868[] PROGMEM = {0x03,0x17,0x1D,0x26,0x50,0x86,0xCD,0xC0};
const byte patable_power_915[] PROGMEM = {0x0B,0x1B,0x6D,0x67,0x50,0x85,0xC9,0xC1};

//----------------------------------[END]---------------------------------------

class CCC1100 {
public:
    enum ENM_ISM_BAND {ISM_315=1, ISM_433, ISM_868, ISM_915};
    enum ENM_OUTPUT_POWER_DBM {MINUS_30=1, MINUS_20, MINUS_15, MINUS_10, PLUS_0, PLUS_5, PLUS_7, PLUS_10};
    enum ENM_BAUD_RATE_MODULATION {GFSK_1_2_kb=1, GFSK_38_4_kb, GFSK_100_kb, MSK_250_kb, MSK_500_kb, OOK_4_8_kb};
    enum ENM_PAYLOAD_TYPE {ACK=0x00, MSG=0x01};

    struct STRUCT_RADIO_PAYLOAD_HEADER {
        byte                byPayloadLength;
        byte                byRecipientAddr;
        byte                bySenderAddr;
        word                wMessageToken;
        byte                byPayloadType;  
    } __attribute__ ((packed));     //non aligment pragma

    struct STRUCT_RADIO_PAYLOAD_MESSAGE {
        byte                byMessageType;
        byte                byDataLength;
        byte                abyData[MAX_RADIO_MESSAGE_DATA_LENGTH];
    } __attribute__ ((packed));     //non aligment pragma

    struct STRUCT_RADIO_PAYLOAD {
        STRUCT_RADIO_PAYLOAD_HEADER     strctPayLoadHeader;
        STRUCT_RADIO_PAYLOAD_MESSAGE    strctPayloadMessage;
        byte                            byRSSI;
        byte                            byLQI;
        byte                            _dummy[FIFO_BUFFER_SIZE - MAX_RADIO_MESSAGE_LENGTH - 2];        
    } __attribute__ ((packed));     //non aligment pragma

    union UNION_PAYLOAD {
        byte                    byArray[FIFO_BUFFER_SIZE];
        STRUCT_RADIO_PAYLOAD    strctPayLoad;
    };
    
    boolean postMessage(byte p_pyRecipientAddr, STRUCT_RADIO_PAYLOAD_MESSAGE *p_pstrRadioPayloadMessage, byte p_byTXRetryMax);
    int16_t getMessage(STRUCT_RADIO_PAYLOAD_MESSAGE *p_pstrRadioPayloadMessage);
    byte getVersion();
    byte getPartNumber();
    void interruptHandler();
    boolean poll();
    boolean init(byte p_byDeviceAdd, ENM_OUTPUT_POWER_DBM p_byOutputPowerLevel, uint16_t p_uiMsgSignature);
    
private:
    enum ENM_CC1101_MARCSTATES {SLEEP=0x00, IDLE, XOFF, VCOON_LC, REGON_MC, MANCAL, VCOON, REGON, STARTCAL, BWBOOST, FS_LOCK, IFADCON, ENDCAL, 
                                RX, RX_END, RX_RST, TXRX_SWITCH, RXFIFO_OVERFLOW, FSTXON, TX, TX_END, RXTX_SWITXH, TXFIFO_UNDERFLOW};
    
    enum ENM_CC1101_READ_BURST_COMMANDS {
        READ_SINGLE=0x80            /*read 1 byte*/,
        READ_ARRAY=0xC0             /*read byte array*/,
        RXFIFO_SINGLE=0xBF          /*FIFO read 1 byte*/,
        RXFIFO_ARRAY= 0xFF          /*FIFO read byte arrayy*/,
        READ_PATABLE_SINGLE=0xFE    /*power control read 1 byte*/,
        READ_PATABLE_ARRAY=0x7E     /*power control read byte array*/,
    };
    
    enum ENM_CC1101_WRITE_BURST_COMMANDS {
        WRITE_SINGLE=0x00,          /*write 1 byte*/
        WRITE_ARRAY=0x40            /*write byte array*/,
        TXFIFO_SINGLE=0x3F          /*FIFO write 1 byte*/,
        TXFIFO_ARRAY=0x7F           /*FIFO write byte array*/,
        WRITE_PATABLE_SINGLE=0xFE   /*power control write 1 byte*/,
        WRITE_PATABLE_ARRAY=0x7E    /*power control write byte array*/,
    };

    enum ENM_CC1101_STROBE_COMMANDS {
        SRES=0x30       /*Reset chip*/, 
        SFSTXON         /*Enable/calibrate freq synthesizer*/, 
        SXOFF           /*Turn off crystal oscillator*/, 
        SCAL            /*Calibrate freq synthesizer & disable*/, 
        SRX             /*Enable RX*/, 
        STX             /*Enable TX*/, 
        SIDLE           /*Exit RX / TX*/, 
        SAFC            /*AFC adjustment of freq synthesizer*/, 
        SWOR            /*Start automatic RX polling sequence*/, 
        SPWD            /*Enter pwr down mode when CSn goes hi*/, 
        SFRX            /*Flush the RX FIFO buffer*/, 
        SFTX            /*Flush the TX FIFO buffer*/, 
        SWORRST         /*Reset real time clock*/, 
        SNOP            /*No operation*/
    };

    
    enum ENM_CC1101_READ_WRITE_REGISTERS {
        IOCFG2 = 0x00   /*GDO2 output pin configuration*/,
        IOCFG1          /*GDO1 output pin configuration*/,
        IOCFG0          /*GDO0 output pin configuration*/,
        FIFOTHR         /*RX FIFO and TX FIFO thresholds*/,
        SYNC1           /*Sync word, high byte*/,
        SYNC0           /*Sync word, low byte*/,
        PKTLEN          /*Packet length*/,
        PKTCTRL1        /*Packet automation control*/,
        PKTCTRL0        /*Packet automation control*/,
        ADDR            /*Device address*/,
        CHANNR          /*Channel number*/,
        FSCTRL1         /*Frequency synthesizer control*/,
        FSCTRL0         /*Frequency synthesizer control*/,
        FREQ2           /*Frequency control word, high byte*/,
        FREQ1           /*Frequency control word, middle byte*/,
        FREQ0           /*Frequency control word, low byte*/,
        MDMCFG4         /*Modem configuration*/,
        MDMCFG3         /*Modem configuration*/,
        MDMCFG2         /*Modem configuration*/,
        MDMCFG1         /*Modem configuration*/,
        MDMCFG0         /*Modem configuration*/,
        DEVIATN         /*Modem deviation setting*/,
        MCSM2           /*Main Radio Cntrl State Machine config*/,
        MCSM1           /*Main Radio Cntrl State Machine config*/,
        MCSM0           /*Main Radio Cntrl State Machine config*/,
        FOCCFG          /*Frequency Offset Compensation config*/,
        BSCFG           /*Bit Synchronization configuration*/,
        AGCCTRL2        /*AGC control*/,
        AGCCTRL1        /*AGC control*/,
        AGCCTRL0        /*AGC control*/,
        WOREVT1         /*High byte Event 0 timeout*/,
        WOREVT0         /*Low byte Event 0 timeout*/,
        WORCTRL         /*Wake On Radio control*/,
        FREND1          /*Front end RX configuration*/,
        FREND0          /*Front end TX configuration*/,
        FSCAL3          /*Frequency synthesizer calibration*/,
        FSCAL2          /*Frequency synthesizer calibration*/,
        FSCAL1          /*Frequency synthesizer calibration*/,
        FSCAL0          /*Frequency synthesizer calibration*/,
        RCCTRL1         /*RC oscillator configuration*/,
        RCCTRL0         /*RC oscillator configuration*/,
        FSTEST          /*Frequency synthesizer cal control*/,
        PTEST           /*Production test*/,
        AGCTEST         /*AGC test*/,
        TEST2           /*Various test settings*/,
        TEST1           /*Various test settings*/,
        TEST0           /*Various test settings*/
    };

    enum ENM_CC1101_READ_ONLY_REGISTERS {
        PARTNUM=0xF0    /*Part number*/,
        VERSION         /*Current version number*/,
        FREQEST         /*Frequency offset estimate*/,
        LQI             /*Demodulator estimate for link quality*/,
        RSSI            /*Received signal strength indication*/,
        MARCSTATE       /*Control state machine state*/,
        WORTIME1        /*High byte of WOR timer*/,
        WORTIME0        /*Low byte of WOR timer*/,
        PKTSTATUS       /*Current GDOx status and packet status*/,
        VCO_VC_DAC      /*Current setting from PLL cal module*/,
        TXBYTES         /*Underflow and # of bytes in TXFIFO*/,
        RXBYTES         /*Overflow and # of bytes in RXFIFO*/,
        RCCTRL1_STATUS  /*Last RC Oscillator Calibration Result*/,
        RCCTRL0_STATUS  /*Last RC Oscillator Calibration Result*/
    };


    byte m_byDeviceAddr;

    uint16_t m_uiMessageSignature;
    
    UNION_PAYLOAD   m_unPayloadRXFIFOBuffer;
    UNION_PAYLOAD   m_unPayloadTXFIFOBuffer; 

    byte m_byRegisterIOCFG2Settings;
    boolean m_bReceivedAck = false;
    CCircularBuffer m_RXCircularBuffer;
    volatile boolean m_bReceiveIT = false;

    boolean getPayload();
     void TXPayloadBurst(UNION_PAYLOAD *p_pstrctunionPayload);
    boolean checkAcknowledge(byte p_byRecipientAddr, byte p_bySenderAddr, byte p_byType);
    void sendAcknowledge(byte p_byRecipientddr);
    int8_t RXPayloadBurst();
    void setTransmitMode();
    void spiTransaction(byte *p_pbyData, byte p_byLength);
    void spiWriteStrobe(ENM_CC1101_STROBE_COMMANDS p_enumStrobeCommand);
    void spiWriteBurst(ENM_CC1101_WRITE_BURST_COMMANDS p_enumBustCommand, byte *p_pbyDataArray, byte p_byLengthToWrite);
    void spiReadBurst(ENM_CC1101_READ_BURST_COMMANDS p_enumBurstCOmmande, byte *p_pbyDataArray, byte p_byLengthToRead);
    void spiWriteRegister(ENM_CC1101_READ_WRITE_REGISTERS p_enumRegister, byte p_byData);
    byte spiReadRegister(ENM_CC1101_READ_WRITE_REGISTERS p_enumRegister);
    byte spiReadRegister(ENM_CC1101_READ_ONLY_REGISTERS p_enumRegister);
    void setBaudeRateAndModulation(ENM_BAUD_RATE_MODULATION p_enumBaudRateModulation);
    void setISMBand(ENM_ISM_BAND p_enumISMBand);
    void setChannel(byte p_byChannelNumber);
    void setOutputPowerLevel(ENM_OUTPUT_POWER_DBM p_enumDBPowerLever);
    void setDeviceAddr(byte p_byAddr);
    void sidle();
    void setReceiveMode();
    boolean checkUnitPayload(UNION_PAYLOAD *p_puinionPayload);
};

#endif