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

/** \file gameengine/GameLogic/SCA_InputEvent.cpp
 *  \ingroup gamelogic
 */


#include "SCA_InputEvent.h"

#include "EXP_ListWrapper.h"

#include <algorithm>

SCA_InputEvent::SCA_InputEvent()
	:m_unicode(0)
{
	m_status.push_back(KX_NO_INPUTSTATUS);
	m_values.push_back(0); // TODO mouse default value.
}

void SCA_InputEvent::Clear()
{
	SCA_EnumInputs status = m_status[m_status.size() - 1];
	m_status.clear();
	m_status.push_back(status);

	int value = m_values[m_values.size() - 1];
	m_values.clear();
	m_values.push_back(value);

	m_queue.clear();
}

bool SCA_InputEvent::Find(SCA_EnumInputs inputenum) const
{
	if (inputenum == KX_NO_INPUTSTATUS || inputenum == KX_ACTIVE) {
		return std::count(m_status.begin(), m_status.end(), inputenum);
	}
	else {
		return std::count(m_queue.begin(), m_queue.end(), inputenum);
	}
}

#ifdef WITH_PYTHON

PyTypeObject SCA_InputEvent::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_InputEvent",
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
	0,0,0,
	tp_richcompare,
	0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_InputEvent::Methods[] = {
	{NULL, NULL} //Sentinel
};

PyAttributeDef SCA_InputEvent::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("status", SCA_InputEvent, pyattr_get_status),
	KX_PYATTRIBUTE_RO_FUNCTION("queue", SCA_InputEvent, pyattr_get_queue),
	KX_PYATTRIBUTE_RO_FUNCTION("values", SCA_InputEvent, pyattr_get_values),
	{NULL} //Sentinel
};

int SCA_InputEvent::get_status_size_cb(void *self_v)
{
	return ((SCA_InputEvent *)self_v)->m_status.size();
}

PyObject *SCA_InputEvent::get_status_item_cb(void *self_v, int index)
{
	return PyLong_FromLong(((SCA_InputEvent *)self_v)->m_status[index]);
}

PyObject *SCA_InputEvent::pyattr_get_status(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return (new CListWrapper(self_v,
							 ((SCA_InputEvent *)self_v)->GetProxy(),
							 NULL,
							 SCA_InputEvent::get_status_size_cb,
							 SCA_InputEvent::get_status_item_cb,
							 NULL,
							 NULL))->NewProxy(true);
}

int SCA_InputEvent::get_queue_size_cb(void *self_v)
{
	return ((SCA_InputEvent *)self_v)->m_queue.size();
}

PyObject *SCA_InputEvent::get_queue_item_cb(void *self_v, int index)
{
	return PyLong_FromLong(((SCA_InputEvent *)self_v)->m_queue[index]);
}

PyObject *SCA_InputEvent::pyattr_get_queue(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return (new CListWrapper(self_v,
							 ((SCA_InputEvent *)self_v)->GetProxy(),
							 NULL,
							 SCA_InputEvent::get_queue_size_cb,
							 SCA_InputEvent::get_queue_item_cb,
							 NULL,
							 NULL))->NewProxy(true);
}

int SCA_InputEvent::get_values_size_cb(void *self_v)
{
	return ((SCA_InputEvent *)self_v)->m_values.size();
}

PyObject *SCA_InputEvent::get_values_item_cb(void *self_v, int index)
{
	return PyLong_FromLong(((SCA_InputEvent *)self_v)->m_values[index]);
}

PyObject *SCA_InputEvent::pyattr_get_values(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return (new CListWrapper(self_v,
							 ((SCA_InputEvent *)self_v)->GetProxy(),
							 NULL,
							 SCA_InputEvent::get_values_size_cb,
							 SCA_InputEvent::get_values_item_cb,
							 NULL,
							 NULL))->NewProxy(true);
}

PyObject *SCA_InputEvent::tp_richcompare(PyObject *a, PyObject *b, int op)
{
	if (op == Py_EQ || op == Py_NE) {
		int val = -1;
		SCA_InputEvent *event = NULL;
		if (PyLong_Check(a)) {
			val = PyLong_AsLong(a);
			event = (SCA_InputEvent *)BGE_PROXY_REF(b);
		}
		else if (PyLong_Check(b)) {
			val = PyLong_AsLong(b);
			event = (SCA_InputEvent *)BGE_PROXY_REF(a);
		}

		if (val != -1 && event) {
			if ((val == KX_ACTIVE) ||
				(val == KX_NO_INPUTSTATUS) ||
				(val == KX_JUSTACTIVATED) ||
				(val == KX_JUSTRELEASED))
			{
				ShowDeprecationWarning("event == value", "value in event.status or value in event.queue");
				bool found = event->Find((SCA_EnumInputs)val);
				if (op == Py_NE) {
					// Inverse returned value.
					found = !found;
				}
				PyObject *ret = (found ? Py_True : Py_False);
				Py_INCREF(ret);
				return ret;
			}
		}
	}

	return Py_False;
}

#endif  // WITH_PYTHON
