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
#include "CFlashLed.h"

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
*   Simple init of the flash led
*   params: 
*       p_iPinNum:					Digital pin number
*		p_bPinLevelOn:				state of the digital pin when set to ON. false=LOW, true=HIGH
*		p_uiTickingTimerMS:			rate of the ticking timer which is going to drive the flashing (by calling update())
*   return:
*       NONE       
*/
void CFlashLed::init(int p_iPinNum, boolean p_bPinLevelOn, uint16_t p_uiTickingTimerMS) {
	init(p_iPinNum, p_bPinLevelOn, -1, -1, -1, -1, -1, p_uiTickingTimerMS);
}


/**
*   Init of the flash led and set the default built-in model. cf below
*   params: 
*       p_iPinNum:					Digital pin number
*		p_bPinLevelOn:				state of the digital pin when set to ON. false=LOW, true=HIGH
*		p_builtIn:					built-in model to use. cf FLASH_BUILTIN and FLASH_BUILTIN_ARRAY
*		p_uiTickingTimerMS:			rate of the ticking timer which is going to drive the flashing (by calling update())
*   return:
*       NONE       
*/
void CFlashLed::init(int p_iPinNum, boolean p_bPinLevelOn, FLASH_BUILTIN p_builtIn, uint16_t p_uiTickingTimerMS) {
	init(p_iPinNum, p_bPinLevelOn, FLASH_BUILTIN_ARRAY[(int)p_builtIn][0], FLASH_BUILTIN_ARRAY[(int)p_builtIn][1], FLASH_BUILTIN_ARRAY[(int)p_builtIn][2], 
									FLASH_BUILTIN_ARRAY[(int)p_builtIn][3], FLASH_BUILTIN_ARRAY[(int)p_builtIn][4], p_uiTickingTimerMS);
}

/**
*   Init of the flash led and set the default model. 
*	A flash led model is composed by:
*		- flash or pulse: 
*			.duration of state ON
*			.interval between 2 state ON, or in other words the duration of a state OFF
*			.count of flash
*		- salvo: the repeated time of the flash model above
*			.interval between 2 salvos
*			.count of salvos. -1 for endlessly repetive
*
*	example:
*		flash 200ms ON, 800ms OFF 3 times, endlessly repetive every 1500ms	
*		- p_iFlashPulseDurationMS = 200
*		- p_iFlashPulseIntervalMS = 800
*		- p_iFlashCounts = 3
*		- p_iSalvoIntervalMS = 1500
*		- p_iSalvoCounts = -1
*	
*   params: 
*       p_iPinNum:					Digital pin number
*		p_bPinLevelOn:				state of the digital pin when set to ON. false=LOW, true=HIGH
*		p_iFlashPulseDurationMS:	duration of state ON. cf above
*		p_iFlashPulseIntervalMS:	duration of state OFF. cf above
*		p_iFlashCounts:				count of flash. cf above
*		p_iSalvoIntervalMS:			interval between the next repetitive sequence. cf above
*		p_iSalvoCounts:				count repetitives sequences. cf above
*		p_uiTickingTimerMS:			rate of the ticking timer which is going to drive the flashing (by calling update())
*   return:
*       NONE       
*/
void CFlashLed::init(int p_iPinNum, boolean p_bPinLevelOn, int p_iFlashPulseIntervalMS, int p_iFlashPulseDurationMS, int p_iFlashCounts, 
					int p_iSalvoIntervalMS, int p_iSalvoCounts, uint16_t p_uiTickingTimerMS) {
	if (m_iPinUm == -1) {
		m_iPinUm = p_iPinNum;
		pinMode(m_iPinUm, OUTPUT);
	}

	m_uiTickingTimerMS = p_uiTickingTimerMS;
	m_bPinLevelOn = p_bPinLevelOn;

	digitalWrite(m_iPinUm, (m_bPinLevelOn == LOW) ? HIGH : LOW);

	updateModel(p_iFlashPulseIntervalMS, p_iFlashPulseDurationMS, p_iFlashCounts, p_iSalvoIntervalMS, p_iSalvoCounts);

	m_flashState = FLASH_STATE::stop_flashing;
}

/**
*   Update the model of the flash. Change the sequences timings and counts. cf above
*   params: 
*		p_builtIn:					built-in model to use. cf FLASH_BUILTIN and FLASH_BUILTIN_ARRAY
*   return:
*       NONE       
*/
void CFlashLed::updateModel( FLASH_BUILTIN p_builtIn) {
	updateModel(FLASH_BUILTIN_ARRAY[(int)p_builtIn][0], FLASH_BUILTIN_ARRAY[(int)p_builtIn][1], FLASH_BUILTIN_ARRAY[(int)p_builtIn][2], FLASH_BUILTIN_ARRAY[(int)p_builtIn][3], FLASH_BUILTIN_ARRAY[(int)p_builtIn][4]);
}

