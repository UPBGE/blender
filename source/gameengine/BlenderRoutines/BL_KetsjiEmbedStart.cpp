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
 * Blender's Ketsji startpoint
 */

/** \file gameengine/BlenderRoutines/BL_KetsjiEmbedStart.cpp
 *  \ingroup blroutines
 */


#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _MSC_VER
   /* don't show stl-warnings */
#  pragma warning (disable:4786)
#endif

#include "KX_BlenderCanvas.h"

#include "KX_KetsjiEngine.h"
#include "KX_NetworkMessageManager.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_PythonInit.h"
#include "KX_PyConstraintBinding.h"
#include "KX_PythonMain.h"
#include "KX_Globals.h"

#include "RAS_OpenGLRasterizer.h"

#include "BL_BlenderDataConversion.h"

#include "GPU_extensions.h"
#include "EXP_Value.h"

#include "GHOST_ISystem.h"
#include "GH_EventConsumer.h"
#include "GH_InputDevice.h"

#include "LA_BlenderLauncher.h"

extern "C" {
	#include "DNA_object_types.h"
	#include "DNA_view3d_types.h"
	#include "DNA_screen_types.h"
	#include "DNA_userdef_types.h"
	#include "DNA_scene_types.h"
	#include "DNA_windowmanager_types.h"

	#include "BKE_global.h"
	#include "BKE_report.h"
	#include "BKE_ipo.h"
	#include "BKE_main.h"
	#include "BKE_context.h"
	#include "BKE_sound.h"

	/* avoid c++ conflict with 'new' */
	#define new _new
	#include "BKE_screen.h"
	#undef new

	#include "MEM_guardedalloc.h"

	#include "BLI_blenlib.h"
	#include "BLO_readfile.h"

	#include "../../blender/windowmanager/WM_types.h"
	#include "../../blender/windowmanager/wm_window.h"

/* avoid more includes (not used by BGE) */
typedef void * wmUIHandlerFunc;
typedef void * wmUIHandlerRemoveFunc;

	#include "../../blender/windowmanager/wm_event_system.h"
}

#ifdef WITH_AUDASPACE
#  include AUD_DEVICE_H
#endif

static BlendFileData *load_game_data(char *filename)
{
	ReportList reports;
	BlendFileData *bfd;
	
	BKE_reports_init(&reports, RPT_STORE);
	bfd= BLO_read_from_file(filename, &reports);

	if (!bfd) {
		printf("Loading %s failed: ", filename);
		BKE_reports_print(&reports, RPT_ERROR);
	}

	BKE_reports_clear(&reports);

	return bfd;
}


extern "C" void StartKetsjiShell(struct bContext *C, struct ARegion *ar, rcti *cam_frame, int always_use_expand_framing)
{
	/* context values */
	Scene *startscene = CTX_data_scene(C);
	Main* maggie1 = CTX_data_main(C);

	int exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
	Main* blenderdata = maggie1;

	char* startscenename = startscene->id.name + 2;
	char pathname[FILE_MAXDIR+FILE_MAXFILE];
	STR_String exitstring = "";
	BlendFileData *bfd = NULL;

	BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
#ifdef WITH_PYTHON

	// Acquire Python's GIL (global interpreter lock)
	// so we can safely run Python code and API calls
	PyGILState_STATE gilstate = PyGILState_Ensure();

	PyObject *globalDict = PyDict_New();

#endif

	GlobalSettings gs;
	gs.glslflag = startscene->gm.flag;

	do
	{
		// if we got an exitcode 3 (KX_EXIT_REQUEST_START_OTHER_GAME) load a different file
		if (exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME || exitrequested == KX_EXIT_REQUEST_RESTART_GAME) {
			exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
			if (bfd) {
				BLO_blendfiledata_free(bfd);
			}
			
			char basedpath[FILE_MAX];
			// base the actuator filename with respect
			// to the original file working directory

			if (exitstring != "") {
				BLI_strncpy(basedpath, exitstring.ReadPtr(), sizeof(basedpath));
			}

			// load relative to the last loaded file, this used to be relative
			// to the first file but that makes no sense, relative paths in
			// blend files should be relative to that file, not some other file
			// that happened to be loaded first
			BLI_path_abs(basedpath, pathname);
			bfd = load_game_data(basedpath);
			
			// if it wasn't loaded, try it forced relative
			if (!bfd) {
				// just add "//" in front of it
				char temppath[FILE_MAX] = "//";
				BLI_strncpy(temppath + 2, basedpath, FILE_MAX - 2);
				
				BLI_path_abs(temppath, pathname);
				bfd = load_game_data(temppath);
			}
			
			// if we got a loaded blendfile, proceed
			if (bfd) {
				blenderdata = bfd->main;
				startscenename = bfd->curscene->id.name + 2;

				if (blenderdata) {
					BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
				}
			}
			// else forget it, we can't find it
			else {
				exitrequested = KX_EXIT_REQUEST_QUIT_GAME;
			}
		}

		Scene *scene = bfd ? bfd->curscene : (Scene *)BLI_findstring(&blenderdata->scene, startscenename, offsetof(ID, name) + 2);

		RAS_IRasterizer::StereoMode stereoMode = RAS_IRasterizer::RAS_STEREO_NOSTEREO;
		if (scene) {
			// Quad buffered needs a special window.
			if (scene->gm.stereoflag == STEREO_ENABLED) {
				if (scene->gm.stereomode != RAS_IRasterizer::RAS_STEREO_QUADBUFFERED) {
					stereoMode = (RAS_IRasterizer::StereoMode)scene->gm.stereomode;
				}
			}
		}

		GHOST_ISystem *system = GHOST_ISystem::getSystem();
		LA_BlenderLauncher launcher = LA_BlenderLauncher(system, blenderdata, startscene, &gs, stereoMode, 0, NULL, C, cam_frame, ar, always_use_expand_framing);
#ifdef WITH_PYTHON
		launcher.SetPythonGlobalDict(globalDict);
#endif  // WITH_PYTHON

		launcher.InitEngine();

		std::cout << std::endl << "Blender Game Engine Started" << std::endl;
		bool run = true;
		while (run) {
			run = launcher.EngineNextFrame();
		}
		std::cout << "Blender Game Engine Finished" << std::endl;

		exitrequested = launcher.GetExitRequested();
		exitstring = launcher.GetExitString();
		gs = *launcher.GetGlobalSettings();

		launcher.ExitEngine();
	
	} while (exitrequested == KX_EXIT_REQUEST_RESTART_GAME || exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME);
	
	if (bfd) {
		BLO_blendfiledata_free(bfd);
	}

#ifdef WITH_PYTHON

	PyDict_Clear(globalDict);
	Py_DECREF(globalDict);

	// Release Python's GIL
	PyGILState_Release(gilstate);
#endif

}
