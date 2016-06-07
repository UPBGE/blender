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

/** \file SCA_IInputDevice.h
 *  \ingroup gamelogic
 */

#ifndef __SCA_IINPUTDEVICE_H__
#define __SCA_IINPUTDEVICE_H__

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

#include "SCA_InputEvent.h"

#define MOUSEX         MOUSEMOVE
#define MOUSEY         ACTIONMOUSE

class SCA_IInputDevice 
{
public:
	SCA_IInputDevice();
	virtual ~SCA_IInputDevice();

	enum SCA_EnumInputs {
		NOKEY = 0,

		BEGINWIN,

		WINRESIZE,
		WINCLOSE,
		WINQUIT,

		ENDWIN,

		BEGINKEY,

		RETKEY,
		SPACEKEY,
		PADASTERKEY,
		COMMAKEY,
		MINUSKEY,
		PERIODKEY,
		PLUSKEY,
		ZEROKEY,

		ONEKEY,
		TWOKEY,
		THREEKEY,
		FOURKEY,
		FIVEKEY,
		SIXKEY,
		SEVENKEY,
		EIGHTKEY,
		NINEKEY,

		AKEY,
		BKEY,
		CKEY,
		DKEY,
		EKEY,
		FKEY,
		GKEY,
		HKEY,
		IKEY,
		JKEY,
		KKEY,
		LKEY,
		MKEY,
		NKEY,
		OKEY,
		PKEY,
		QKEY,
		RKEY,
		SKEY,
		TKEY,
		UKEY,
		VKEY,
		WKEY,
		XKEY,
		YKEY,
		ZKEY,

		CAPSLOCKKEY,

		LEFTCTRLKEY,
		LEFTALTKEY,
		RIGHTALTKEY,
		RIGHTCTRLKEY,
		RIGHTSHIFTKEY,
		LEFTSHIFTKEY,

		ESCKEY,
		TABKEY,

		LINEFEEDKEY,
		BACKSPACEKEY,
		DELKEY,
		SEMICOLONKEY,

		QUOTEKEY,
		ACCENTGRAVEKEY,

		SLASHKEY,
		BACKSLASHKEY,
		EQUALKEY,
		LEFTBRACKETKEY,
		RIGHTBRACKETKEY,

		LEFTARROWKEY,
		DOWNARROWKEY,
		RIGHTARROWKEY,
		UPARROWKEY,

		PAD2,
		PAD4,
		PAD6,
		PAD8,

		PAD1,
		PAD3,
		PAD5,
		PAD7,
		PAD9,
		
		PADPERIOD,
		PADSLASHKEY,

		PAD0,
		PADMINUS,
		PADENTER,
		PADPLUSKEY,

		F1KEY,
		F2KEY,
		F3KEY,
		F4KEY,
		F5KEY,
		F6KEY,
		F7KEY,
		F8KEY,
		F9KEY,
		F10KEY,
		F11KEY,
		F12KEY,
		F13KEY,
		F14KEY,
		F15KEY,
		F16KEY,
		F17KEY,
		F18KEY,
		F19KEY,

		OSKEY,

		PAUSEKEY,
		INSERTKEY,
		HOMEKEY,
		PAGEUPKEY,
		PAGEDOWNKEY,
		ENDKEY,

		BEGINMOUSE,

		BEGINMOUSEBUTTONS,

		LEFTMOUSE,
		MIDDLEMOUSE,
		RIGHTMOUSE,

		ENDMOUSEBUTTONS,

		WHEELUPMOUSE,
		WHEELDOWNMOUSE,

		MOUSEX,
		MOUSEY,

		ENDMOUSE,

		MAX_KEYS
	}; // enum


protected:
	/// Table of all possible input.
	SCA_InputEvent m_eventsTable[SCA_IInputDevice::MAX_KEYS];
	/// Typed text in unicode during a frame.
	std::wstring m_text;

public:
	virtual SCA_InputEvent& GetEvent(SCA_IInputDevice::SCA_EnumInputs inputcode);

	/**
	 * Count active events(active and just_activated)
	 */
	virtual int		GetNumActiveEvents();

	/**
	 * Get the number of remapping events (just_activated, just_released)
	 */
	virtual int		GetNumJustEvents();

	/** Clear event:
	 *     - Clear status and copy last status to first status.
	 *     - Clear queue
	 *     - Clear values and copy last value to first value.
	 */
	virtual void ClearEvents();

	/** Manage move event like mouse by releasing if possible.
	 * These kind of events are precise of one frame.
	 */
	virtual void ReleaseMoveEvent();

	/// Return typed unicode text during a frame.
	const std::wstring& GetText() const;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:SCA_InputEvent")
#endif
};

#endif	 /* __SCA_IINPUTDEVICE_H__ */