/**
*   Update the model of the flash. Change the sequences timings and counts. cf above
*   params: 
*		p_iFlashPulseDurationMS:	duration of state ON. cf above
*		p_iFlashPulseIntervalMS:	duration of state OFF. cf above
*		p_iFlashCounts:				count of flash. cf above
*		p_iSalvoIntervalMS:			interval between the next repetitive sequence. cf above
*		p_iSalvoCounts:				count repetitives sequences. cf above
*   return:
*       NONE       
*/
void CFlashLed::updateModel(int p_iFlashPulseIntervalMS, int p_iFlashPulseDurationMS, int p_iFlashCounts, int p_iSalvoIntervalMS, int p_iSalvoCounts) {
	
	if (m_iPinUm != -1) {
		digitalWrite(m_iPinUm, (m_bPinLevelOn == LOW) ? HIGH : LOW);

		m_iFlashPulseIntervalMS = p_iFlashPulseIntervalMS / m_uiTickingTimerMS;
		m_iFlashPulseDurationMS = p_iFlashPulseDurationMS / m_uiTickingTimerMS;
		m_iSalvoIntervalMS = p_iSalvoIntervalMS / m_uiTickingTimerMS;
		m_iFlashCounts = p_iFlashCounts;
		m_iSalvoCounts = p_iSalvoCounts;
	}
}

/**
*   Callback to pass to the timer ticking interrupt
*	IMPORTANT: timer rate shall fit with the pulse duration and interval. e.g. if duration between 2 pulses is 5ms 
*	but timer ticking is 100ms, it will not work ! 
*   params: 
*		NONE
*   return:
*       NONE       
*/
void CFlashLed::poll() {
	switch (m_flashState) {
		
	case FLASH_STATE::interval_counting:
		if (--m_iFlashPulseIntervalMSCounter <= 0) {
			m_iFlashPulseIntervalMSCounter = m_iFlashPulseIntervalMS;

			//switch on
			digitalWrite(m_iPinUm, (m_bPinLevelOn == LOW) ? LOW : HIGH);

			m_iFlashPulseDurationMSCounter = m_iFlashPulseDurationMS;
			m_flashState = FLASH_STATE::pulse_counting;

		}
		
		break;

	case FLASH_STATE::pulse_counting:
		if (--m_iFlashPulseDurationMSCounter <= 0) {
			//switch off
			digitalWrite(m_iPinUm, (m_bPinLevelOn == LOW) ? HIGH : LOW);

			if (m_iFlashCounts != -1) {
				if (--m_iFlashCountsCounter <= 0) {
					m_iSalvoIntervalMSCounter = m_iSalvoIntervalMS;
					m_flashState = FLASH_STATE::salvo_counting;
				}
				else {
					m_iFlashPulseIntervalMSCounter = m_iFlashPulseIntervalMS;
					m_flashState = FLASH_STATE::interval_counting;
				}
			}
			else {
				m_iFlashPulseIntervalMSCounter = m_iFlashPulseIntervalMS;
				m_flashState = FLASH_STATE::interval_counting;
			}
		}
		break;

	case FLASH_STATE::salvo_counting:
		if (--m_iSalvoIntervalMSCounter <= 0) {
			if (m_iSalvoCounts != -1) {
				if (--m_iSalvoCountsCounter <= 0) {
					stop(); 
				}
				else {
					m_iFlashPulseIntervalMSCounter = m_iFlashPulseIntervalMS;
					m_iFlashPulseDurationMSCounter = m_iFlashPulseDurationMS;
					m_iSalvoIntervalMSCounter = m_iSalvoIntervalMS;
					m_iFlashCountsCounter = m_iFlashCounts;

					m_flashState = FLASH_STATE::interval_counting;
				}
			}
			else {
				start();
			}
		}
		break;

	default:
		break;
	}
}

/**
*   Start flashing by enabling the state machine
*   params: 
*		NONE
*   return:
*       NONE       
*/
void CFlashLed::start() {
	//switch off
	digitalWrite(m_iPinUm, (m_bPinLevelOn == LOW) ? HIGH : LOW);

	m_iFlashPulseIntervalMSCounter = m_iFlashPulseIntervalMS;
	m_iFlashPulseDurationMSCounter = m_iFlashPulseDurationMS;
	m_iSalvoIntervalMSCounter = m_iSalvoIntervalMS;
	m_iFlashCountsCounter = m_iFlashCounts;
	m_iSalvoCountsCounter = m_iSalvoCounts;

	m_flashState = FLASH_STATE::interval_counting;
}

/**
*   Stop flashing by disabling the state machine
*   params: 
*		NONE
*   return:
*       NONE       
*/
void CFlashLed::stop() {
	m_flashState = FLASH_STATE::stop_flashing;
	digitalWrite(m_iPinUm, (m_bPinLevelOn == LOW) ? HIGH : LOW);
}
