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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Porteries Tristan.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RAS_MeshUser.h
 *  \ingroup bgerast
 */

#ifndef __RAS_MESH_USER_H__
#define __RAS_MESH_USER_H__

#include "RAS_MeshSlot.h"
#include "MT_Vector4.h"

class RAS_MeshUser
{
private:
	bool m_frontFace;
	bool m_culled;
	MT_Vector4 m_color;
	float *m_matrix;
	void *m_clientObject;
	RAS_MeshSlotList m_meshSlots;

public:
	RAS_MeshUser(void *clientobj);
	virtual ~RAS_MeshUser();

	void AddMeshSlot(RAS_MeshSlot *meshSlot);
	bool GetFrontFace() const;
	bool GetCulled() const;
	const MT_Vector4& GetColor() const;
	float *GetMatrix() const;
	void *GetClientObject() const;
	RAS_MeshSlotList& GetMeshSlots();

	void SetFrontFace(bool frontFace);
	void SetCulled(bool culled);
	void SetColor(const MT_Vector4& color);
	void SetMatrix(float *matrix);
};

#endif  // __RAS_MESH_USER_H__
