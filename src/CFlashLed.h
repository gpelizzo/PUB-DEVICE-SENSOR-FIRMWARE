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
#ifndef _CFLASH_LED_H_
#define _CFLASH_LED_H_

#include <arduino.h>

class CFlashLed {
public:
	//multiple of 10ms
	const int FLASH_BUILTIN_ARRAY[4][5] = { {200, 50, 3, 1000, -1}, {200, 50, 1, 5000, -1} };

	enum FLASH_BUILTIN {not_config=0, alive = 1};

    void init (int p_iPinNum, boolean p_bPinLevelOn, int p_iFlashPulseIntervalMS, int p_iFlashPulseDurationMS, int p_iFlashCounts, int p_iSalvoIntervalMS, int p_iSalvoCounts, uint16_t p_uiTickingTimerMS);
	void init (int p_iPinNum, boolean p_bPinLevelOn, FLASH_BUILTIN p_builtIn, uint16_t p_uiTickingTimerMS);
	void init (int p_iPinNum, boolean p_bPinLevelOn, uint16_t p_uiTickingTimerMS);
	void updateModel (int p_iFlashPulseIntervalMS, int p_iFlashPulseDurationMS, int p_iFlashCounts, int p_iSalvoIntervalMS, int p_iSalvoCounts);
	void updateModel (FLASH_BUILTIN p_builtIn);
	void poll();
	void start();
	void stop();

private:
	enum FLASH_STATE { stop_flashing, interval_counting, pulse_counting, salvo_counting };

	int			m_iPinUm = -1;
	int 		m_iFlashPulseIntervalMS;
	int 		m_iFlashPulseDurationMS;
	int			m_iSalvoIntervalMS;
	int 		m_iFlashCounts;
	int 		m_iSalvoCounts;
	uint16_t	m_uiTickingTimerMS;

	int 		m_iFlashPulseIntervalMSCounter;
	int 		m_iFlashPulseDurationMSCounter;
	int 		m_iSalvoIntervalMSCounter;
	int 		m_iFlashCountsCounter;
	int 		m_iSalvoCountsCounter;
	boolean 	m_bPinLevelOn;

	FLASH_STATE m_flashState = FLASH_STATE::stop_flashing;
};

#endif