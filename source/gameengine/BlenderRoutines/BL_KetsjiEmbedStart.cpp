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

#include "BL_System.h"
#include "BL_BlenderDataConversion.h"

#include "GPU_extensions.h"
#include "EXP_Value.h"

#include "GHOST_ISystem.h"
#include "GH_EventConsumer.h"
#include "GH_InputDevice.h"
#include "LA_System.h"

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

static int BL_KetsjiNextFrame(KX_KetsjiEngine *ketsjiengine, bContext *C, wmWindow *win, Scene *scene,
							  ARegion *ar, GHOST_ISystem *system, int draw_letterbox)
{
	// first check if we want to exit
	int exitrequested = ketsjiengine->GetExitCode();

	// kick the engine
	bool render = ketsjiengine->NextFrame();

	if (render) {
		if (draw_letterbox) {
			RAS_IRasterizer *rasty = ketsjiengine->GetRasterizer();

			// Clear screen to border color
			// We do this here since we set the canvas to be within the frames. This means the engine
			// itself is unaware of the extra space, so we clear the whole region for it.
			rasty->SetClearColor(scene->gm.framing.col[0], scene->gm.framing.col[1], scene->gm.framing.col[2]);
			rasty->SetViewport(ar->winrct.xmin, ar->winrct.ymin,
			           BLI_rcti_size_x(&ar->winrct), BLI_rcti_size_y(&ar->winrct));
			rasty->Clear(RAS_IRasterizer::RAS_COLOR_BUFFER_BIT);
		}

		// render the frame
		ketsjiengine->Render();
	}

	system->processEvents(false);
	system->dispatchEvents();

	SCA_IInputDevice *inputDevice = ketsjiengine->GetInputDevice();

	if (inputDevice->GetEvent((SCA_IInputDevice::SCA_EnumInputs)ketsjiengine->GetExitKey()).Find(SCA_InputEvent::KX_ACTIVE)) {
		exitrequested = KX_EXIT_REQUEST_BLENDER_ESC;
	}
	else if (inputDevice->GetEvent(SCA_IInputDevice::KX_WINCLOSE).Find(SCA_InputEvent::KX_ACTIVE) ||
		inputDevice->GetEvent(SCA_IInputDevice::KX_WINQUIT).Find(SCA_InputEvent::KX_ACTIVE))
	{
		exitrequested = KX_EXIT_REQUEST_OUTSIDE;
	}

	// test for the ESC key
	//XXX while (qtest())
	/*while (wmEvent *event= (wmEvent *)win->queue.first) {
		short val = 0;
		//unsigned short event = 0; //XXX extern_qread(&val);
		unsigned int unicode = event->utf8_buf[0] ? BLI_str_utf8_as_unicode(event->utf8_buf) : event->ascii;

		if (keyboarddevice->ConvertBlenderEvent(event->type, event->val, unicode))
			exitrequested = KX_EXIT_REQUEST_BLENDER_ESC;*/

		/* Coordinate conversion... where
		 * should this really be?
		 */
		/*if (event->type == MOUSEMOVE) {*/
			/* Note, not nice! XXX 2.5 event hack */
			/*val = event->x - ar->winrct.xmin;
			mousedevice->ConvertBlenderEvent(MOUSEX, val, 0);

			val = ar->winy - (event->y - ar->winrct.ymin) - 1;
			mousedevice->ConvertBlenderEvent(MOUSEY, val, 0);
		}
		else {
			mousedevice->ConvertBlenderEvent(event->type, event->val, 0);
		}

		BLI_remlink(&win->queue, event);
		wm_event_free(event);
	}*/

	if (win != CTX_wm_window(C)) {
		exitrequested= KX_EXIT_REQUEST_OUTSIDE; /* window closed while bge runs */
	}
	return exitrequested;
}


#ifdef WITH_PYTHON
static struct BL_KetsjiNextFrameState {
	class KX_KetsjiEngine* ketsjiengine;
	struct bContext *C;
	struct wmWindow* win;
	struct Scene* scene;
	struct ARegion *ar;
	GHOST_ISystem *system;
	int draw_letterbox;
} ketsjinextframestate;

static int BL_KetsjiPyNextFrame(void *state0)
{
	BL_KetsjiNextFrameState *state = (BL_KetsjiNextFrameState *) state0;
	return BL_KetsjiNextFrame(
		state->ketsjiengine, 
		state->C, 
		state->win, 
		state->scene, 
		state->ar,
		state->system,
		state->draw_letterbox);
}
#endif


