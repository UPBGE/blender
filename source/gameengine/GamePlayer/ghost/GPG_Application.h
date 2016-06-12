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

/** \file GPG_Application.h
 *  \ingroup player
 *  \brief GHOST Blender Player application declaration file.
 */

#include "GHOST_IEventConsumer.h"
#include "STR_String.h"

#ifdef WIN32
#include <wtypes.h>
#endif

#include "KX_KetsjiEngine.h"

class KX_KetsjiEngine;
class KX_Scene;
class KX_ISceneConverter;
class KX_NetworkMessageManager;
class RAS_IRasterizer;
class GHOST_IEvent;
class GHOST_ISystem;
class GHOST_ITimerTask;
class GHOST_IWindow;
class GPG_Canvas;
class GPG_System;
class GH_InputDevice;
class GH_EventConsumer;
struct Main;
struct Scene;

class GPG_Application
{
public:
	GPG_Application(GHOST_ISystem* system);
	~GPG_Application(void);

	bool SetGameEngineData(struct Main* maggie, struct Scene* scene, GlobalSettings* gs, int argc, char** argv);
	bool startWindow(STR_String& title,
	                 int windowLeft, int windowTop,
	                 int windowWidth, int windowHeight,
	                 const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
	bool startFullScreen(int width, int height,
	                     int bpp, int frequency,
	                     const bool stereoVisual, const int stereoMode,
	                     const GHOST_TUns16 samples=0, bool useDesktop=false);
	bool startEmbeddedWindow(STR_String& title, const GHOST_TEmbedderWindowID parent_window,
	                         const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
#ifdef WIN32
	bool startScreenSaverFullScreen(int width, int height,
	                                int bpp, int frequency,
	                                const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
	bool startScreenSaverPreview(HWND parentWindow,
	                             const bool stereoVisual, const int stereoMode, const GHOST_TUns16 samples=0);
#endif

	int getExitRequested(void);
	const STR_String& getExitString(void);
	GlobalSettings* getGlobalSettings(void);

	inline KX_Scene *GetStartScene() const
	{
		return m_kxStartScene;
	}

	bool StartGameEngine(int stereoMode);
	void StopGameEngine();
	void EngineNextFrame();

protected:
	/**
	 * Initializes the game engine.
	 */
	bool initEngine(GHOST_IWindow* window, int stereoMode);

	/**
	 * Starts the game engine.
	 */
	bool startEngine(void);

	/**
	 * Stop the game engine.
	 */
	void stopEngine(void);

	/**
	 * Shuts the game engine down.
	 */
	void exitEngine(void);
	short					m_exitkey;

	/* The game data */
	STR_String				m_startSceneName;
	struct Scene*			m_startScene;
	struct Main*			m_maggie;
	KX_Scene *m_kxStartScene;

	/* Exit state. */
	int						m_exitRequested;
	STR_String				m_exitString;
	GlobalSettings*	m_globalSettings;

	/* GHOST system abstraction. */
	GHOST_ISystem*			m_system;
	/* Main window. */
	GHOST_IWindow*			m_mainWindow;
	/* The cursor shape displayed. */
	GHOST_TStandardCursor	m_cursor;

	/** Engine construction state. */
	bool m_engineInitialized;
	/** Engine state. */
	bool m_engineRunning;
	/** Running on embedded window */
	bool m_isEmbedded;

	/** the gameengine itself */
	KX_KetsjiEngine* m_ketsjiengine;
	/** The game engine's system abstraction. */
	GPG_System* m_kxsystem;
	/** The game engine's input device abstraction. */
	GH_InputDevice *m_inputDevice;
	GH_EventConsumer *m_eventConsumer;
	/** The game engine's canvas abstraction. */
	GPG_Canvas* m_canvas;
	/** the rasterizer */
	RAS_IRasterizer* m_rasterizer;
	/** Converts Blender data files. */
	KX_ISceneConverter* m_sceneconverter;
	/// Manage messages.
	KX_NetworkMessageManager *m_networkMessageManager;

	/*
	 * GameLogic.globalDict as a string so that loading new blend files can use the same dict.
	 * Do this because python starts/stops when loading blend files.
	 */
	char* m_pyGlobalDictString;
	int m_pyGlobalDictString_Length;
	
	/* argc and argv need to be passed on to python */
	int		m_argc;
	char**	m_argv;
};
