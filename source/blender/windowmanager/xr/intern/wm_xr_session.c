/*
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
 */

/** \file
 * \ingroup wm
 */

#include "BKE_callbacks.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DEG_depsgraph.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"

#include "DRW_engine.h"

#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "GHOST_C-api.h"

#include "GPU_viewport.h"

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "../transform/transform.h"
#include "../transform/transform_convert.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm_event_system.h"
#include "wm_surface.h"
#include "wm_window.h"
#include "wm_xr_intern.h"

static wmSurface *g_xr_surface = NULL;
static CLG_LogRef LOG = {"wm.xr"};

/* -------------------------------------------------------------------- */

static void wm_xr_session_object_pose_get(const Object *ob, GHOST_XrPose *pose)
{
  copy_v3_v3(pose->position, ob->loc);
  eul_to_quat(pose->orientation_quat, ob->rot);
}

static void wm_xr_session_object_pose_set(const GHOST_XrPose *pose, Object *ob)
{
  copy_v3_v3(ob->loc, pose->position);
  quat_to_eul(ob->rot, pose->orientation_quat);

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
}

static void wm_xr_session_create_cb(void)
{
  Main *bmain = G_MAIN;
  wmWindowManager *wm = bmain->wm.first;
  wmXrData *xr_data = &wm->xr;
  XrSessionSettings *settings = &xr_data->session_settings;
  wmXrSessionState *state = &xr_data->runtime->session_state;

  /* Get action set data from Python. */
  BKE_callback_exec_null(bmain, BKE_CB_EVT_XR_SESSION_START_PRE);

  wm_xr_session_actions_init(xr_data);

  /* Store constraint object poses. */
  if (settings->headset_object) {
    wm_xr_session_object_pose_get(settings->headset_object, &state->headset_object_orig_pose);
  }
  if (settings->controller0_object) {
    wm_xr_session_object_pose_get(settings->controller0_object,
                                  &state->controller0_object_orig_pose);
  }
  if (settings->controller1_object) {
    wm_xr_session_object_pose_get(settings->controller1_object,
                                  &state->controller1_object_orig_pose);
  }

  /* Initialize navigation. */
  WM_xr_session_state_navigation_reset(state);
  if (settings->base_scale < 0.001f) {
    settings->base_scale = 1.0f;
  }
  state->prev_base_scale = settings->base_scale;
}

static void wm_xr_session_exit_cb(void *customdata)
{
  wmXrData *xr_data = customdata;
  XrSessionSettings *settings = &xr_data->session_settings;
  wmXrSessionState *state = &xr_data->runtime->session_state;

  state->is_started = false;

  /* Restore constraint object poses. */
  if (settings->headset_object) {
    wm_xr_session_object_pose_set(&state->headset_object_orig_pose, settings->headset_object);
  }
  if (settings->controller0_object) {
    wm_xr_session_object_pose_set(&state->controller0_object_orig_pose,
                                  settings->controller0_object);
  }
  if (settings->controller1_object) {
    wm_xr_session_object_pose_set(&state->controller1_object_orig_pose,
                                  settings->controller1_object);
  }

  wm_xr_session_actions_uninit(xr_data);

  if (xr_data->runtime->exit_fn) {
    xr_data->runtime->exit_fn(xr_data);
  }

  /* Free the entire runtime data (including session state and context), to play safe. */
  wm_xr_runtime_data_free(&xr_data->runtime);
}

static void wm_xr_session_begin_info_create(wmXrData *xr_data,
                                            GHOST_XrSessionBeginInfo *r_begin_info)
{
  /* Callback for when the session is created. This is needed to create and bind OpenXR actions
   * after the session is created but before it is started. */
  r_begin_info->create_fn = wm_xr_session_create_cb;

  /* WM-XR exit function, does some own stuff and calls callback passed to wm_xr_session_toggle(),
   * to allow external code to execute its own session-exit logic. */
  r_begin_info->exit_fn = wm_xr_session_exit_cb;
  r_begin_info->exit_customdata = xr_data;
}

void wm_xr_session_toggle(wmWindowManager *wm,
                          wmWindow *session_root_win,
                          wmXrSessionExitFn session_exit_fn)
{
  wmXrData *xr_data = &wm->xr;

  if (WM_xr_session_exists(xr_data)) {
    GHOST_XrSessionEnd(xr_data->runtime->context);
  }
  else {
    GHOST_XrSessionBeginInfo begin_info;

    xr_data->runtime->session_root_win = session_root_win;
    xr_data->runtime->session_state.is_started = true;
    xr_data->runtime->exit_fn = session_exit_fn;

    wm_xr_session_begin_info_create(xr_data, &begin_info);
    GHOST_XrSessionStart(xr_data->runtime->context, &begin_info);
  }
}

/**
 * Check if the XR-Session was triggered.
 * If an error happened while trying to start a session, this returns false too.
 */
bool WM_xr_session_exists(const wmXrData *xr)
{
  return xr->runtime && xr->runtime->context && xr->runtime->session_state.is_started;
}

void WM_xr_session_base_pose_reset(wmXrData *xr)
{
  xr->runtime->session_state.force_reset_to_base_pose = true;
}

/**
 * Check if the session is running, according to the OpenXR definition.
 */
bool WM_xr_session_is_ready(const wmXrData *xr)
{
  return WM_xr_session_exists(xr) && GHOST_XrSessionIsRunning(xr->runtime->context);
}

static void wm_xr_session_base_pose_calc(const Scene *scene,
                                         const XrSessionSettings *settings,
                                         GHOST_XrPose *r_base_pose,
                                         float *r_base_scale)
{
  const Object *base_pose_object = ((settings->base_pose_type == XR_BASE_POSE_OBJECT) &&
                                    settings->base_pose_object) ?
                                       settings->base_pose_object :
                                       scene->camera;

  if (settings->base_pose_type == XR_BASE_POSE_CUSTOM) {
    float tmp_quatx[4], tmp_quatz[4];

    copy_v3_v3(r_base_pose->position, settings->base_pose_location);
    axis_angle_to_quat_single(tmp_quatx, 'X', M_PI_2);
    axis_angle_to_quat_single(tmp_quatz, 'Z', settings->base_pose_angle);
    mul_qt_qtqt(r_base_pose->orientation_quat, tmp_quatz, tmp_quatx);
  }
  else if (base_pose_object) {
    float tmp_quat[4];
    float tmp_eul[3];

    mat4_to_loc_quat(r_base_pose->position, tmp_quat, base_pose_object->obmat);

    /* Only use rotation around Z-axis to align view with floor. */
    quat_to_eul(tmp_eul, tmp_quat);
    tmp_eul[0] = M_PI_2;
    tmp_eul[1] = 0;
    eul_to_quat(r_base_pose->orientation_quat, tmp_eul);
  }
  else {
    copy_v3_fl(r_base_pose->position, 0.0f);
    axis_angle_to_quat_single(r_base_pose->orientation_quat, 'X', M_PI_2);
  }

  *r_base_scale = settings->base_scale;
}

