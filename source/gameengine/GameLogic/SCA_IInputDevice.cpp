/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/GameLogic/SCA_IInputDevice.cpp
 *  \ingroup gamelogic
 */


#include "BLI_utildefines.h"
#include "SCA_IInputDevice.h"

SCA_IInputDevice::SCA_IInputDevice()
{
	for (int i = 0; i < SCA_IInputDevice::MAX_KEYS; ++i) {
		m_eventsTable[i] = SCA_InputEvent(i);
	}
}

SCA_IInputDevice::~SCA_IInputDevice()
{
	for (int i = 0; i < SCA_IInputDevice::MAX_KEYS; ++i) {
		m_eventsTable[i].InvalidateProxy();
	}
}

void SCA_IInputDevice::ClearEvents()
{
	for (int i = 0; i < SCA_IInputDevice::MAX_KEYS; ++i) {
		m_eventsTable[i].Clear();
	}
	m_text.clear();
}

void SCA_IInputDevice::ReleaseMoveEvent()
{
	/* We raise the release mouse move event if:
	 *   - there are only one value from the last call to Clear()
	 *   - the last state was ACTIVE
	 * If the both are true then the ACTIVE come from the last call to ClearEvent must
	 * be removed of the status list to avoid setting the mouse active for two frames.
	 */
	SCA_EnumInputs eventTypes[4] = {
		MOUSEX,
		MOUSEY,
		WHEELUPMOUSE,
		WHEELDOWNMOUSE
	};

	for (unsigned short i = 0; i < 4; ++i) {
		SCA_InputEvent &event = m_eventsTable[eventTypes[i]];
		if ((event.m_values.size() == 1) && (event.m_status[event.m_status.size() - 1] == SCA_InputEvent::ACTIVE)) {
			event.m_status.pop_back();
			event.m_status.push_back(SCA_InputEvent::NONE);
			event.m_queue.push_back(SCA_InputEvent::JUSTRELEASED);
		}
	}
}

const std::wstring& SCA_IInputDevice::GetText() const
{
	return m_text;
}

SCA_InputEvent& SCA_IInputDevice::GetEvent(SCA_IInputDevice::SCA_EnumInputs inputcode)
{
	return m_eventsTable[inputcode];
}
