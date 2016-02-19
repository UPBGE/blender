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

/** \file RAS_BucketManager.h
 *  \ingroup bgerast
 */

#ifndef __RAS_BUCKETMANAGER_H__
#define __RAS_BUCKETMANAGER_H__

#include "MT_Transform.h"
#include "RAS_MaterialBucket.h"

#include <vector>

class SCA_IScene;

class RAS_BucketManager
{
public:
	typedef std::vector<class RAS_MaterialBucket*> BucketList;
	struct sortedmeshslot
	{
		/// depth
		MT_Scalar m_z;
		/// mesh slot
		RAS_MeshSlot *m_ms;
		/// buck mesh slot came from
		RAS_MaterialBucket *m_bucket;
		void set(RAS_MeshSlot *ms, RAS_MaterialBucket *bucket, const MT_Vector3& pnorm);
	};
	struct backtofront
	{
		bool operator()(const sortedmeshslot &a, const sortedmeshslot &b);
	};
	struct fronttoback
	{
		bool operator()(const sortedmeshslot &a, const sortedmeshslot &b);
	};

protected:
	enum BucketType {
		SOLID_BUCKET = 0,
		ALPHA_BUCKET,
		SOLID_INSTANCING_BUCKET,
		ALPHA_INSTANCING_BUCKET,
		SOLID_SHADOW_BUCKET,
		ALPHA_SHADOW_BUCKET,
		SOLID_SHADOW_INSTANCING_BUCKET,
		ALPHA_SHADOW_INSTANCING_BUCKET,
		ALL_BUCKET,
		NUM_BUCKET_TYPE,
	};

	BucketList m_buckets[NUM_BUCKET_TYPE];

public:
	RAS_BucketManager();
	virtual ~RAS_BucketManager();

	void Renderbuckets(const MT_Transform & cameratrans, RAS_IRasterizer* rasty);

	RAS_MaterialBucket* FindBucket(RAS_IPolyMaterial *material, bool &bucketCreated);
	void OptimizeBuckets(MT_Scalar distance);
	
	void ReleaseDisplayLists(RAS_IPolyMaterial *material = NULL);
	void ReleaseMaterials(RAS_IPolyMaterial *material = NULL);

	void RemoveMaterial(RAS_IPolyMaterial *mat); // freeing scenes only

	/* for merging */
	void MergeBucketManager(RAS_BucketManager *other, SCA_IScene *scene);
	BucketList& GetBuckets()
	{
		return m_buckets[ALL_BUCKET];
	}

	/*void PrintStats(int verbose_level) {
		printf("\nMappings...\n");
		printf("\t m_SolidBuckets: %d\n", m_SolidBuckets.size());
		printf("\t\t m_SolidBuckets: %d\n", m_SolidBuckets.size());
		printf("\t m_AlphaBuckets: %d\n", m_AlphaBuckets.size());
	}*/


private:
	unsigned int GetNumActiveMeshSlots(BucketType bucketType);
	void OrderBuckets(const MT_Transform& cameratrans, RAS_BucketManager::BucketType bucketType,
					  std::vector<sortedmeshslot>& slots, bool alpha, RAS_IRasterizer *rasty);

	void RenderBasicBuckets(const MT_Transform& cameratrans, RAS_IRasterizer *rasty, BucketType bucketType);
	void RenderSortedBuckets(const MT_Transform& cameratrans, RAS_IRasterizer* rasty, BucketType bucketType);


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_BucketManager")
#endif
};

#endif  /* __RAS_BUCKETMANAGER_H__ */