static void wm_xr_session_draw_data_populate(wmXrData *xr_data,
                                             Scene *scene,
                                             Depsgraph *depsgraph,
                                             wmXrDrawData *r_draw_data)
{
  const XrSessionSettings *settings = &xr_data->session_settings;

  memset(r_draw_data, 0, sizeof(*r_draw_data));
  r_draw_data->scene = scene;
  r_draw_data->depsgraph = depsgraph;
  r_draw_data->xr_data = xr_data;
  r_draw_data->surface_data = g_xr_surface->customdata;

  wm_xr_session_base_pose_calc(
      r_draw_data->scene, settings, &r_draw_data->base_pose, &r_draw_data->base_scale);
}

wmWindow *wm_xr_session_root_window_or_fallback_get(const wmWindowManager *wm,
                                                    const wmXrRuntimeData *runtime_data)
{
  if (runtime_data->session_root_win &&
      BLI_findindex(&wm->windows, runtime_data->session_root_win) != -1) {
    /* Root window is still valid, use it. */
    return runtime_data->session_root_win;
  }
  /* Otherwise, fallback. */
  return wm->windows.first;
}

/**
 * Get the scene and depsgraph shown in the VR session's root window (the window the session was
 * started from) if still available. If it's not available, use some fallback window.
 *
 * It's important that the VR session follows some existing window, otherwise it would need to have
 * an own depsgraph, which is an expense we should avoid.
 */
static void wm_xr_session_scene_and_evaluated_depsgraph_get(Main *bmain,
                                                            const wmWindowManager *wm,
                                                            Scene **r_scene,
                                                            Depsgraph **r_depsgraph)
{
  const wmWindow *root_win = wm_xr_session_root_window_or_fallback_get(wm, wm->xr.runtime);

  /* Follow the scene & view layer shown in the root 3D View. */
  Scene *scene = WM_window_get_active_scene(root_win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(root_win);

  Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
  BLI_assert(scene && view_layer && depsgraph);
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  *r_scene = scene;
  *r_depsgraph = depsgraph;
}

typedef enum wmXrSessionStateEvent {
  SESSION_STATE_EVENT_NONE = 0,
  SESSION_STATE_EVENT_START,
  SESSION_STATE_EVENT_RESET_TO_BASE_POSE,
  SESSION_STATE_EVENT_POSITION_TRACKING_TOGGLE,
} wmXrSessionStateEvent;

static bool wm_xr_session_draw_data_needs_reset_to_base_pose(const wmXrSessionState *state,
                                                             const XrSessionSettings *settings)
{
  if (state->force_reset_to_base_pose) {
    return true;
  }
  return ((settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) &&
         ((state->prev_base_pose_type != settings->base_pose_type) ||
          (state->prev_base_pose_object != settings->base_pose_object));
}

static wmXrSessionStateEvent wm_xr_session_state_to_event(const wmXrSessionState *state,
                                                          const XrSessionSettings *settings)
{
  if (!state->is_view_data_set) {
    return SESSION_STATE_EVENT_START;
  }
  if (wm_xr_session_draw_data_needs_reset_to_base_pose(state, settings)) {
    return SESSION_STATE_EVENT_RESET_TO_BASE_POSE;
  }

  const bool position_tracking_toggled = ((state->prev_settings_flag &
                                           XR_SESSION_USE_POSITION_TRACKING) !=
                                          (settings->flag & XR_SESSION_USE_POSITION_TRACKING));
  if (position_tracking_toggled) {
    return SESSION_STATE_EVENT_POSITION_TRACKING_TOGGLE;
  }

  return SESSION_STATE_EVENT_NONE;
}

void wm_xr_session_draw_data_update(wmXrSessionState *state,
                                    const XrSessionSettings *settings,
                                    const GHOST_XrDrawViewInfo *draw_view,
                                    wmXrDrawData *draw_data)
{
  const wmXrSessionStateEvent event = wm_xr_session_state_to_event(state, settings);
  const bool use_position_tracking = (settings->flag & XR_SESSION_USE_POSITION_TRACKING);

  switch (event) {
    case SESSION_STATE_EVENT_START:
      if (use_position_tracking) {
        /* We want to start the session exactly at landmark position.
         * Run-times may have a non-[0,0,0] starting position that we have to subtract for that. */
        copy_v3_v3(draw_data->eye_position_ofs, draw_view->local_pose.position);
      }
      else {
        copy_v3_fl(draw_data->eye_position_ofs, 0.0f);
      }
      break;
      /* This should be triggered by the VR add-on if a landmark changes. */
    case SESSION_STATE_EVENT_RESET_TO_BASE_POSE:
      if (use_position_tracking) {
        /* Switch exactly to base pose, so use eye offset to cancel out current position delta. */
        copy_v3_v3(draw_data->eye_position_ofs, draw_view->local_pose.position);
      }
      else {
        copy_v3_fl(draw_data->eye_position_ofs, 0.0f);
      }
      /* Reset navigation. */
      WM_xr_session_state_navigation_reset(state);
      break;
    case SESSION_STATE_EVENT_POSITION_TRACKING_TOGGLE:
      if (use_position_tracking) {
        /* Keep the current position, and let the user move from there. */
        copy_v3_v3(draw_data->eye_position_ofs, state->prev_eye_position_ofs);
      }
      else {
        /* Back to the exact base-pose position. */
        copy_v3_fl(draw_data->eye_position_ofs, 0.0f);
      }
      break;
    case SESSION_STATE_EVENT_NONE:
      /* Keep previous offset when positional tracking is disabled. */
      copy_v3_v3(draw_data->eye_position_ofs, state->prev_eye_position_ofs);
      break;
  }
}

/**
 * Update information that is only stored for external state queries. E.g. for Python API to
 * request the current (as in, last known) viewer pose.
 * Controller data and action sets will be updated separately via wm_xr_session_actions_update().
 */
void wm_xr_session_state_update(const XrSessionSettings *settings,
                                const wmXrDrawData *draw_data,
                                const GHOST_XrDrawViewInfo *draw_view,
                                const float viewmat[4][4],
                                const float viewmat_base[4][4],
                                wmXrSessionState *state)
{
  const unsigned int view_idx = (unsigned int)min_ii((int)ARRAY_SIZE(state->eyes),
                                                     (int)draw_view->view_idx);
  wmXrEyeData *eye = &state->eyes[view_idx];
  GHOST_XrPose viewer_pose;
  float viewer_mat[4][4], base_mat[4][4], nav_mat[4][4];

  /* Calculate viewer matrix. */
  copy_qt_qt(viewer_pose.orientation_quat, draw_view->local_pose.orientation_quat);
  if ((settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) {
    zero_v3(viewer_pose.position);
  }
  else {
    copy_v3_v3(viewer_pose.position, draw_view->local_pose.position);
  }
  if ((settings->flag & XR_SESSION_USE_ABSOLUTE_TRACKING) == 0) {
    sub_v3_v3(viewer_pose.position, draw_data->eye_position_ofs);
  }
  wm_xr_pose_to_mat(&viewer_pose, viewer_mat);

  /* Apply base pose and navigation. */
  wm_xr_pose_scale_to_mat(&draw_data->base_pose, draw_data->base_scale, base_mat);
  wm_xr_pose_scale_to_mat(&state->nav_pose_prev, state->nav_scale_prev, nav_mat);
  mul_m4_m4m4(state->viewer_mat_base, base_mat, viewer_mat);
  mul_m4_m4m4(viewer_mat, nav_mat, state->viewer_mat_base);

  /* Save final viewer pose and viewmat. */
  mat4_to_loc_quat(state->viewer_pose.position, state->viewer_pose.orientation_quat, viewer_mat);
  wm_xr_pose_scale_to_imat(
      &state->viewer_pose, draw_data->base_scale * state->nav_scale_prev, state->viewer_viewmat);

  /* No idea why, but multiplying by two seems to make it match the VR view more. */
  eye->focal_len = 2.0f *
                   fov_to_focallength(draw_view->fov.angle_right - draw_view->fov.angle_left,
                                      DEFAULT_SENSOR_WIDTH);
  copy_m4_m4(eye->viewmat, viewmat);
  copy_m4_m4(eye->viewmat_base, viewmat_base);

  memcpy(&state->prev_base_pose, &draw_data->base_pose, sizeof(state->prev_base_pose));
  state->prev_base_scale = draw_data->base_scale;
  memcpy(&state->prev_local_pose, &draw_view->local_pose, sizeof(state->prev_local_pose));
  copy_v3_v3(state->prev_eye_position_ofs, draw_data->eye_position_ofs);

  state->prev_settings_flag = settings->flag;
  state->prev_base_pose_type = settings->base_pose_type;
  state->prev_base_pose_object = settings->base_pose_object;
  state->is_view_data_set = true;
  /* Assume this was already done through wm_xr_session_draw_data_update(). */
  state->force_reset_to_base_pose = false;
}

wmXrSessionState *WM_xr_session_state_handle_get(const wmXrData *xr)
{
  return xr->runtime ? &xr->runtime->session_state : NULL;
}

bool WM_xr_session_state_viewer_pose_location_get(const wmXrData *xr, float r_location[3])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    zero_v3(r_location);
    return false;
  }

  copy_v3_v3(r_location, xr->runtime->session_state.viewer_pose.position);
  return true;
}

bool WM_xr_session_state_viewer_pose_rotation_get(const wmXrData *xr, float r_rotation[4])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    unit_qt(r_rotation);
    return false;
  }

  copy_qt_qt(r_rotation, xr->runtime->session_state.viewer_pose.orientation_quat);
  return true;
}

