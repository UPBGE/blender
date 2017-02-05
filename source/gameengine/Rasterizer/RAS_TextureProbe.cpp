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

/** \file RAS_TextureProbe.cpp
 *  \ingroup bgerast
 */

#include "RAS_TextureProbe.h"
#include "RAS_Texture.h"
#include "RAS_IRasterizer.h"

#include "GPU_texture.h"
#include "GPU_framebuffer.h"
#include "GPU_draw.h"

#include "BKE_image.h"
#include "KX_GameObject.h"
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"

#include "DNA_texture_types.h"

#include "glew-mx.h"

#include "BLI_math.h"

RAS_TextureProbe::Face::Face(int target)
	:m_fbo(NULL),
	m_rb(NULL),
	m_target(target)
{
}

RAS_TextureProbe::Face::~Face()
{
}

void RAS_TextureProbe::Face::AttachTexture(GPUTexture *tex)
{
	m_fbo = GPU_framebuffer_create();
	m_rb = GPU_renderbuffer_create(GPU_texture_width(tex), GPU_texture_height(tex),
		0, GPU_HDR_NONE, GPU_RENDERBUFFER_DEPTH, NULL);

	GPU_framebuffer_texture_attach_target(m_fbo, tex, m_target, 0, NULL);
	GPU_framebuffer_renderbuffer_attach(m_fbo, m_rb, 0, NULL);
}

void RAS_TextureProbe::Face::DetachTexture(GPUTexture *tex)
{
	if (m_fbo) {
		GPU_framebuffer_texture_detach_target(tex, m_target);
	}
	if (m_rb) {
		GPU_framebuffer_renderbuffer_detach(m_rb);
	}

	if (m_fbo) {
		GPU_framebuffer_free(m_fbo);
		m_fbo = NULL;
	}
	if (m_rb) {
		GPU_renderbuffer_free(m_rb);
		m_rb = NULL;
	}
}

void RAS_TextureProbe::Face::Bind()
{
	// Set the viewport in the same time.
	GPU_framebuffer_bind_no_save(m_fbo, 0);
}

RAS_TextureProbe::RAS_TextureProbe()
	:m_gpuTex(NULL),
	m_useMipmap(false)
{
}

RAS_TextureProbe::~RAS_TextureProbe()
{
	for (Face& face : m_faces) {
		face.DetachTexture(m_gpuTex);
	}

	if (m_gpuTex) {
		GPU_texture_free(m_gpuTex);
	}

	/* This call has for side effect to ask regeneration of all textures
	* depending of this image.
	*/
	for (RAS_Texture *texture : m_textureUsers) {
		// Invalidate the probe in each material texture users.
		texture->SetProbe(NULL);
		BKE_image_free_buffers(texture->GetImage());
	}
}

void RAS_TextureProbe::GetValidTexture()
{
	BLI_assert(m_textureUsers.size() > 0);

	/* The gpu texture returned by all material textures are the same.
	* We can so use the first material texture user.
	*/
	RAS_Texture *texture = m_textureUsers[0];
	texture->CheckValidTexture();
	GPUTexture *gputex = texture->GetGPUTexture();

	if (m_gpuTex == gputex) {
		// The gpu texture is the same.
		return;
	}

	if (m_gpuTex) {
		for (Face& face : m_faces) {
			face.DetachTexture(m_gpuTex);
		}

		GPU_texture_free(m_gpuTex);
	}

	m_gpuTex = gputex;

	// Increment reference to make sure the gpu texture will not be freed by someone else.
	GPU_texture_ref(m_gpuTex);

	for (Face& face : m_faces) {
		face.AttachTexture(m_gpuTex);
	}

	Tex *tex = texture->GetTex();
	EnvMap *env = tex->env;
	m_useMipmap = (env->filtering == ENVMAP_MIPMAP_MIPMAP) && GPU_get_mipmap();

	if (!m_useMipmap) {
		// Disable mipmaping.
		GPU_texture_bind(m_gpuTex, 0);
		GPU_texture_filter_mode(m_gpuTex, false, (env->filtering == ENVMAP_MIPMAP_LINEAR), false);
		GPU_texture_unbind(m_gpuTex);
	}
}

unsigned short RAS_TextureProbe::GetNumFaces() const
{
	return m_faces.size();
}

const std::vector<RAS_Texture *>& RAS_TextureProbe::GetTextureUsers() const
{
	return m_textureUsers;
}

void RAS_TextureProbe::AddTextureUser(RAS_Texture *texture)
{
	m_textureUsers.push_back(texture);
	texture->SetProbe(this);
}

void RAS_TextureProbe::BeginRender(RAS_IRasterizer *rasty)
{
	GetValidTexture();
}

void RAS_TextureProbe::EndRender(RAS_IRasterizer *rasty)
{
	if (m_useMipmap) {
		GPU_texture_bind(m_gpuTex, 0);
		GPU_texture_generate_mipmap(m_gpuTex);
		GPU_texture_unbind(m_gpuTex);
	}
}

void RAS_TextureProbe::BindFace(RAS_IRasterizer *rasty, unsigned short index)
{
	m_faces[index].Bind();

	rasty->Clear(RAS_IRasterizer::RAS_COLOR_BUFFER_BIT | RAS_IRasterizer::RAS_DEPTH_BUFFER_BIT);
}
