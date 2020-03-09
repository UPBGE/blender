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
 * Contributor(s): Mitchell Stokes.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BL_Action.cpp
 *  \ingroup ketsji
 */

#include "CM_Message.h"

#include "BL_Action.h"
#include "BL_ArmatureObject.h"
#include "KX_IpoConvert.h"
#include "KX_GameObject.h"
#include "KX_Globals.h"

#include "RAS_MeshObject.h"

#include "SG_Controller.h"

// These three are for getting the action from the logic manager
#include "KX_Scene.h"
#include "KX_BlenderConverter.h"
#include "SCA_LogicManager.h"

extern "C" {
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_context.h"

#include "BKE_layer.h"
#include "BKE_scene.h"

#include "RNA_access.h"
#include "RNA_define.h"

// Needed for material IPOs
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "DNA_anim_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "depsgraph/DEG_depsgraph.h"
#include "depsgraph/DEG_depsgraph_query.h"
}

#include "MEM_guardedalloc.h"
#include "BKE_library.h"
#include "BKE_global.h"

BL_Action::BL_Action(class KX_GameObject *gameobj)
    : m_action(nullptr),
      m_blendpose(nullptr),
      m_blendinpose(nullptr),
      m_obj(gameobj),
      m_startframe(0.f),
      m_endframe(0.f),
      m_localframe(0.f),
      m_blendin(0.f),
      m_blendframe(0.f),
      m_blendstart(0.f),
      m_speed(0.f),
      m_priority(0),
      m_playmode(ACT_MODE_PLAY),
      m_blendmode(ACT_BLEND_BLEND),
      m_ipo_flags(0),
      m_done(true),
      m_appliedToObject(true),
      m_requestIpo(false),
      m_calc_localtime(true),
      m_prevUpdate(-1.0f)
{
}

BL_Action::~BL_Action()
{
  if (m_blendpose)
    BKE_pose_free(m_blendpose);
  if (m_blendinpose)
    BKE_pose_free(m_blendinpose);
  ClearControllerList();

  Object *ob = m_obj->GetBlenderObject();
  if (ob && ob->adt && m_action) {
    ob->adt->action = m_action;
  }
}

void BL_Action::ClearControllerList()
{
  // Clear out the controller list
  std::vector<SG_Controller *>::iterator it;
  for (it = m_sg_contr_list.begin(); it != m_sg_contr_list.end(); it++) {
    m_obj->GetSGNode()->RemoveSGController((*it));
    delete *it;
  }

  m_sg_contr_list.clear();
}

bool BL_Action::Play(const std::string &name,
                     float start,
                     float end,
                     short priority,
                     float blendin,
                     short play_mode,
                     float layer_weight,
                     short ipo_flags,
                     float playback_speed,
                     short blend_mode)
{

  // Only start playing a new action if we're done, or if
  // the new action has a higher priority
  if (!IsDone() && priority > m_priority)
    return false;
  m_priority = priority;
  bAction *prev_action = m_action;

  KX_Scene *kxscene = m_obj->GetScene();

  // First try to load the action
  m_action = (bAction *)kxscene->GetLogicManager()->GetActionByName(name);
  if (!m_action) {
    CM_Error("failed to load action: " << name);
    m_done = true;
    return false;
  }

  // If we have the same settings, don't play again
  // This is to resolve potential issues with pulses on sensors such as the ones
  // reported in bug #29412. The fix is here so it works for both logic bricks and Python.
  // However, this may eventually lead to issues where a user wants to override an already
  // playing action with the same action and settings. If this becomes an issue,
  // then this fix may have to be re-evaluated.
  if (!IsDone() && m_action == prev_action && m_startframe == start && m_endframe == end &&
      m_priority == priority && m_speed == playback_speed)
    return false;

  // First get rid of any old controllers
  ClearControllerList();

  // Create an SG_Controller
  SG_Controller *sg_contr = BL_CreateIPO(m_action, m_obj, kxscene);
  m_sg_contr_list.push_back(sg_contr);
  m_obj->GetSGNode()->AddSGController(sg_contr);
  sg_contr->SetNode(m_obj->GetSGNode());

  // Try obcolor
  sg_contr = BL_CreateObColorIPO(m_action, m_obj, kxscene);
  if (sg_contr) {
    m_sg_contr_list.push_back(sg_contr);
    m_obj->GetSGNode()->AddSGController(sg_contr);
    sg_contr->SetNode(m_obj->GetSGNode());
  }

  // Now try materials
  for (unsigned short i = 0, meshcount = m_obj->GetMeshCount(); i < meshcount; ++i) {
    RAS_MeshObject *mesh = m_obj->GetMesh(i);
    for (unsigned short j = 0, matcount = mesh->NumMaterials(); j < matcount; ++j) {
      RAS_MeshMaterial *meshmat = mesh->GetMeshMaterial(j);
      RAS_IPolyMaterial *polymat = meshmat->GetBucket()->GetPolyMaterial();

      sg_contr = BL_CreateMaterialIpo(m_action, polymat, m_obj, kxscene);
      if (sg_contr) {
        m_sg_contr_list.push_back(sg_contr);
        m_obj->GetSGNode()->AddSGController(sg_contr);
        sg_contr->SetNode(m_obj->GetSGNode());
      }
    }
  }

  // Extra controllers
  if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_LIGHT) {
    sg_contr = BL_CreateLampIPO(m_action, m_obj, kxscene);
    m_sg_contr_list.push_back(sg_contr);
    m_obj->GetSGNode()->AddSGController(sg_contr);
    sg_contr->SetNode(m_obj->GetSGNode());
  }
  else if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_CAMERA) {
    sg_contr = BL_CreateCameraIPO(m_action, m_obj, kxscene);
    m_sg_contr_list.push_back(sg_contr);
    m_obj->GetSGNode()->AddSGController(sg_contr);
    sg_contr->SetNode(m_obj->GetSGNode());
  }

  m_ipo_flags = ipo_flags;
  InitIPO();

  // Setup blendin shapes/poses
  if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE) {
    BL_ArmatureObject *obj = (BL_ArmatureObject *)m_obj;
    obj->GetPose(&m_blendinpose);
  }
  else {
  }

  // Now that we have an action, we have something we can play
  m_starttime = KX_GetActiveEngine()->GetFrameTime();
  m_startframe = m_localframe = start;
  m_endframe = end;
  m_blendin = blendin;
  m_playmode = play_mode;
  m_blendmode = blend_mode;
  m_blendframe = 0.f;
  m_blendstart = 0.f;
  m_speed = playback_speed;
  m_layer_weight = layer_weight;

  m_done = false;
  m_appliedToObject = false;
  m_requestIpo = false;

  m_prevUpdate = -1.0f;

  return true;
}