bool WM_xr_session_state_viewer_pose_matrix_info_get(const wmXrData *xr,
                                                     bool from_selection_eye,
                                                     float r_viewmat[4][4],
                                                     float *r_focal_len,
                                                     float *r_clip_start,
                                                     float *r_clip_end)
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    unit_m4(r_viewmat);
    *r_focal_len = *r_clip_start = *r_clip_end = 0.0f;
    return false;
  }

  const wmXrEyeData *eye = &xr->runtime->session_state.eyes[xr->session_settings.selection_eye];
  if (from_selection_eye) {
    copy_m4_m4(r_viewmat, eye->viewmat);
  }
  else {
    copy_m4_m4(r_viewmat, xr->runtime->session_state.viewer_viewmat);
  }
  /* Since eye centroid does not have a focal length, just take it from selection eye. */
  *r_focal_len = eye->focal_len;
  *r_clip_start = xr->session_settings.clip_start;
  *r_clip_end = xr->session_settings.clip_end;

  return true;
}

bool WM_xr_session_state_controller_pose_location_get(const wmXrData *xr,
                                                      unsigned int subaction_idx,
                                                      float r_location[3])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set ||
      subaction_idx >= ARRAY_SIZE(xr->runtime->session_state.controllers)) {
    zero_v3(r_location);
    return false;
  }

  copy_v3_v3(r_location, xr->runtime->session_state.controllers[subaction_idx].pose.position);
  return true;
}

bool WM_xr_session_state_controller_pose_rotation_get(const wmXrData *xr,
                                                      unsigned int subaction_idx,
                                                      float r_rotation[4])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set ||
      subaction_idx >= ARRAY_SIZE(xr->runtime->session_state.controllers)) {
    unit_qt(r_rotation);
    return false;
  }

  copy_qt_qt(r_rotation,
             xr->runtime->session_state.controllers[subaction_idx].pose.orientation_quat);
  return true;
}

bool WM_xr_session_state_nav_location_get(const wmXrData *xr, float r_location[3])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    zero_v3(r_location);
    return false;
  }

  copy_v3_v3(r_location, xr->runtime->session_state.nav_pose.position);
  return true;
}

void WM_xr_session_state_nav_location_set(wmXrData *xr, const float location[3])
{
  if (WM_xr_session_exists(xr)) {
    copy_v3_v3(xr->runtime->session_state.nav_pose.position, location);
    xr->runtime->session_state.is_navigation_dirty = true;
  }
}

bool WM_xr_session_state_nav_rotation_get(const wmXrData *xr, float r_rotation[4])
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    unit_qt(r_rotation);
    return false;
  }

  copy_qt_qt(r_rotation, xr->runtime->session_state.nav_pose.orientation_quat);
  return true;
}

void WM_xr_session_state_nav_rotation_set(wmXrData *xr, const float rotation[4])
{
  if (WM_xr_session_exists(xr)) {
    BLI_ASSERT_UNIT_QUAT(rotation);
    copy_qt_qt(xr->runtime->session_state.nav_pose.orientation_quat, rotation);
    xr->runtime->session_state.is_navigation_dirty = true;
  }
}