extern "C" void StartKetsjiShell(struct bContext *C, struct ARegion *ar, rcti *cam_frame, int always_use_expand_framing)
{
	/* context values */
	struct wmWindowManager *wm= CTX_wm_manager(C);
	struct wmWindow *win= CTX_wm_window(C);
	struct Scene *startscene= CTX_data_scene(C);
	struct Main* maggie1= CTX_data_main(C);


	RAS_Rect area_rect;
	area_rect.SetLeft(cam_frame->xmin);
	area_rect.SetBottom(cam_frame->ymin);
	area_rect.SetRight(cam_frame->xmax);
	area_rect.SetTop(cam_frame->ymax);

	int exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
	Main* blenderdata = maggie1;

	char* startscenename = startscene->id.name+2;
	char pathname[FILE_MAXDIR+FILE_MAXFILE], oldsce[FILE_MAXDIR+FILE_MAXFILE];
	STR_String exitstring = "";
	BlendFileData *bfd= NULL;

	BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
	BLI_strncpy(oldsce, G.main->name, sizeof(oldsce));
#ifdef WITH_PYTHON
	resetGamePythonPath(); // need this so running a second time wont use an old blendfiles path
	setGamePythonPath(G.main->name);

	// Acquire Python's GIL (global interpreter lock)
	// so we can safely run Python code and API calls
	PyGILState_STATE gilstate = PyGILState_Ensure();
	
	PyObject *pyGlobalDict = PyDict_New(); /* python utility storage, spans blend file loading */
#endif

	// Globals to be carried on over blender files
	GlobalSettings gs;
	gs.glslflag= startscene->gm.flag;

	do
	{
		View3D *v3d= CTX_wm_view3d(C);
		RegionView3D *rv3d= CTX_wm_region_view3d(C);

		// get some preferences
		SYS_SystemHandle syshandle = SYS_GetSystem();
		bool properties	= (SYS_GetCommandLineInt(syshandle, "show_properties", 0) != 0);
		bool usefixed = (SYS_GetCommandLineInt(syshandle, "fixedtime", 0) != 0);
		bool profile = (SYS_GetCommandLineInt(syshandle, "show_profile", 0) != 0);
		bool frameRate = (SYS_GetCommandLineInt(syshandle, "show_framerate", 0) != 0);
		bool displaylists = (SYS_GetCommandLineInt(syshandle, "displaylists", 0) != 0) && GPU_display_list_support();
		bool showBoundingBox = (SYS_GetCommandLineInt(syshandle, "show_bounding_box", 0) != 0);
		bool showArmatures = (SYS_GetCommandLineInt(syshandle, "show_armatures", 0) != 0);
#ifdef WITH_PYTHON
		bool nodepwarnings = (SYS_GetCommandLineInt(syshandle, "ignore_deprecation_warnings", 0) != 0);
#endif
		// bool novertexarrays = (SYS_GetCommandLineInt(syshandle, "novertexarrays", 0) != 0);
		bool mouse_state = (startscene->gm.flag & GAME_SHOW_MOUSE) != 0;
		bool restrictAnimFPS = (startscene->gm.flag & GAME_RESTRICT_ANIM_UPDATES) != 0;

		RAS_IRasterizer::DrawType drawmode = RAS_IRasterizer::RAS_TEXTURED;
		switch(v3d->drawtype) {
			case OB_BOUNDBOX:
			case OB_WIRE:
			{
				drawmode = RAS_IRasterizer::RAS_WIREFRAME;
				break;
			}
			case OB_SOLID:
			{
				drawmode = RAS_IRasterizer::RAS_SOLID;
				break;
			}
			case OB_MATERIAL:
			{
				drawmode = RAS_IRasterizer::RAS_TEXTURED;
				break;
			}
		}

		// create the rasterizer and canvas
		RAS_IRasterizer* rasterizer = NULL;
		RAS_STORAGE_TYPE raster_storage = RAS_AUTO_STORAGE;
		int storageInfo = RAS_STORAGE_INFO_NONE;

		if (startscene->gm.raster_storage == RAS_STORE_VBO) {
			raster_storage = RAS_VBO;
		}
		else if (startscene->gm.raster_storage == RAS_STORE_VA) {
			raster_storage = RAS_VA;
		}

		if (displaylists) {
			storageInfo |= RAS_STORAGE_USE_DISPLAY_LIST;
		}

		rasterizer = new RAS_OpenGLRasterizer(raster_storage, storageInfo);

		RAS_IRasterizer::MipmapOption mipmapval = rasterizer->GetMipmapping();

		RAS_ICanvas* canvas = new KX_BlenderCanvas(rasterizer, wm, win, area_rect, ar);

		// default mouse state set on render panel
		if (mouse_state)
			canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);
		else
			canvas->SetMouseState(RAS_ICanvas::MOUSE_INVISIBLE);

		// Setup vsync
		int previous_vsync = 0;
		canvas->GetSwapInterval(previous_vsync);
		if (startscene->gm.vsync == VSYNC_ADAPTIVE)
			canvas->SetSwapInterval(-1);
		else
			canvas->SetSwapInterval((startscene->gm.vsync == VSYNC_ON) ? 1 : 0);
		
		// create the inputdevices
		GH_InputDevice *inputDevice = new GH_InputDevice();

		GHOST_ISystem *system = GHOST_ISystem::getSystem();
		GH_EventConsumer *eventConsumer = new GH_EventConsumer(inputDevice);
		system->addEventConsumer(eventConsumer);

		//
		// create a ketsji/blendersystem (only needed for timing and stuff)
		KX_ISystem *kxsystem = new LA_System();

		KX_NetworkMessageManager *networkMessageManager = new KX_NetworkMessageManager();

		// create the ketsjiengine
		KX_KetsjiEngine* ketsjiengine = new KX_KetsjiEngine(kxsystem);
		
		// set the devices
		ketsjiengine->SetInputDevice(inputDevice);
		ketsjiengine->SetCanvas(canvas);
		ketsjiengine->SetRasterizer(rasterizer);
		ketsjiengine->SetNetworkMessageManager(networkMessageManager);
		ketsjiengine->SetUseFixedTime(usefixed);
		ketsjiengine->SetTimingDisplay(frameRate, profile, properties);
		ketsjiengine->SetRestrictAnimationFPS(restrictAnimFPS);
		ketsjiengine->SetShowBoundingBox(showBoundingBox);
		ketsjiengine->SetShowArmatures(showArmatures);
		KX_KetsjiEngine::SetExitKey(ConvertKeyCode(startscene->gm.exitkey));

		//set the global settings (carried over if restart/load new files)
		ketsjiengine->SetGlobalSettings(&gs);

#ifdef WITH_PYTHON
		CValue::SetDeprecationWarnings(nodepwarnings);
#endif

		//lock frame and camera enabled - storing global values
		int tmp_lay= startscene->lay;
		Object *tmp_camera = startscene->camera;

		if (v3d->scenelock==0) {
			startscene->lay= v3d->lay;
			startscene->camera= v3d->camera;
		}

		// some blender stuff
		float camzoom = 1.0f;
		int draw_letterbox = 0;

		if (rv3d->persp==RV3D_CAMOB) {
			if (startscene->gm.framing.type == SCE_GAMEFRAMING_BARS) { /* Letterbox */
				draw_letterbox = 1;
			}
			else {
				camzoom = 1.0f / BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
			}
		}

		rasterizer->SetDrawingMode(drawmode);
		ketsjiengine->SetCameraZoom(camzoom);
		ketsjiengine->SetCameraOverrideZoom(2.0f);

		// if we got an exitcode 3 (KX_EXIT_REQUEST_START_OTHER_GAME) load a different file
		if (exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME || exitrequested == KX_EXIT_REQUEST_RESTART_GAME)
		{
			exitrequested = KX_EXIT_REQUEST_NO_REQUEST;
			if (bfd) BLO_blendfiledata_free(bfd);
			
			char basedpath[FILE_MAX];
			// base the actuator filename with respect
			// to the original file working directory

			if (exitstring != "")
				BLI_strncpy(basedpath, exitstring.ReadPtr(), sizeof(basedpath));

			// load relative to the last loaded file, this used to be relative
			// to the first file but that makes no sense, relative paths in
			// blend files should be relative to that file, not some other file
			// that happened to be loaded first
			BLI_path_abs(basedpath, pathname);
			bfd = load_game_data(basedpath);
			
			// if it wasn't loaded, try it forced relative
			if (!bfd)
			{
				// just add "//" in front of it
				char temppath[FILE_MAX] = "//";
				BLI_strncpy(temppath + 2, basedpath, FILE_MAX - 2);
				
				BLI_path_abs(temppath, pathname);
				bfd = load_game_data(temppath);
			}
			
			// if we got a loaded blendfile, proceed
			if (bfd)
			{
				blenderdata = bfd->main;
				startscenename = bfd->curscene->id.name + 2;

				if (blenderdata) {
					BLI_strncpy(G.main->name, blenderdata->name, sizeof(G.main->name));
					BLI_strncpy(pathname, blenderdata->name, sizeof(pathname));
#ifdef WITH_PYTHON
					setGamePythonPath(G.main->name);
#endif
				}
			}
			// else forget it, we can't find it
			else
			{
				exitrequested = KX_EXIT_REQUEST_QUIT_GAME;
			}
		}

		Scene *scene= bfd ? bfd->curscene : (Scene *)BLI_findstring(&blenderdata->scene, startscenename, offsetof(ID, name) + 2);

		if (scene)
		{
			// Quad buffered needs a special window.
			if (scene->gm.stereoflag == STEREO_ENABLED) {
				if (scene->gm.stereomode != RAS_IRasterizer::RAS_STEREO_QUADBUFFERED)
					rasterizer->SetStereoMode((RAS_IRasterizer::StereoMode) scene->gm.stereomode);

				rasterizer->SetEyeSeparation(scene->gm.eyeseparation);
			}
		}
		
		if (exitrequested != KX_EXIT_REQUEST_QUIT_GAME)
		{
			if (rv3d->persp != RV3D_CAMOB)
			{
				ketsjiengine->EnableCameraOverride(startscenename);
				ketsjiengine->SetCameraOverrideUseOrtho((rv3d->persp == RV3D_ORTHO));
				ketsjiengine->SetCameraOverrideProjectionMatrix(MT_CmMatrix4x4(rv3d->winmat));
				ketsjiengine->SetCameraOverrideViewMatrix(MT_CmMatrix4x4(rv3d->viewmat));
				ketsjiengine->SetCameraOverrideClipping(v3d->near, v3d->far);
				ketsjiengine->SetCameraOverrideLens(v3d->lens);
			}
			
			// create a scene converter, create and convert the startingscene
			KX_ISceneConverter* sceneconverter = new KX_BlenderSceneConverter(blenderdata, ketsjiengine);
			ketsjiengine->SetSceneConverter(sceneconverter);
			if (always_use_expand_framing)
				sceneconverter->SetAlwaysUseExpandFraming(true);

			KX_Scene* startscene = new KX_Scene(inputDevice,
				startscenename,
				scene,
				canvas,
				networkMessageManager);

			KX_SetActiveScene(startscene);
			KX_SetActiveEngine(ketsjiengine);
#ifdef WITH_PYTHON
			// some python things
			PyObject *gameLogic, *gameLogic_keys;
			setupGamePython(ketsjiengine, blenderdata, pyGlobalDict, &gameLogic, &gameLogic_keys, 0, NULL);
#endif // WITH_PYTHON

			//initialize Dome Settings
			if (scene->gm.stereoflag == STEREO_DOME)
				ketsjiengine->InitDome(scene->gm.dome.res, scene->gm.dome.mode, scene->gm.dome.angle, scene->gm.dome.resbuf, scene->gm.dome.tilt, scene->gm.dome.warptext);

			// initialize 3D Audio Settings
			AUD_Device* device = BKE_sound_get_device();
			AUD_Device_setSpeedOfSound(device, scene->audio.speed_of_sound);
			AUD_Device_setDopplerFactor(device, scene->audio.doppler_factor);
			AUD_Device_setDistanceModel(device, AUD_DistanceModel(scene->audio.distance_model));

			// from see blender.c:
			// FIXME: this version patching should really be part of the file-reading code,
			// but we still get too many unrelated data-corruption crashes otherwise...
			if (blenderdata->versionfile < 250)
				do_versions_ipos_to_animato(blenderdata);

			if (sceneconverter)
			{
				// convert and add scene
				sceneconverter->ConvertScene(
					startscene,
				    rasterizer,
					canvas);
				ketsjiengine->AddScene(startscene);
				startscene->Release();
				
				// init the rasterizer
				rasterizer->Init();
				
				// start the engine
				ketsjiengine->StartEngine(true);
				

				// Set the animation playback rate for ipo's and actions
				// the framerate below should patch with FPS macro defined in blendef.h
				// Could be in StartEngine set the framerate, we need the scene to do this
				ketsjiengine->SetAnimFrameRate(FPS);
				
#ifdef WITH_PYTHON
				char *python_main = NULL;
				pynextframestate.state = NULL;
				pynextframestate.func = NULL;
				python_main = KX_GetPythonMain(scene);

				// the mainloop
				printf("\nBlender Game Engine Started\n");
				if (python_main) {
					char *python_code = KX_GetPythonCode(blenderdata, python_main);
					if (python_code) {
						// Set python environement variable.
						PHY_SetActiveEnvironment(startscene->GetPhysicsEnvironment());

						ketsjinextframestate.ketsjiengine = ketsjiengine;
						ketsjinextframestate.C = C;
						ketsjinextframestate.win = win;
						ketsjinextframestate.scene = scene;
						ketsjinextframestate.ar = ar;
						ketsjinextframestate.system = system;
						ketsjinextframestate.draw_letterbox = draw_letterbox;
			
						pynextframestate.state = &ketsjinextframestate;
						pynextframestate.func = &BL_KetsjiPyNextFrame;
						printf("Yielding control to Python script '%s'...\n", python_main);
						PyRun_SimpleString(python_code);
						printf("Exit Python script '%s'\n", python_main);
						MEM_freeN(python_code);
					}
				}
				else
#endif  /* WITH_PYTHON */
				{
					while (!exitrequested)
					{
						exitrequested = BL_KetsjiNextFrame(ketsjiengine, C, win, scene, ar, system, draw_letterbox);
					}
				}
				printf("Blender Game Engine Finished\n");
				exitstring = ketsjiengine->GetExitString();
#ifdef WITH_PYTHON
				if (python_main) MEM_freeN(python_main);
#endif  /* WITH_PYTHON */

				gs = *(ketsjiengine->GetGlobalSettings());

				// when exiting the mainloop
#ifdef WITH_PYTHON
				// Clears the dictionary by hand:
				// This prevents, extra references to global variables
				// inside the GameLogic dictionary when the python interpreter is finalized.
				// which allows the scene to safely delete them :)
				// see: (space.c)->start_game

				PyDict_Clear(PyModule_GetDict(gameLogic));
				PyDict_SetItemString(PyModule_GetDict(gameLogic), "globalDict", pyGlobalDict);
#endif

				ketsjiengine->StopEngine();
#ifdef WITH_PYTHON
				exitGamePythonScripting();
#endif
			}
			if (sceneconverter)
			{
				delete sceneconverter;
				sceneconverter = NULL;
			}

#ifdef WITH_PYTHON
			Py_DECREF(gameLogic_keys);
			gameLogic_keys = NULL;
#endif
		}
		//lock frame and camera enabled - restoring global values
		if (v3d->scenelock==0) {
			startscene->lay= tmp_lay;
			startscene->camera= tmp_camera;
		}

		if (exitrequested != KX_EXIT_REQUEST_OUTSIDE)
		{
			// set the cursor back to normal
			canvas->SetMouseState(RAS_ICanvas::MOUSE_NORMAL);

			// set mipmap setting back to its original value
			rasterizer->SetMipmapping(mipmapval);
		}
		
		// clean up some stuff
		if (ketsjiengine)
		{
			delete ketsjiengine;
			ketsjiengine = NULL;
		}
		if (kxsystem)
		{
			delete kxsystem;
			kxsystem = NULL;
		}
		if (inputDevice) {
			delete inputDevice;
			inputDevice = NULL;
		}
		if (canvas)
		{
			canvas->SetSwapInterval(previous_vsync); // Set the swap interval back
			delete canvas;
			canvas = NULL;
		}
		if (rasterizer)
		{
			delete rasterizer;
			rasterizer = NULL;
		}
		if (networkMessageManager) {
			delete networkMessageManager;
			networkMessageManager = NULL;
		}

		// stop all remaining playing sounds
		AUD_Device_stopAll(BKE_sound_get_device());

		system->removeEventConsumer(eventConsumer);
		delete eventConsumer;
	
	} while (exitrequested == KX_EXIT_REQUEST_RESTART_GAME || exitrequested == KX_EXIT_REQUEST_START_OTHER_GAME);
	
	if (bfd) BLO_blendfiledata_free(bfd);

	BLI_strncpy(G.main->name, oldsce, sizeof(G.main->name));

#ifdef WITH_PYTHON
	PyDict_Clear(pyGlobalDict);
	Py_DECREF(pyGlobalDict);

	// Release Python's GIL
	PyGILState_Release(gilstate);
#endif

}