bool BL_Action::IsDone()
{
  return m_done;
}

void BL_Action::InitIPO()
{
  // Initialize the IPOs
  std::vector<SG_Controller *>::iterator it;
  for (it = m_sg_contr_list.begin(); it != m_sg_contr_list.end(); it++) {
    (*it)->SetOption(SG_Controller::SG_CONTR_IPO_RESET, true);
    (*it)->SetOption(SG_Controller::SG_CONTR_IPO_IPO_AS_FORCE, m_ipo_flags & ACT_IPOFLAG_FORCE);
    (*it)->SetOption(SG_Controller::SG_CONTR_IPO_IPO_ADD, m_ipo_flags & ACT_IPOFLAG_ADD);
    (*it)->SetOption(SG_Controller::SG_CONTR_IPO_LOCAL, m_ipo_flags & ACT_IPOFLAG_LOCAL);
  }
}

bAction *BL_Action::GetAction()
{
  return (IsDone()) ? nullptr : m_action;
}

float BL_Action::GetFrame()
{
  return m_localframe;
}

const std::string BL_Action::GetName()
{
  if (m_action != nullptr) {
    return m_action->id.name + 2;
  }
  else {
    return "";
  }
}

void BL_Action::SetFrame(float frame)
{
  // Clamp the frame to the start and end frame
  if (frame < std::min(m_startframe, m_endframe))
    frame = std::min(m_startframe, m_endframe);
  else if (frame > std::max(m_startframe, m_endframe))
    frame = std::max(m_startframe, m_endframe);

  m_localframe = frame;
  m_calc_localtime = false;
}

void BL_Action::SetPlayMode(short play_mode)
{
  m_playmode = play_mode;
}

void BL_Action::SetLocalTime(float curtime)
{
  float dt = (curtime - m_starttime) * (float)KX_GetActiveEngine()->GetAnimFrameRate() * m_speed;

  if (m_endframe < m_startframe)
    dt = -dt;

  m_localframe = m_startframe + dt;
}

void BL_Action::ResetStartTime(float curtime)
{
  float dt = (m_localframe > m_startframe) ? m_localframe - m_startframe :
                                             m_startframe - m_localframe;

  m_starttime = curtime - dt / ((float)KX_GetActiveEngine()->GetAnimFrameRate() * m_speed);
  SetLocalTime(curtime);
}

void BL_Action::IncrementBlending(float curtime)
{
  // Setup m_blendstart if we need to
  if (m_blendstart == 0.f)
    m_blendstart = curtime;

  // Bump the blend frame
  m_blendframe = (curtime - m_blendstart) * (float)KX_GetActiveEngine()->GetAnimFrameRate();

  // Clamp
  if (m_blendframe > m_blendin)
    m_blendframe = m_blendin;
}