bool WM_xr_session_state_nav_scale_get(const wmXrData *xr, float *r_scale)
{
  if (!WM_xr_session_is_ready(xr) || !xr->runtime->session_state.is_view_data_set) {
    *r_scale = 1.0f;
    return false;
  }

  *r_scale = xr->runtime->session_state.nav_scale;
  return true;
}

void WM_xr_session_state_nav_scale_set(wmXrData *xr, float scale)
{
  if (WM_xr_session_exists(xr)) {
    /* Clamp to reasonable values. */
    CLAMP(scale, 0.001f, 1000.0f);
    xr->runtime->session_state.nav_scale = scale;
    xr->runtime->session_state.is_navigation_dirty = true;
  }
}

void WM_xr_session_state_navigation_reset(wmXrSessionState *state)
{
  zero_v3(state->nav_pose.position);
  unit_qt(state->nav_pose.orientation_quat);
  state->nav_scale = 1.0f;
  state->is_navigation_dirty = true;
}

void WM_xr_session_state_viewer_object_get(const wmXrData *xr, Object *ob)
{
  if (WM_xr_session_exists(xr)) {
    wm_xr_session_object_pose_set(&xr->runtime->session_state.headset_object_orig_pose, ob);
  }
}

void WM_xr_session_state_viewer_object_set(wmXrData *xr, const Object *ob)
{
  if (WM_xr_session_exists(xr)) {
    wm_xr_session_object_pose_get(ob, &xr->runtime->session_state.headset_object_orig_pose);
  }
}

void WM_xr_session_state_controller_object_get(const wmXrData *xr,
                                               unsigned int subaction_idx,
                                               Object *ob)
{
  if (WM_xr_session_exists(xr)) {
    switch (subaction_idx) {
      case 0:
        wm_xr_session_object_pose_set(&xr->runtime->session_state.controller0_object_orig_pose,
                                      ob);
        break;
      case 1:
        wm_xr_session_object_pose_set(&xr->runtime->session_state.controller1_object_orig_pose,
                                      ob);
        break;
      default:
        BLI_assert(false);
        break;
    }
  }
}

