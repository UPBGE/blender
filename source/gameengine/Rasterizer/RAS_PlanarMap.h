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
* Contributor(s): Ulysse Martin, Tristan Porteries, Martins Upitis.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file RAS_PlanarMap.h
*  \ingroup bgerast
*/

#ifndef __RAS_PLANAR_MAP_H__
#define __RAS_PLANAR_MAP_H__

#include "RAS_TextureRenderer.h"

class RAS_PlanarMap : public virtual RAS_TextureRenderer
{
public:
	RAS_PlanarMap();
	virtual ~RAS_PlanarMap();

	void EnableClipPlane(const MT_Vector3& mirrorWorldZ, float mirrorPlaneDTerm, int planartype);
	void DisableClipPlane(int planartype);
};

#endif  // __RAS_PLANAR_MAP_H__
