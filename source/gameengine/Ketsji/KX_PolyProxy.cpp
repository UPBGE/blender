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

/** \file gameengine/Ketsji/KX_PolyProxy.cpp
 *  \ingroup ketsji
 */


#ifdef WITH_PYTHON

#include "KX_PolyProxy.h"
#include "KX_MeshProxy.h"
#include "RAS_MeshObject.h"
#include "KX_VertexProxy.h"
#include "RAS_Polygon.h"
#include "KX_BlenderMaterial.h"
#include "EXP_ListWrapper.h"

#include "KX_PyMath.h"

RAS_Polygon *KX_PolyProxy::GetPolygon()
{
	return m_polygon;
}

RAS_MeshObject *KX_PolyProxy::GetMeshObject()
{
	return m_mesh;
}

KX_MeshProxy *KX_PolyProxy::GetMeshProxy()
{
	return m_meshProxy;
}

PyTypeObject KX_PolyProxy::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_PolyProxy",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_PolyProxy::Methods[] = {
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterialIndex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getNumVertex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,isVisible),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,isCollider),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterialName),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getTextureName),
	KX_PYMETHODTABLE(KX_PolyProxy,getVertexIndex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMesh),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterial),
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_PolyProxy::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("material_name", KX_PolyProxy, pyattr_get_material_name),
	KX_PYATTRIBUTE_RO_FUNCTION("texture_name", KX_PolyProxy, pyattr_get_texture_name),
	KX_PYATTRIBUTE_RO_FUNCTION("material", KX_PolyProxy, pyattr_get_material),
	KX_PYATTRIBUTE_RO_FUNCTION("material_id", KX_PolyProxy, pyattr_get_material_id),
	KX_PYATTRIBUTE_RO_FUNCTION("v1", KX_PolyProxy, pyattr_get_v1),
	KX_PYATTRIBUTE_RO_FUNCTION("v2", KX_PolyProxy, pyattr_get_v2),
	KX_PYATTRIBUTE_RO_FUNCTION("v3", KX_PolyProxy, pyattr_get_v3),
	KX_PYATTRIBUTE_RO_FUNCTION("v4", KX_PolyProxy, pyattr_get_v4),
	KX_PYATTRIBUTE_RO_FUNCTION("visible", KX_PolyProxy, pyattr_get_visible),
	KX_PYATTRIBUTE_RO_FUNCTION("collide", KX_PolyProxy, pyattr_get_collide),
	KX_PYATTRIBUTE_RO_FUNCTION("vertices", KX_PolyProxy, pyattr_get_vertices),
	{ NULL }	//Sentinel
};

KX_PolyProxy::KX_PolyProxy(KX_MeshProxy *meshProxy, RAS_Polygon* polygon)
	:m_meshProxy(meshProxy),
	m_polygon(polygon),
	m_mesh(meshProxy->GetMesh())
{
	Py_INCREF(m_meshProxy->GetProxy());
}

KX_PolyProxy::~KX_PolyProxy()
{
	Py_DECREF(m_meshProxy->GetProxy());
}


// stuff for cvalue related things
static STR_String sPolyName = "polygone";
STR_String &KX_PolyProxy::GetName()
{
	return sPolyName;
}

// stuff for python integration

PyObject *KX_PolyProxy::pyattr_get_material_name(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetMaterialName();
}

PyObject *KX_PolyProxy::pyattr_get_texture_name(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetTextureName();
}

PyObject *KX_PolyProxy::pyattr_get_material(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetMaterial();
}

PyObject *KX_PolyProxy::pyattr_get_material_id(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PygetMaterialIndex();
}

PyObject *KX_PolyProxy::pyattr_get_v1(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);

	return PyLong_FromLong(self->m_polygon->GetVertexOffset(0));
}

PyObject *KX_PolyProxy::pyattr_get_v2(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);

	return PyLong_FromLong(self->m_polygon->GetVertexOffset(1));
}

PyObject *KX_PolyProxy::pyattr_get_v3(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);

	return PyLong_FromLong(self->m_polygon->GetVertexOffset(2));
}

PyObject *KX_PolyProxy::pyattr_get_v4(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);

	if (3 < self->m_polygon->VertexCount())
	{
		return PyLong_FromLong(self->m_polygon->GetVertexOffset(3));
	}
	return PyLong_FromLong(0);
}

PyObject *KX_PolyProxy::pyattr_get_visible(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PyisVisible();
}