void WM_xr_session_state_controller_object_set(wmXrData *xr,
                                               unsigned int subaction_idx,
                                               const Object *ob)
{
  if (WM_xr_session_exists(xr)) {
    switch (subaction_idx) {
      case 0:
        wm_xr_session_object_pose_get(ob,
                                      &xr->runtime->session_state.controller0_object_orig_pose);
        break;
      case 1:
        wm_xr_session_object_pose_get(ob,
                                      &xr->runtime->session_state.controller1_object_orig_pose);
        break;
      default:
        BLI_assert(false);
        break;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name XR-Session Actions
 *
 * XR action processing and event dispatching.
 *
 * \{ */

void wm_xr_session_actions_init(wmXrData *xr)
{
  if (!xr->runtime) {
    return;
  }

  GHOST_XrAttachActionSets(xr->runtime->context);
}

static void wm_xr_session_controller_mats_update(const bContext *C,
                                                 const XrSessionSettings *settings,
                                                 const wmXrAction *controller_pose_action,
                                                 wmXrSessionState *state,
                                                 wmWindow *win)
{
  const unsigned int count = (unsigned int)min_ii(
      (int)controller_pose_action->count_subaction_paths, (int)ARRAY_SIZE(state->controllers));

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  bScreen *screen_anim = ED_screen_animation_playing(wm);
  Object *ob_constraint = NULL;
  char ob_flag;
  float view_ofs[3], base_mat[4][4], nav_mat[4][4], m[4][4];

  if ((settings->flag & XR_SESSION_USE_POSITION_TRACKING) == 0) {
    copy_v3_v3(view_ofs, state->prev_local_pose.position);
  }
  else {
    zero_v3(view_ofs);
  }
  if ((settings->flag & XR_SESSION_USE_ABSOLUTE_TRACKING) == 0) {
    add_v3_v3(view_ofs, state->prev_eye_position_ofs);
  }

  wm_xr_pose_scale_to_mat(&state->prev_base_pose, state->prev_base_scale, base_mat);
  wm_xr_pose_scale_to_mat(&state->nav_pose, state->nav_scale, nav_mat);

  for (unsigned int i = 0; i < count; ++i) {
    wmXrControllerData *controller = &state->controllers[i];
    switch (i) {
      case 0:
        ob_constraint = settings->controller0_object;
        ob_flag = settings->controller0_flag;
        break;
      case 1:
        ob_constraint = settings->controller1_object;
        ob_flag = settings->controller1_flag;
        break;
      default:
        ob_constraint = NULL;
        ob_flag = 0;
        break;
    }

    /* Calculate controller matrix in world space. */
    wm_xr_pose_to_mat(&((GHOST_XrPose *)controller_pose_action->states)[i], m);

    /* Apply eye position offset. */
    sub_v3_v3(m[3], view_ofs);

    /* Apply base pose and navigation. */
    mul_m4_m4m4(controller->mat_base, base_mat, m);
    mul_m4_m4m4(controller->mat, nav_mat, controller->mat_base);

    /* Save final pose. */
    mat4_to_loc_quat(
        controller->pose.position, controller->pose.orientation_quat, controller->mat);

    if (ob_constraint && ((ob_flag & XR_OBJECT_ENABLE) != 0)) {
      wm_xr_session_object_pose_set(&controller->pose, ob_constraint);

      if (((ob_flag & XR_OBJECT_AUTOKEY) != 0) && screen_anim &&
          autokeyframe_cfra_can_key(scene, &ob_constraint->id)) {
        wm_xr_session_object_autokey(
            (bContext *)C, scene, view_layer, win, ob_constraint, (i == 0) ? true : false);
      }
    }
  }
}

static const GHOST_XrPose *wm_xr_session_controller_pose_find(const wmXrSessionState *state,
                                                              const char *subaction_path)
{
  for (unsigned int i = 0; i < (unsigned int)ARRAY_SIZE(state->controllers); ++i) {
    if (STREQ(state->controllers[i].subaction_path, subaction_path)) {
      return &state->controllers[i].pose;
    }
  }

  return NULL;
}

BLI_INLINE bool test_float_state(const float *state, float threshold, eXrActionFlag flag)
{
  if ((flag & XR_ACTION_AXIS0_POS) != 0) {
    if (*state > threshold) {
      return true;
    }
  }
  else if ((flag & XR_ACTION_AXIS0_NEG) != 0) {
    if (*state < -threshold) {
      return true;
    }
  }
  else {
    if (fabsf(*state) > threshold) {
      return true;
    }
  }
  return false;
}

BLI_INLINE bool test_vec2f_state(const float state[2], float threshold, eXrActionFlag flag)
{
  if ((flag & XR_ACTION_AXIS0_POS) != 0) {
    if (state[0] < 0.0f) {
      return false;
    }
  }
  else if ((flag & XR_ACTION_AXIS0_NEG) != 0) {
    if (state[0] > 0.0f) {
      return false;
    }
  }
  if ((flag & XR_ACTION_AXIS1_POS) != 0) {
    if (state[1] < 0.0f) {
      return false;
    }
  }
  else if ((flag & XR_ACTION_AXIS1_NEG) != 0) {
    if (state[1] > 0.0f) {
      return false;
    }
  }
  return (len_v2(state) > threshold);
}

static bool wm_xr_session_modal_action_test(const ListBase *active_modal_actions,
                                            const wmXrAction *action,
                                            bool *r_found)
{
  if (r_found) {
    *r_found = false;
  }

  LISTBASE_FOREACH (LinkData *, ld, active_modal_actions) {
    wmXrAction *active_modal_action = ld->data;
    if (action == active_modal_action) {
      if (r_found) {
        *r_found = true;
      }
      return true;
    }
    if (action->ot == active_modal_action->ot &&
        IDP_EqualsProperties(action->op_properties, active_modal_action->op_properties)) {
      /* Don't allow duplicate modal operators since this can lead to unwanted modal handler
       * behavior. */
      return false;
    }
  }

  return true;
}

static void wm_xr_session_modal_action_test_add(ListBase *active_modal_actions,
                                                const wmXrAction *action)
{
  bool found;
  if (wm_xr_session_modal_action_test(active_modal_actions, action, &found) && !found) {
    LinkData *ld = MEM_callocN(sizeof(LinkData), __func__);
    ld->data = (void *)action;
    BLI_addtail(active_modal_actions, ld);
  }
}

static void wm_xr_session_modal_action_remove(ListBase *active_modal_actions,
                                              const wmXrAction *action)
{
  LISTBASE_FOREACH (LinkData *, ld, active_modal_actions) {
    if (action == ld->data) {
      BLI_freelinkN(active_modal_actions, ld);
      return;
    }
  }
}

static wmXrHapticAction *wm_xr_session_haptic_action_find(ListBase *active_haptic_actions,
                                                          const wmXrAction *action,
                                                          const char **subaction_path)
{
  LISTBASE_FOREACH (wmXrHapticAction *, ha, active_haptic_actions) {
    if (action == ha->action && subaction_path == ha->subaction_path) {
      return ha;
    }
  }

  return NULL;
}

static void wm_xr_session_haptic_action_add(ListBase *active_haptic_actions,
                                            const wmXrAction *action,
                                            const char **subaction_path,
                                            long long time_now)
{
  wmXrHapticAction *ha = wm_xr_session_haptic_action_find(
      active_haptic_actions, action, subaction_path);
  if (ha) {
    /* Reset start time since OpenXR restarts haptics if they are already active. */
    ha->time_start = time_now;
  }
  else {
    ha = MEM_callocN(sizeof(wmXrHapticAction), __func__);
    ha->action = (wmXrAction *)action;
    ha->subaction_path = subaction_path;
    ha->time_start = time_now;
    BLI_addtail(active_haptic_actions, ha);
  }
}

static void wm_xr_session_haptic_action_remove(ListBase *active_haptic_actions,
                                               const wmXrAction *action)
{
  LISTBASE_FOREACH (wmXrHapticAction *, ha, active_haptic_actions) {
    if (action == ha->action) {
      BLI_freelinkN(active_haptic_actions, ha);
      return;
    }
  }
}

static void wm_xr_session_haptic_timers_check(ListBase *active_haptic_actions, long long time_now)
{
  wmXrHapticAction *ha = active_haptic_actions->first;
  while (ha) {
    if (time_now - ha->time_start >= ha->action->haptic_duration) {
      wmXrHapticAction *_ha = ha;
      ha = ha->next;
      BLI_freelinkN(active_haptic_actions, _ha);
    }
    else {
      ha = ha->next;
    }
  }
}

static void wm_xr_session_action_states_interpret(wmXrData *xr,
                                                  const char *action_set_name,
                                                  wmXrAction *action,
                                                  unsigned int subaction_idx,
                                                  ListBase *active_modal_actions,
                                                  ListBase *active_haptic_actions,
                                                  long long time_now,
                                                  bool modal,
                                                  bool haptic,
                                                  short *r_val,
                                                  bool *r_press_start)
{
  const char **haptic_subaction_path = ((action->flag & XR_ACTION_HAPTIC_MATCHUSERPATHS) != 0) ?
                                           (const char **)&action->subaction_paths[subaction_idx] :
                                           NULL;
  bool curr = false;
  bool prev = false;

  switch (action->type) {
    case XR_BOOLEAN_INPUT: {
      const bool *state = &((bool *)action->states)[subaction_idx];
      bool *state_prev = &((bool *)action->states_prev)[subaction_idx];
      if (*state) {
        curr = true;
      }
      if (*state_prev) {
        prev = true;
      }
      *state_prev = *state;
      break;
    }
    case XR_FLOAT_INPUT: {
      const float *state = &((float *)action->states)[subaction_idx];
      float *state_prev = &((float *)action->states_prev)[subaction_idx];
      if (test_float_state(state, action->float_threshold, action->flag)) {
        curr = true;
      }
      if (test_float_state(state_prev, action->float_threshold, action->flag)) {
        prev = true;
      }
      *state_prev = *state;
      break;
    }
    case XR_VECTOR2F_INPUT: {
      const float(*state)[2] = &((float(*)[2])action->states)[subaction_idx];
      float(*state_prev)[2] = &((float(*)[2])action->states_prev)[subaction_idx];
      if (test_vec2f_state(*state, action->float_threshold, action->flag)) {
        curr = true;
      }
      if (test_vec2f_state(*state_prev, action->float_threshold, action->flag)) {
        prev = true;
      }
      copy_v2_v2(*state_prev, *state);
      break;
    }
    case XR_POSE_INPUT:
    case XR_VIBRATION_OUTPUT:
      BLI_assert_unreachable();
      break;
  }

  if (curr) {
    if (!prev) {
      if (modal || (action->flag & XR_ACTION_PRESS) != 0) {
        *r_val = KM_PRESS;
        *r_press_start = true;
      }
      if (haptic && (action->flag & (XR_ACTION_HAPTIC_PRESS | XR_ACTION_HAPTIC_REPEAT)) != 0) {
        /* Apply haptics. */
        if (WM_xr_haptic_action_apply(xr,
                                      action_set_name,
                                      action->haptic_name,
                                      haptic_subaction_path,
                                      &action->haptic_duration,
                                      &action->haptic_frequency,
                                      &action->haptic_amplitude)) {
          wm_xr_session_haptic_action_add(
              active_haptic_actions, action, haptic_subaction_path, time_now);
        }
      }
    }
    else if (modal) {
      *r_val = KM_PRESS;
    }
    if (modal && !action->active_modal_path) {
      /* Set active modal path. */
      action->active_modal_path = &action->subaction_paths[subaction_idx];
      *r_press_start = true;
      /* Add to active modal actions. */
      wm_xr_session_modal_action_test_add(active_modal_actions, action);
    }
    if (haptic && ((action->flag & XR_ACTION_HAPTIC_REPEAT) != 0)) {
      if (!wm_xr_session_haptic_action_find(
              active_haptic_actions, action, haptic_subaction_path)) {
        /* Apply haptics. */
        if (WM_xr_haptic_action_apply(xr,
                                      action_set_name,
                                      action->haptic_name,
                                      haptic_subaction_path,
                                      &action->haptic_duration,
                                      &action->haptic_frequency,
                                      &action->haptic_amplitude)) {
          wm_xr_session_haptic_action_add(
              active_haptic_actions, action, haptic_subaction_path, time_now);
        }
      }
    }
  }
  else if (prev) {
    if (modal || (action->flag & XR_ACTION_RELEASE) != 0) {
      *r_val = KM_RELEASE;
      if (modal && (&action->subaction_paths[subaction_idx] == action->active_modal_path)) {
        /* Unset active modal path. */
        action->active_modal_path = NULL;
        /* Remove from active modal actions. */
        wm_xr_session_modal_action_remove(active_modal_actions, action);
      }
    }
    if (haptic) {
      if ((action->flag & XR_ACTION_HAPTIC_RELEASE) != 0) {
        /* Apply haptics. */
        if (WM_xr_haptic_action_apply(xr,
                                      action_set_name,
                                      action->haptic_name,
                                      haptic_subaction_path,
                                      &action->haptic_duration,
                                      &action->haptic_frequency,
                                      &action->haptic_amplitude)) {
          wm_xr_session_haptic_action_add(
              active_haptic_actions, action, haptic_subaction_path, time_now);
        }
      }
      else if ((action->flag & XR_ACTION_HAPTIC_REPEAT) != 0) {
        /* Stop any active haptics. */
        WM_xr_haptic_action_stop(xr, action_set_name, action->haptic_name, haptic_subaction_path);
        wm_xr_session_haptic_action_remove(active_haptic_actions, action);
      }
    }
  }
}

static bool wm_xr_session_action_test_bimanual(const wmXrSessionState *session_state,
                                               wmXrAction *action,
                                               unsigned int subaction_idx,
                                               unsigned int *r_subaction_idx_other,
                                               const GHOST_XrPose **r_pose_other)
{
  if ((action->flag & XR_ACTION_BIMANUAL) == 0) {
    return false;
  }

  bool bimanual = false;

  *r_subaction_idx_other = (subaction_idx == 0) ?
                               (unsigned int)min_ii(1, action->count_subaction_paths - 1) :
                               0;

  switch (action->type) {
    case XR_BOOLEAN_INPUT: {
      const bool *state = &((bool *)action->states)[*r_subaction_idx_other];
      if (*state) {
        bimanual = true;
      }
      break;
    }
    case XR_FLOAT_INPUT: {
      const float *state = &((float *)action->states)[*r_subaction_idx_other];
      if (test_float_state(state, action->float_threshold, action->flag)) {
        bimanual = true;
      }
      break;
    }
    case XR_VECTOR2F_INPUT: {
      const float(*state)[2] = &((float(*)[2])action->states)[*r_subaction_idx_other];
      if (test_vec2f_state(*state, action->float_threshold, action->flag)) {
        bimanual = true;
      }
      break;
    }
    case XR_POSE_INPUT:
    case XR_VIBRATION_OUTPUT:
      BLI_assert_unreachable();
      break;
  }

  if (bimanual) {
    *r_pose_other = wm_xr_session_controller_pose_find(
        session_state, action->subaction_paths[*r_subaction_idx_other]);
  }

  return bimanual;
}

/* Dispatch events to XR surface / window queues. */
static void wm_xr_session_events_dispatch(wmXrData *xr,
                                          const XrSessionSettings *settings,
                                          GHOST_XrContextHandle xr_context,
                                          wmXrActionSet *action_set,
                                          wmXrSessionState *session_state,
                                          wmSurface *surface,
                                          wmWindow *win)
{
  const char *action_set_name = action_set->name;

  const unsigned int count = GHOST_XrGetActionCount(xr_context, action_set_name);
  if (count < 1) {
    return;
  }

  const long long time_now = (long long)(PIL_check_seconds_timer() * 1000);

  const wmXrEyeData *eye_data = &session_state->eyes[settings->selection_eye];
  ListBase *active_modal_actions = &action_set->active_modal_actions;
  ListBase *active_haptic_actions = &action_set->active_haptic_actions;

  wmXrAction **actions = MEM_calloc_arrayN(count, sizeof(*actions), __func__);

  GHOST_XrGetActionCustomdatas(xr_context, action_set_name, (void **)actions);

  /* Check haptic action timers. */
  wm_xr_session_haptic_timers_check(active_haptic_actions, time_now);

  for (unsigned int action_idx = 0; action_idx < count; ++action_idx) {
    wmXrAction *action = actions[action_idx];
    if (action && action->ot) {
      const bool modal = (action->ot->modal || action->ot->modal_3d);
      const bool haptic = (GHOST_XrGetActionCustomdata(
                               xr_context, action_set_name, action->haptic_name) != NULL);

      for (unsigned int subaction_idx = 0; subaction_idx < action->count_subaction_paths;
           ++subaction_idx) {
        short val = KM_NOTHING;
        bool press_start = false;

        /* Interpret action states (update modal/haptic action lists, apply haptics, etc). */
        wm_xr_session_action_states_interpret(xr,
                                              action_set_name,
                                              action,
                                              subaction_idx,
                                              active_modal_actions,
                                              active_haptic_actions,
                                              time_now,
                                              modal,
                                              haptic,
                                              &val,
                                              &press_start);

        if ((val != KM_NOTHING) &&
            (!modal || (wm_xr_session_modal_action_test(active_modal_actions, action, NULL) &&
                        (!action->active_modal_path || (&action->subaction_paths[subaction_idx] ==
                                                        action->active_modal_path))))) {
          const GHOST_XrPose *pose = wm_xr_session_controller_pose_find(
              session_state, action->subaction_paths[subaction_idx]);
          const GHOST_XrPose *pose_other = NULL;
          unsigned int subaction_idx_other;

          /* Test for bimanual interaction. */
          const bool bimanual = wm_xr_session_action_test_bimanual(
              session_state, action, subaction_idx, &subaction_idx_other, &pose_other);

          wm_event_add_xrevent(action_set_name,
                               action,
                               pose,
                               pose_other,
                               eye_data,
                               surface,
                               win,
                               subaction_idx,
                               bimanual ? subaction_idx_other : subaction_idx,
                               val,
                               press_start);
        }
      }
    }
  }

  MEM_freeN(actions);
}

void wm_xr_session_actions_update(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmXrData *xr = &wm->xr;
  if (!xr->runtime) {
    return;
  }

  const XrSessionSettings *settings = &xr->session_settings;
  wmWindow *win = wm_xr_session_root_window_or_fallback_get(wm, xr->runtime);
  GHOST_XrContextHandle xr_context = xr->runtime->context;
  wmXrSessionState *state = &xr->runtime->session_state;
  wmXrActionSet *active_action_set = state->active_action_set;
  Object *ob_constraint = settings->headset_object;
  char ob_flag = settings->headset_flag;

  if (state->is_navigation_dirty) {
    memcpy(&state->nav_pose_prev, &state->nav_pose, sizeof(state->nav_pose_prev));
    state->nav_scale_prev = state->nav_scale;
    state->is_navigation_dirty = false;

    /* Update viewer/eye poses with any navigation changes since the last actions sync so that data
     * is correct for queries. */
    float m[4][4], viewer_mat[4][4];
    wm_xr_pose_scale_to_mat(&state->nav_pose, state->nav_scale, m);
    mul_m4_m4m4(viewer_mat, m, state->viewer_mat_base);
    mat4_to_loc_quat(state->viewer_pose.position, state->viewer_pose.orientation_quat, viewer_mat);
    wm_xr_pose_scale_to_imat(
        &state->viewer_pose, settings->base_scale * state->nav_scale, state->viewer_viewmat);

    wm_xr_pose_scale_to_imat(&state->nav_pose, state->nav_scale, m);
    for (unsigned int i = 0; i < (unsigned int)ARRAY_SIZE(state->eyes); ++i) {
      mul_m4_m4m4(state->eyes[i].viewmat, state->eyes[i].viewmat_base, m);
    }
  }

  /* Update headset constraint object (if any). */
  if (ob_constraint && ((ob_flag & XR_OBJECT_ENABLE) != 0)) {
    wm_xr_session_object_pose_set(&state->viewer_pose, ob_constraint);

    if ((ob_flag & XR_OBJECT_AUTOKEY) != 0) {
      bScreen *screen_anim = ED_screen_animation_playing(wm);
      if (screen_anim) {
        Scene *scene = CTX_data_scene(C);
        if (autokeyframe_cfra_can_key(scene, &ob_constraint->id)) {
          ViewLayer *view_layer = CTX_data_view_layer(C);
          wm_xr_session_object_autokey((bContext *)C, scene, view_layer, win, ob_constraint, true);
        }
      }
    }
  }

  int ret = GHOST_XrSyncActions(xr_context, active_action_set ? active_action_set->name : NULL);
  if (!ret) {
    return;
  }

  wmSurface *surface = (g_xr_surface && g_xr_surface->customdata) ? g_xr_surface : NULL;

  /* Only update controller mats and dispatch events for active action set. */
  if (active_action_set) {
    if (active_action_set->controller_pose_action) {
      wm_xr_session_controller_mats_update(
          C, &xr->session_settings, active_action_set->controller_pose_action, state, win);
    }

    if (surface && win) {
      wm_xr_session_events_dispatch(
          xr, settings, xr_context, active_action_set, state, surface, win);
    }
  }
}

void wm_xr_session_actions_uninit(wmXrData *xr)
{
  wm_xr_session_controller_data_clear(&xr->runtime->session_state);
}

void wm_xr_session_controller_data_populate(const wmXrAction *controller_pose_action, wmXrData *xr)
{
  wmXrSessionState *state = &xr->runtime->session_state;

  const unsigned int count = (unsigned int)min_ii(
      (int)ARRAY_SIZE(state->controllers), (int)controller_pose_action->count_subaction_paths);

  for (unsigned int i = 0; i < count; ++i) {
    wmXrControllerData *c = &state->controllers[i];
    strcpy(c->subaction_path, controller_pose_action->subaction_paths[i]);
    memset(&c->pose, 0, sizeof(c->pose));
    zero_m4(c->mat);
  }

  /* Activate draw callback. */
  if (g_xr_surface) {
    wmXrSurfaceData *surface_data = g_xr_surface->customdata;
    if (surface_data && !surface_data->controller_draw_handle) {
      if (surface_data->controller_art) {
        surface_data->controller_draw_handle = ED_region_draw_cb_activate(
            surface_data->controller_art, wm_xr_draw_controllers, xr, REGION_DRAW_POST_VIEW);
      }
    }
  }
}

void wm_xr_session_controller_data_clear(wmXrSessionState *state)
{
  memset(state->controllers, 0, sizeof(state->controllers));

  /* Deactivate draw callback. */
  if (g_xr_surface) {
    wmXrSurfaceData *surface_data = g_xr_surface->customdata;
    if (surface_data && surface_data->controller_draw_handle) {
      if (surface_data->controller_art) {
        ED_region_draw_cb_exit(surface_data->controller_art, surface_data->controller_draw_handle);
      }
      surface_data->controller_draw_handle = NULL;
    }
  }
}

void wm_xr_session_object_autokey(
    bContext *C, Scene *scene, ViewLayer *view_layer, wmWindow *win, Object *ob, bool notify)
{
  /* Poll functions in keyingsets_utils.py require an active window and object. */
  wmWindow *win_prev = win ? CTX_wm_window(C) : NULL;
  if (win) {
    CTX_wm_window_set(C, win);
  }

  Object *obact = CTX_data_active_object(C);
  if (!obact) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base) {
      ED_object_base_select(base, BA_SELECT);
      ED_object_base_activate(C, base);
    }
  }

  bScreen *screen = CTX_wm_screen(C);
  if (screen && screen->animtimer && (IS_AUTOKEY_FLAG(scene, INSERTAVAIL) == 0) &&
      ((scene->toolsettings->autokey_flag & ANIMRECORD_FLAG_WITHNLA) != 0)) {
    TransInfo t;
    t.scene = scene;
    t.animtimer = screen->animtimer;
    animrecord_check_state(&t, ob);
  }

  autokeyframe_object(C, scene, view_layer, ob, TFM_TRANSLATION);
  if (IS_AUTOKEY_FLAG(scene, INSERTNEEDED)) {
    autokeyframe_object(C, scene, view_layer, ob, TFM_ROTATION);
  }

  if (motionpath_need_update_object(scene, ob)) {
    ED_objects_recalculate_paths(C, scene, OBJECT_PATH_CALC_RANGE_CURRENT_FRAME);
  }

  if (notify) {
    WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, NULL);
    WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
  }

  if (win) {
    CTX_wm_window_set(C, win_prev);
  }
}

/** \} */ /* XR-Session Actions */

/* -------------------------------------------------------------------- */
/** \name XR-Session Surface
 *
 * A wmSurface is used to manage drawing of the VR viewport. It's created and destroyed with the
 * session.
 *
 * \{ */

/**
 * \brief Call Ghost-XR to draw a frame
 *
 * Draw callback for the XR-session surface. It's expected to be called on each main loop
 * iteration and tells Ghost-XR to submit a new frame by drawing its views. Note that for drawing
 * each view, #wm_xr_draw_view() will be called through Ghost-XR (see GHOST_XrDrawViewFunc()).
 */
static void wm_xr_session_surface_draw(bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Main *bmain = CTX_data_main(C);
  wmXrDrawData draw_data;

  if (!GHOST_XrSessionIsRunning(wm->xr.runtime->context)) {
    return;
  }

  Scene *scene;
  Depsgraph *depsgraph;
  wm_xr_session_scene_and_evaluated_depsgraph_get(bmain, wm, &scene, &depsgraph);
  wm_xr_session_draw_data_populate(&wm->xr, scene, depsgraph, &draw_data);

  GHOST_XrSessionDrawViews(wm->xr.runtime->context, &draw_data);

  GPU_framebuffer_restore();
}

bool wm_xr_session_surface_offscreen_ensure(wmXrSurfaceData *surface_data,
                                            const GHOST_XrDrawViewInfo *draw_view)
{
  const unsigned int view_idx = (unsigned int)min_ii((int)ARRAY_SIZE(surface_data->offscreen),
                                                     (int)draw_view->view_idx);
  GPUOffScreen *offscreen = surface_data->offscreen[view_idx];
  GPUViewport *viewport = surface_data->viewport[view_idx];
  const bool size_changed = offscreen && (GPU_offscreen_width(offscreen) != draw_view->width) &&
                            (GPU_offscreen_height(offscreen) != draw_view->height);
  char err_out[256] = "unknown";
  bool failure = false;

  if (offscreen) {
    BLI_assert(viewport);

    if (!size_changed) {
      return true;
    }
    GPU_viewport_free(viewport);
    GPU_offscreen_free(offscreen);
  }

  offscreen = surface_data->offscreen[view_idx] = GPU_offscreen_create(
      draw_view->width, draw_view->height, true, false, err_out);
  if (offscreen) {
    viewport = surface_data->viewport[view_idx] = GPU_viewport_create();
    if (!viewport) {
      GPU_offscreen_free(offscreen);
      offscreen = surface_data->offscreen[view_idx] = NULL;
      failure = true;
    }
  }
  else {
    failure = true;
  }

  if (failure) {
    CLOG_ERROR(&LOG, "Failed to get buffer, %s\n", err_out);
    return false;
  }

  return true;
}

static void wm_xr_session_surface_free_data(wmSurface *surface)
{
  wmXrSurfaceData *data = surface->customdata;

  for (unsigned int i = 0; i < (unsigned int)ARRAY_SIZE(data->offscreen); ++i) {
    if (data->viewport[i]) {
      GPU_viewport_free(data->viewport[i]);
    }
    if (data->offscreen[i]) {
      GPU_offscreen_free(data->offscreen[i]);
    }
  }

  if (data->controller_art) {
    BLI_freelistN(&data->controller_art->drawcalls);
    MEM_freeN(data->controller_art);
  }

  MEM_freeN(surface->customdata);

  g_xr_surface = NULL;
}

static wmSurface *wm_xr_session_surface_create(void)
{
  if (g_xr_surface) {
    BLI_assert(false);
    return g_xr_surface;
  }

  wmSurface *surface = MEM_callocN(sizeof(*surface), __func__);
  wmXrSurfaceData *data = MEM_callocN(sizeof(*data), "XrSurfaceData");
  data->controller_art = MEM_callocN(sizeof(*(data->controller_art)), "XrControllerRegionType");

  surface->draw = wm_xr_session_surface_draw;
  surface->free_data = wm_xr_session_surface_free_data;
  surface->activate = DRW_xr_drawing_begin;
  surface->deactivate = DRW_xr_drawing_end;

  surface->ghost_ctx = DRW_xr_opengl_context_get();
  surface->gpu_ctx = DRW_xr_gpu_context_get();
  surface->is_xr = true;

  data->controller_art->regionid = RGN_TYPE_XR;
  surface->customdata = data;

  g_xr_surface = surface;

  return surface;
}

void *wm_xr_session_gpu_binding_context_create(void)
{
  wmSurface *surface = wm_xr_session_surface_create();

  wm_surface_add(surface);

  /* Some regions may need to redraw with updated session state after the session is entirely up
   * and running. */
  WM_main_add_notifier(NC_WM | ND_XR_DATA_CHANGED, NULL);

  return surface->ghost_ctx;
}

void wm_xr_session_gpu_binding_context_destroy(GHOST_ContextHandle UNUSED(context))
{
  if (g_xr_surface) { /* Might have been freed already */
    wm_surface_remove(g_xr_surface);
  }

  wm_window_reset_drawable();

  /* Some regions may need to redraw with updated session state after the session is entirely
   * stopped. */
  WM_main_add_notifier(NC_WM | ND_XR_DATA_CHANGED, NULL);
}

ARegionType *WM_xr_surface_controller_region_type_get(void)
{
  if (g_xr_surface) {
    wmXrSurfaceData *data = g_xr_surface->customdata;
    return data->controller_art;
  }

  return NULL;
}

/** \} */ /* XR-Session Surface */