void BL_Action::BlendShape(Key *key, float srcweight, std::vector<float> &blendshape)
{
}

/* Look at object_transform for original function */
static void ignore_parent_tx_bge(Main *bmain, Depsgraph *depsgraph, KX_Scene *kxscene, Object *ob)
{
  Object workob;
  Object *ob_child;

  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);

  /* a change was made, adjust the children to compensate */
  for (KX_GameObject *gameobj : kxscene->GetObjectList()) {
    if (gameobj->GetBlenderObject()->parent == ob) {
      ob_child = gameobj->GetBlenderObject();
      Object *ob_child_eval = DEG_get_evaluated_object(depsgraph, ob_child);
      BKE_object_apply_mat4(ob_child_eval, ob_child_eval->obmat, true, false);
      BKE_object_workob_calc_parent(depsgraph, kxscene->GetBlenderScene(), ob_child_eval, &workob);
      invert_m4_m4(ob_child->parentinv, workob.obmat);
      /* Copy result of BKE_object_apply_mat4(). */
      BKE_object_transform_copy(ob_child, ob_child_eval);
      /* Make sure evaluated object is in a consistent state with the original one.
       * It might be needed for applying transform on its children. */
      copy_m4_m4(ob_child_eval->parentinv, ob_child->parentinv);
      BKE_object_eval_transform_all(depsgraph, scene_eval, ob_child_eval);
      /* Tag for update.
       * This is because parent matrix did change, so in theory the child object might now be
       * evaluated to a different location in another editing context. */
      DEG_id_tag_update(&ob_child->id, ID_RECALC_TRANSFORM);
    }
  }
}