PyObject *KX_PolyProxy::pyattr_get_collide(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolyProxy* self = static_cast<KX_PolyProxy*>(self_v);
	return self->PyisCollider();
}

static int kx_poly_proxy_get_vertices_size_cb(void *self_v)
{
	return ((KX_PolyProxy *)self_v)->GetPolygon()->VertexCount();
}

static PyObject *kx_poly_proxy_get_vertices_item_cb(void *self_v, int index)
{
	KX_PolyProxy *self = static_cast<KX_PolyProxy *>(self_v);
	KX_MeshProxy *meshproxy = self->GetMeshProxy();
	int vertindex = self->GetPolygon()->GetVertexOffset(index);
	RAS_MaterialBucket *polyBucket = self->GetPolygon()->GetMaterial();
	unsigned int matid;
	for (matid = 0; matid < (unsigned int)self->GetMeshObject()->NumMaterials(); matid++)
	{
		RAS_MeshMaterial *meshMat = self->GetMeshObject()->GetMeshMaterial(matid);
		if (meshMat->m_bucket == polyBucket)
			// found it
			break;
	}
	KX_VertexProxy *vert = new KX_VertexProxy(meshproxy, (RAS_ITexVert *)(self->GetMeshObject()->GetVertex(matid, vertindex)));
	return vert->GetProxy();
}

PyObject *KX_PolyProxy::pyattr_get_vertices(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return (new CListWrapper(self_v,
		((KX_PolyProxy *)self_v)->GetProxy(),
		NULL,
		kx_poly_proxy_get_vertices_size_cb,
		kx_poly_proxy_get_vertices_item_cb,
		NULL,
		NULL))->NewProxy(true);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterialIndex,
"getMaterialIndex() : return the material index of the polygon in the mesh\n")
{
	RAS_MaterialBucket* polyBucket = m_polygon->GetMaterial();
	unsigned int matid;
	for (matid=0; matid<(unsigned int)m_mesh->NumMaterials(); matid++)
	{
		RAS_MeshMaterial* meshMat = m_mesh->GetMeshMaterial(matid);
		if (meshMat->m_bucket == polyBucket)
			// found it
			break;
	}
	return PyLong_FromLong(matid);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getNumVertex,
"getNumVertex() : returns the number of vertex of the polygon, 3 or 4\n")
{
	return PyLong_FromLong(m_polygon->VertexCount());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, isVisible,
"isVisible() : returns whether the polygon is visible or not\n")
{
	return PyLong_FromLong(m_polygon->IsVisible());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, isCollider,
"isCollider() : returns whether the polygon is receives collision or not\n")
{
	return PyLong_FromLong(m_polygon->IsCollider());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterialName,
"getMaterialName() : returns the polygon material name, \"NoMaterial\" if no material\n")
{
	return PyUnicode_From_STR_String(m_polygon->GetMaterial()->GetPolyMaterial()->GetName());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getTextureName,
"getTexturelName() : returns the polygon texture name, \"NULL\" if no texture\n")
{
	return PyUnicode_From_STR_String(m_polygon->GetMaterial()->GetPolyMaterial()->GetTextureName());
}

KX_PYMETHODDEF_DOC(KX_PolyProxy, getVertexIndex,
"getVertexIndex(vertex) : returns the mesh vertex index of a polygon vertex\n"
"vertex: index of the vertex in the polygon: 0->3\n"
"return value can be used to retrieve the vertex details through mesh proxy\n"
"Note: getVertexIndex(3) on a triangle polygon returns 0\n")
{
	int index;
	if (!PyArg_ParseTuple(args,"i:getVertexIndex",&index))
	{
		return NULL;
	}
	if (index < 0 || index > 3)
	{
		PyErr_SetString(PyExc_AttributeError, "poly.getVertexIndex(int): KX_PolyProxy, expected an index between 0-3");
		return NULL;
	}
	if (index < m_polygon->VertexCount())
	{
		return PyLong_FromLong(m_polygon->GetVertexOffset(index));
	}
	return PyLong_FromLong(0);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMesh,
"getMesh() : returns a mesh proxy\n")
{
	return m_meshProxy->GetProxy();
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterial,
"getMaterial() : returns a material\n")
{
	RAS_IPolyMaterial *polymat = m_polygon->GetMaterial()->GetPolyMaterial();
	KX_BlenderMaterial* mat = static_cast<KX_BlenderMaterial*>(polymat);
	return mat->GetProxy();
}

#endif // WITH_PYTHON