void BL_Action::Update(float curtime, bool applyToObject)
{
  /* Don't bother if we're done with the animation and if the animation was already applied to the
   * object. of if the animation made a double update for the same time and that it was applied to
   * the object.
   */
  if ((m_done || m_prevUpdate == curtime) && m_appliedToObject) {
    return;
  }
  m_prevUpdate = curtime;

  KX_Scene *scene = m_obj->GetScene();

  if (m_calc_localtime)
    SetLocalTime(curtime);
  else {
    ResetStartTime(curtime);
    m_calc_localtime = true;
  }

  // Handle wrap around
  if (m_localframe < std::min(m_startframe, m_endframe) ||
      m_localframe > std::max(m_startframe, m_endframe)) {
    switch (m_playmode) {
      case ACT_MODE_PLAY:
        // Clamp
        m_localframe = m_endframe;
        m_done = true;
        break;
      case ACT_MODE_LOOP:
        // Put the time back to the beginning
        m_localframe = m_startframe;
        m_starttime = curtime;
        break;
      case ACT_MODE_PING_PONG:
        // Swap the start and end frames
        float temp = m_startframe;
        m_startframe = m_endframe;
        m_endframe = temp;

        m_starttime = curtime;

        break;
    }
  }

  m_appliedToObject = applyToObject;
  // In case of culled armatures (doesn't requesting to transform the object) we only manages time.
  if (!applyToObject) {
    return;
  }

  m_requestIpo = true;

  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(KX_GetActiveEngine()->GetContext());
  Object *ob = m_obj->GetBlenderObject();  // eevee

  if (m_obj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE) {
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

    // BKE_object_where_is_calc_time(depsgraph, sc, ob, m_localframe);

    scene->ResetTaaSamples();

    BL_ArmatureObject *obj = (BL_ArmatureObject *)m_obj;

    if (m_layer_weight >= 0)
      obj->GetPose(&m_blendpose);

    // Extract the pose from the action
    obj->SetPoseByAction(m_action, m_localframe);

    ignore_parent_tx_bge(G_MAIN, depsgraph, scene, ob);

    // Handle blending between armature actions
    if (m_blendin && m_blendframe < m_blendin) {
      IncrementBlending(curtime);

      // Calculate weight
      float weight = 1.f - (m_blendframe / m_blendin);

      // Blend the poses
      obj->BlendInPose(m_blendinpose, weight, ACT_BLEND_BLEND);
    }

    // Handle layer blending
    if (m_layer_weight >= 0)
      obj->BlendInPose(m_blendpose, m_layer_weight, m_blendmode);

    obj->UpdateTimestep(curtime);
  }
  else {
    /* WARNING: The check to be sure the right action is played (to know if the action
     * which is in the actuator will be the one which will be played)
     * might be wrong (if (ob->adt && ob->adt->action == m_action) playaction;)
     * because WE MIGHT NEED TO CHANGE OB->ADT->ACTION DURING RUNTIME
     * then another check should be found to ensure to play the right action.
     */
    // TEST KEYFRAMED MODIFIERS (WRONG CODE BUT JUST FOR TESTING PURPOSE)
    for (ModifierData *md = (ModifierData *)ob->modifiers.first; md;
         md = (ModifierData *)md->next) {
      // TODO: We need to find the good notifier per action
      if (!modifier_isNonGeometrical(md) && ob->adt &&
          ob->adt->action->id.name == m_action->id.name) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        PointerRNA ptrrna;
        RNA_id_pointer_create(&ob->id, &ptrrna);
        animsys_evaluate_action(&ptrrna, m_action, m_localframe, false);
        scene->ResetTaaSamples();
        break;
      }
      /* HERE we can add other modifier action types,
       * if some actions require another notifier than ID_RECALC_GEOMETRY */
    }
    // TEST FollowPath action
    for (bConstraint *con = (bConstraint *)ob->constraints.first; con;
         con = (bConstraint *)con->next) {
      if (con) {
        if (ob->adt && ob->adt->action->id.name == m_action->id.name) {
          DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
          PointerRNA ptrrna;
          RNA_id_pointer_create(&ob->id, &ptrrna);
          animsys_evaluate_action(&ptrrna, m_action, m_localframe, false);

          ignore_parent_tx_bge(G_MAIN, depsgraph, scene, ob);

          scene->ResetTaaSamples();
          break;
        }
        /* HERE we can add other constraint action types,
         * if some actions require another notifier than ID_RECALC_TRANSFORM */
      }
    }
    // TEST Material action
    int totcol = ob->totcol;
    for (int i = 0; i < totcol; i++) {
      Material *ma = BKE_object_material_get(ob, i + 1);
      if (ma) {
        if (ma->use_nodes && ma->nodetree) {
          bNodeTree *node_tree = ma->nodetree;
          if (node_tree->adt && node_tree->adt->action->id.name == m_action->id.name) {
            DEG_id_tag_update(&ma->id, ID_RECALC_SHADING);
            PointerRNA ptrrna;
            RNA_id_pointer_create(&node_tree->id, &ptrrna);
            animsys_evaluate_action(&ptrrna, m_action, m_localframe, false);
            scene->ResetTaaSamples();
            break;
          }
        }
      }
    }
    // TEST Shapekeys action
    Mesh *me = (Mesh *)ob->data;
    if (ob->type == OB_MESH && me) {
      const bool bHasShapeKey = me->key && me->key->type == KEY_RELATIVE;
      if (bHasShapeKey && me->key->adt && me->key->adt->action->id.name == m_action->id.name) {
        DEG_id_tag_update(&me->id, ID_RECALC_GEOMETRY);
        Key *key = me->key;

        PointerRNA ptrrna;
        RNA_id_pointer_create(&key->id, &ptrrna);
        animsys_evaluate_action(&ptrrna, m_action, m_localframe, false);

        // Handle blending between shape actions
        if (m_blendin && m_blendframe < m_blendin) {
          IncrementBlending(curtime);

          float weight = 1.f - (m_blendframe / m_blendin);

          // We go through and clear out the keyblocks so there isn't any interference
          // from other shape actions
          KeyBlock *kb;
          for (kb = (KeyBlock *)key->block.first; kb; kb = (KeyBlock *)kb->next) {
            kb->curval = 0.f;
          }

          // Now blend the shape
          BlendShape(key, weight, m_blendinshape);
        }
        //// Handle layer blending
        // if (m_layer_weight >= 0) {
        //  shape_deformer->GetShape(m_blendshape);
        //  BlendShape(key, m_layer_weight, m_blendshape);
        //}

        // shape_deformer->SetLastFrame(curtime);

        scene->ResetTaaSamples();
      }
    }
    // TEST World Background actions
    World *world = scene->GetBlenderScene()->world;
    if (world && world->use_nodes && world->nodetree) {
      bNodeTree *node_tree = world->nodetree;
      if (node_tree->adt && node_tree->adt->action->id.name == m_action->id.name) {
        DEG_id_tag_update(&world->id, ID_RECALC_SHADING);
        PointerRNA ptrrna;
        RNA_id_pointer_create(&node_tree->id, &ptrrna);
        animsys_evaluate_action(&ptrrna, m_action, m_localframe, false);
        scene->ResetTaaSamples();
      }
    }
  }
}

void BL_Action::UpdateIPOs()
{
  if (m_sg_contr_list.size() == 0) {
    // Nothing to update or remove.
    return;
  }

  if (m_requestIpo) {
    m_obj->UpdateIPO(m_localframe, m_ipo_flags & ACT_IPOFLAG_CHILD);
    m_requestIpo = false;
  }

  // If the action is done we can remove its scene graph IPO controller.
  if (m_done) {
    ClearControllerList();
  }
}
