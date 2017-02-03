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

/** \file RAS_IRasterizer.h
 *  \ingroup bgerast
 */

#ifndef __RAS_IRASTERIZER_H__
#define __RAS_IRASTERIZER_H__

#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include "MT_CmMatrix4x4.h"
#include "MT_Matrix4x4.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

#include <string>
#include <map>
#include <vector>
#include <memory>

class RAS_OpenGLRasterizer;
class RAS_OpenGLLight;
class RAS_StorageVBO;
class RAS_ICanvas;
class RAS_OffScreen;
class RAS_MeshSlot;
class RAS_DisplayArrayBucket;
class RAS_ILightObject;
class SCA_IScene;
class RAS_ISync;
struct KX_ClientObjectInfo;
class KX_RayCast;

struct GPUShader;

/**
 * 3D rendering device context interface. 
 */
class RAS_IRasterizer
{
public:
	enum RAS_TEXT_RENDER_MODE {
		RAS_TEXT_RENDER_NODEF = 0,
		RAS_TEXT_NORMAL,
		RAS_TEXT_PADDED,
		RAS_TEXT_MAX,
	};

	/**
	 * Drawing types
	 */
	enum DrawType {
		RAS_WIREFRAME = 0,
		RAS_SOLID,
		RAS_TEXTURED,
		RAS_CUBEMAP,
		RAS_SHADOW,
		RAS_DRAW_MAX,
	};

	/**
	 * Valid SetDepthMask parameters
	 */
	enum DepthMask {
		RAS_DEPTHMASK_ENABLED = 1,
		RAS_DEPTHMASK_DISABLED,
	};

	/**
	 */
	enum {
		RAS_BACKCULL = 16, // GEMAT_BACKCULL
	};

	/**
	 * Stereo mode types
	 */
	enum StereoMode {
		RAS_STEREO_NOSTEREO = 1,
		// WARNING: Not yet supported.
		RAS_STEREO_QUADBUFFERED,
		RAS_STEREO_ABOVEBELOW,
		RAS_STEREO_INTERLACED,
		RAS_STEREO_ANAGLYPH,
		RAS_STEREO_SIDEBYSIDE,
		RAS_STEREO_VINTERLACE,
		RAS_STEREO_3DTVTOPBOTTOM,

		RAS_STEREO_MAXSTEREO
	};

	/**
	 * Texture gen modes.
	 */
	enum TexCoGen {
		RAS_TEXCO_GEN,      /* < GPU will generate texture coordinates */
		RAS_TEXCO_ORCO,     /* < Vertex coordinates (object space) */
		RAS_TEXCO_GLOB,     /* < Vertex coordinates (world space) */
		RAS_TEXCO_UV,       /* < UV coordinates */
		RAS_TEXCO_OBJECT,   /* < Use another object's position as coordinates */
		RAS_TEXCO_LAVECTOR, /* < Light vector as coordinates */
		RAS_TEXCO_VIEW,     /* < View vector as coordinates */
		RAS_TEXCO_STICKY,   /* < Sticky coordinates */
		RAS_TEXCO_WINDOW,   /* < Window coordinates */
		RAS_TEXCO_NORM,     /* < Normal coordinates */
		RAS_TEXTANGENT,     /* < */
		RAS_TEXCO_VCOL,     /* < Vertex Color */
		RAS_TEXCO_DISABLE,  /* < Disable this texture unit (cached) */
	};

	typedef std::vector<TexCoGen> TexCoGenList;
	typedef std::map<unsigned short, unsigned short> AttribLayerList;

	/**
	 * Render pass identifiers for stereo.
	 */
	enum StereoEye {
		RAS_STEREO_LEFTEYE = 0,
		RAS_STEREO_RIGHTEYE,
	};

	/**
	 * Mipmap options
	 */
	enum MipmapOption {
		RAS_MIPMAP_NONE,
		RAS_MIPMAP_NEAREST,
		RAS_MIPMAP_LINEAR,

		RAS_MIPMAP_MAX,  /* Should always be last */
	};

	/**
	 * Override shaders
	 */
	enum OverrideShaderType {
		RAS_OVERRIDE_SHADER_NONE,
		RAS_OVERRIDE_SHADER_BASIC,
		RAS_OVERRIDE_SHADER_BASIC_INSTANCING,
		RAS_OVERRIDE_SHADER_SHADOW_VARIANCE,
		RAS_OVERRIDE_SHADER_SHADOW_VARIANCE_INSTANCING,
	};

	enum ShadowType {
		RAS_SHADOW_NONE,
		RAS_SHADOW_SIMPLE,
		RAS_SHADOW_VARIANCE,
	};

	enum EnableBit {
		RAS_DEPTH_TEST = 0,
		RAS_ALPHA_TEST,
		RAS_SCISSOR_TEST,
		RAS_TEXTURE_2D,
		RAS_TEXTURE_CUBE_MAP,
		RAS_BLEND,
		RAS_COLOR_MATERIAL,
		RAS_CULL_FACE,
		RAS_FOG,
		RAS_LIGHTING,
		RAS_MULTISAMPLE,
		RAS_POLYGON_STIPPLE,
		RAS_POLYGON_OFFSET_FILL,
		RAS_POLYGON_OFFSET_LINE
	};

	enum DepthFunc {
		RAS_NEVER = 0,
		RAS_LEQUAL,
		RAS_LESS,
		RAS_ALWAYS,
		RAS_GEQUAL,
		RAS_GREATER,
		RAS_NOTEQUAL,
		RAS_EQUAL
	};

	enum BlendFunc {
		RAS_ZERO = 0,
		RAS_ONE,
		RAS_SRC_COLOR,
		RAS_ONE_MINUS_SRC_COLOR,
		RAS_DST_COLOR,
		RAS_ONE_MINUS_DST_COLOR,
		RAS_SRC_ALPHA,
		RAS_ONE_MINUS_SRC_ALPHA,
		RAS_DST_ALPHA,
		RAS_ONE_MINUS_DST_ALPHA,
		RAS_SRC_ALPHA_SATURATE
	};

	enum MatrixMode {
		RAS_PROJECTION = 0,
		RAS_MODELVIEW,
		RAS_TEXTURE,
		RAS_MATRIX_MODE_MAX
	};

	enum ClearBit {
		RAS_COLOR_BUFFER_BIT = 0x2,
		RAS_DEPTH_BUFFER_BIT = 0x4,
		RAS_STENCIL_BUFFER_BIT = 0x8
	};

	enum OffScreenType {
		RAS_OFFSCREEN_RENDER = 0,
		RAS_OFFSCREEN_FILTER0,
		RAS_OFFSCREEN_FILTER1,
		RAS_OFFSCREEN_EYE_LEFT0,
		RAS_OFFSCREEN_EYE_RIGHT0,
		RAS_OFFSCREEN_EYE_LEFT1,
		RAS_OFFSCREEN_EYE_RIGHT1,
		RAS_OFFSCREEN_FINAL,
		RAS_OFFSCREEN_BLIT_DEPTH,

		RAS_OFFSCREEN_CUSTOM,

		RAS_OFFSCREEN_MAX,
	};

	enum HdrType {
		RAS_HDR_NONE = 0,
		RAS_HDR_HALF_FLOAT,
		RAS_HDR_FULL_FLOAT,
		RAS_HDR_MAX
	};

	/** Return the output frame buffer normally used for the input frame buffer
	 * index in case of filters render.
	 * \param index The input frame buffer, can be a non-filter frame buffer.
	 * \return The output filter frame buffer.
	 */
	static OffScreenType NextFilterOffScreen(OffScreenType index);


	/** Return the output frame buffer normally used for the input frame buffer
	 * index in case of per eye stereo render.
	 * \param index The input eye frame buffer, can NOT be a non-eye frame buffer.
	 * \return The output eye frame buffer.
	 */
	static OffScreenType NextEyeOffScreen(OffScreenType index);

	/** Return the output frame buffer normally used for the input frame buffer
	 * index in case of simple render.
	 * \param index The input render frame buffer, can NOT be a non-render frame buffer.
	 * \return The output render frame buffer.
	 */
	static OffScreenType NextRenderOffScreen(OffScreenType index);

	struct StorageAttribs
	{
		TexCoGenList attribs;
		TexCoGenList texcos;
		AttribLayerList layers;
	};

	struct DebugShape
	{
		MT_Vector4 m_color;
	};

	struct DebugLine : DebugShape
	{
		MT_Vector3 m_from;
		MT_Vector3 m_to;
	};

	struct DebugCircle : DebugShape
	{
		MT_Vector3 m_center;
		MT_Vector3 m_normal;
		float m_radius;
		int m_sector;
	};

	struct DebugAabb : DebugShape
	{
		MT_Vector3 m_pos;
		MT_Matrix3x3 m_rot;
		MT_Vector3 m_min;
		MT_Vector3 m_max;
	};

	struct DebugBox : DebugShape
	{
		MT_Vector3 m_vertexes[8];
	};

	struct DebugSolidBox : DebugBox
	{
		MT_Vector4 m_insideColor;
		MT_Vector4 m_outsideColor;
		bool m_solid;
	};

	struct SceneDebugShape
	{
		std::vector<DebugLine> m_lines;
		std::vector<DebugCircle> m_circles;
		std::vector<DebugAabb> m_aabbs;
		std::vector<DebugBox> m_boxes;
		std::vector<DebugSolidBox> m_solidBoxes;
	};

private:
	class OffScreens
	{
	private:
		std::unique_ptr<RAS_OffScreen> m_offScreens[RAS_OFFSCREEN_MAX];
		unsigned int m_width;
		unsigned int m_height;
		int m_samples;
		HdrType m_hdr;

	public:
		OffScreens();
		~OffScreens();

		void Update(RAS_ICanvas *canvas);
		RAS_OffScreen *GetOffScreen(RAS_IRasterizer::OffScreenType type);
	};

	// All info used to compute the ray cast transform matrix.
	struct RayCastTranform
	{
		/// The object scale.
		MT_Vector3 scale;
		/// The original object matrix.
		float *origmat;
		/// The output matrix.
		float *mat;
	};

	/// \section Interfaces used for frame buffer shaders.

	struct OverrideShaderDrawFrameBufferInterface
	{
		int colorTexLoc;
	};

	struct OverrideShaderStereoStippleInterface
	{
		int leftEyeTexLoc;
		int rightEyeTexLoc;
		int stippleIdLoc;
	};

	struct OverrideShaderStereoAnaglyph
	{
		int leftEyeTexLoc;
		int rightEyeTexLoc;
	};

	// We store each debug shape by scene.
	std::map<SCA_IScene *, SceneDebugShape> m_debugShapes;

	/* fogging vars */
	bool m_fogenabled;

	double m_time;
	MT_Vector3 m_ambient;
	MT_Matrix4x4 m_viewmatrix;
	MT_Matrix4x4 m_viewinvmatrix;
	MT_Vector3 m_campos;
	bool m_camortho;
	bool m_camnegscale;

	StereoMode m_stereomode;
	StereoEye m_curreye;
	float m_eyeseparation;
	float m_focallength;
	bool m_setfocallength;
	int m_noOfScanlines;

	/* motion blur */
	unsigned short m_motionblur;
	float m_motionblurvalue;

	/* Render tools */
	void *m_clientobject;
	void *m_auxilaryClientInfo;
	std::vector<RAS_OpenGLLight *> m_lights;
	int m_lastlightlayer;
	bool m_lastlighting;
	void *m_lastauxinfo;
	unsigned int m_numgllights;

	/// Class used to manage off screens used by the rasterizer.
	OffScreens m_offScreens;

	DrawType m_drawingmode;
	ShadowType m_shadowMode;

	StorageAttribs m_storageAttribs;

	bool m_invertFrontFace;
	bool m_last_frontface;

	OverrideShaderType m_overrideShader;

	std::unique_ptr<RAS_StorageVBO> m_storage;
	std::unique_ptr<RAS_OpenGLRasterizer> m_impl;

	/// Initialize custom shader interface containing uniform location.
	void InitOverrideShadersInterface();

	/// Return GPUShader coresponding to the override shader enumeration.
	GPUShader *GetOverrideGPUShader(OverrideShaderType type);

	void EnableLights();
	void DisableLights();

	void DisableForText();

public:
	RAS_IRasterizer();
	virtual ~RAS_IRasterizer();

	/**
	 * Enable capability
	 * \param bit Enable bit
	 */
	void Enable(EnableBit bit);

	/**
	 * Disable capability
	 * \param bit Enable bit
	 */
	void Disable(EnableBit bit);

	/**
	 * Set the value for Depth Buffer comparisons
	 * \param func Depth comparison function
	 */
	void SetDepthFunc(DepthFunc func);

	/** 
	 * Set the blending equation.
	 * \param src The src value.
	 * \param dst The destination value.
	 */
	void SetBlendFunc(BlendFunc src, BlendFunc dst);

	/**
	 * Takes a screenshot
	 */
	unsigned int *MakeScreenshot(int x, int y, int width, int height);

	/**
	 * SetDepthMask enables or disables writing a fragment's depth value
	 * to the Z buffer.
	 */
	void SetDepthMask(DepthMask depthmask);

	/**
	 * Init initializes the renderer.
	 */
	void Init();

	/**
	 * Exit cleans up the renderer.
	 */
	void Exit();

	/**
	 * BeginFrame is called at the start of each frame.
	 */
	void BeginFrame(double time);

	/**
	 * EndFrame is called at the end of each frame.
	 */
	void EndFrame();

	/**
	 * Clears a specified set of buffers
	 * \param clearbit What buffers to clear (separated by bitwise OR)
	 */
	void Clear(int clearbit);

	/**
	 * Set background color
	 */
	void SetClearColor(float r, float g, float b, float a=1.0f);

	/**
	 * Set background depth
	 */
	void SetClearDepth(float d);

	/**
	 * Set background color mask.
	 */
	void SetColorMask(bool r, bool g, bool b, bool a);

	/**
	 * Draw screen overlay plane with basic uv coordinates.
	 */
	void DrawOverlayPlane();

	/// Update dimensions of all off screens.
	void UpdateOffScreens(RAS_ICanvas *canvas);

	/** Return the corresponding off screen to off screen type.
	 * \param type The off screen type to return.
	 */
	RAS_OffScreen *GetOffScreen(OffScreenType type);

	/** Draw off screen without set viewport.
	 * Used to copy the frame buffer object to another.
	 * \param srcindex The input off screen index.
	 * \param dstindex The output off screen index.
	 */
	void DrawOffScreen(RAS_OffScreen *srcOffScreen, RAS_OffScreen *dstOffScreen);

	/** Draw off screen at the given index to screen.
	 * \param canvas The canvas containing the screen viewport.
	 * \param index The off screen index to read from.
	 */
	void DrawOffScreen(RAS_ICanvas *canvas, RAS_OffScreen *offScreen);

	/** Draw each stereo off screen to screen.
	 * \param canvas The canvas containing the screen viewport.
	 * \param lefteyeindex The left off screen index.
	 * \param righteyeindex The right off screen index.
	 */
	void DrawStereoOffScreen(RAS_ICanvas *canvas, RAS_OffScreen *leftOffScreen, RAS_OffScreen *rightOffScreen);

	/**
	 * SetRenderArea sets the render area from the 2d canvas.
	 * Returns true if only of subset of the canvas is used.
	 */
	void SetRenderArea(RAS_ICanvas *canvas);

	// Stereo Functions
	/**
	 * SetStereoMode will set the stereo mode
	 */
	void SetStereoMode(const StereoMode stereomode);

	/**
	 * Stereo can be used to query if the rasterizer is in stereo mode.
	 * \return true if stereo mode is enabled.
	 */
	bool Stereo();
	StereoMode GetStereoMode();

	/**
	 * Sets which eye buffer subsequent primitives will be rendered to.
	 */
	void SetEye(const StereoEye eye);
	StereoEye GetEye();

	/**
	 * Sets the distance between eyes for stereo mode.
	 */
	void SetEyeSeparation(const float eyeseparation);
	float GetEyeSeparation();

	/**
	 * Sets the focal length for stereo mode.
	 */
	void SetFocalLength(const float focallength);
	float GetFocalLength();

	/**
	 * Create a sync object
	 * For use with offscreen render
	 */
	RAS_ISync *CreateSync(int type);

	/**
	 * SwapBuffers swaps the back buffer with the front buffer.
	 */
	void SwapBuffers(RAS_ICanvas *canvas);
	
	// Drawing Functions
	/// Set all pre render attributs for given display array bucket.
	void BindPrimitives(RAS_DisplayArrayBucket *arrayBucket);
	/// UnSet all pre render attributs for given display array bucket.
	void UnbindPrimitives(RAS_DisplayArrayBucket *arrayBucket);
	/**
	 * IndexPrimitives: Renders primitives from mesh slot.
	 */
	void IndexPrimitives(RAS_MeshSlot *ms);

	/**
	 * Renders all primitives from mesh slots contained in this display array
	 * bucket with the geometry instancing way.
	 */
	void IndexPrimitivesInstancing(RAS_DisplayArrayBucket *arrayBucket);

	/**
	 * Renders all primitives from mesh slots contained in this display array
	 * bucket with a single batched display array.
	 */
	void IndexPrimitivesBatching(RAS_DisplayArrayBucket *arrayBucket, const std::vector<void *>& indices, const std::vector<int>& counts);

	/// Render text mesh slot using BLF functions.
	void IndexPrimitivesText(RAS_MeshSlot *ms);
 
	void SetProjectionMatrix(MT_CmMatrix4x4 &mat);

	/* This one should become our final version, methinks. */
	/**
	 * Set the projection matrix for the rasterizer. This projects
	 * from camera coordinates to window coordinates.
	 * \param mat The projection matrix.
	 */
	void SetProjectionMatrix(const MT_Matrix4x4 &mat);

	/**
	 * Sets the modelview matrix.
	 */
	void SetViewMatrix(const MT_Matrix4x4 &mat, const MT_Matrix3x3 &ori,
	                           const MT_Vector3 &pos, const MT_Vector3 &scale, bool perspective);

	/**
	 * Get/Set viewport area
	 */
	void SetViewport(int x, int y, int width, int height);
	void GetViewport(int *rect);

	/**
	 * Set scissor mask
	 */
	void SetScissor(int x, int y, int width, int height);

	/**
	 */
	const MT_Vector3& GetCameraPosition();
	bool GetCameraOrtho();

	/**
	 * Fog
	 */
	void SetFog(short type, float start, float dist, float intensity, const MT_Vector3& color);
	void DisplayFog();
	void EnableFog(bool enable);
	
	/**
	 * \param drawingmode = RAS_WIREFRAME, RAS_SOLID, RAS_SHADOW or RAS_TEXTURED.
	 */
	void SetDrawingMode(DrawType drawingmode);

	/**
	 * \return the current drawing mode: RAS_WIREFRAME, RAS_SOLID RAS_SHADOW or RAS_TEXTURED.
	 */
	DrawType GetDrawingMode();

	/// \param shadowmode = RAS_SHADOW_SIMPLE, RAS_SHADOW_VARIANCE.
	void SetShadowMode(ShadowType shadowmode);

	/// \return the current drawing mode: RAS_SHADOW_SIMPLE, RAS_SHADOW_VARIANCE.
	ShadowType GetShadowMode();

	/**
	 * Sets face culling
	 */
	void SetCullFace(bool enable);

	/**
	 * Sets wireframe mode.
	 */
	void SetLines(bool enable);

	/**
	 */
	double GetTime();

	/**
	 * Generates a projection matrix from the specified frustum.
	 * \param left the left clipping plane
	 * \param right the right clipping plane
	 * \param bottom the bottom clipping plane
	 * \param top the top clipping plane
	 * \param frustnear the near clipping plane
	 * \param frustfar the far clipping plane
	 * \return a 4x4 matrix representing the projection transform.
	 */
	MT_Matrix4x4 GetFrustumMatrix(
	        float left, float right, float bottom, float top,
	        float frustnear, float frustfar,
	        float focallength = 0.0f, bool perspective = true);

	/**
	 * Generates a orthographic projection matrix from the specified frustum.
	 * \param left the left clipping plane
	 * \param right the right clipping plane
	 * \param bottom the bottom clipping plane
	 * \param top the top clipping plane
	 * \param frustnear the near clipping plane
	 * \param frustfar the far clipping plane
	 * \return a 4x4 matrix representing the projection transform.
	 */
	MT_Matrix4x4 GetOrthoMatrix(
	        float left, float right, float bottom, float top,
	        float frustnear, float frustfar);

	/**
	 * Sets the specular color component of the lighting equation.
	 */
	void SetSpecularity(float specX, float specY, float specZ, float specval);
	
	/**
	 * Sets the specular exponent component of the lighting equation.
	 */
	void SetShinyness(float shiny);

	/**
	 * Sets the diffuse color component of the lighting equation.
	 */
	void SetDiffuse(float difX,float difY, float difZ, float diffuse);

	/**
	 * Sets the emissive color component of the lighting equation.
	 */ 
	void SetEmissive(float eX, float eY, float eZ, float e);
	
	void SetAmbientColor(const MT_Vector3& color);
	void SetAmbient(float factor);

	/**
	 * Sets a polygon offset.  z depth will be: z1 = mult*z0 + add
	 */
	void	SetPolygonOffset(float mult, float add);
	
	void DrawDebugLine(SCA_IScene *scene, const MT_Vector3 &from, const MT_Vector3 &to, const MT_Vector4& color);
	void DrawDebugCircle(SCA_IScene *scene, const MT_Vector3 &center, const MT_Scalar radius,
								 const MT_Vector4 &color, const MT_Vector3 &normal, int nsector);
	/** Draw a box depends on minimal and maximal corner.
	 * \param scene The scene owner of this call.
	 * \param pos The box's position.
	 * \param rot The box's orientation.
	 * \param min The box's minimal corner.
	 * \param max The box's maximal corner.
	 * \param color The box's color.
	 */
	void DrawDebugAabb(SCA_IScene *scene, const MT_Vector3& pos, const MT_Matrix3x3& rot,
							  const MT_Vector3& min, const MT_Vector3& max, const MT_Vector4& color);
	void DrawDebugBox(SCA_IScene *scene, MT_Vector3 vertexes[8], const MT_Vector4& color);
	void DrawDebugSolidBox(SCA_IScene *scene, MT_Vector3 vertexes[8], const MT_Vector4& insideColor,
							  const MT_Vector4& outsideColor, const MT_Vector4& lineColor);
	/** Draw a box representing a camera frustum volume.
	 * \param scene The scene owner of this call.
	 * \param projmat The camera projection matrix.
	 * \param viewmat The camera view matrix.
	 */
	void DrawDebugCameraFrustum(SCA_IScene *scene, const MT_Matrix4x4& projmat, const MT_Matrix4x4& viewmat);
	void FlushDebugShapes(SCA_IScene *scene);

	/// Clear the material texture coordinates list used by storages.
	void ClearTexCoords();
	/// Clear the material attributes list used by storages.
	void ClearAttribs();
	/// Clear the material attribut layers list used with material attributes by storages.
	void ClearAttribLayers();
	/// Set the material texture coordinates list used by storages.
	void SetTexCoords(const TexCoGenList& texcos);
	/// Set the material attributes list used by storages.
	void SetAttribs(const TexCoGenList& attribs);
	/// Set the material attribut layers used with material attributes by storages.
	void SetAttribLayers(const RAS_IRasterizer::AttribLayerList& layers);

	const MT_Matrix4x4 &GetViewMatrix() const;
	const MT_Matrix4x4 &GetViewInvMatrix() const;

	void EnableMotionBlur(float motionblurvalue);
	void DisableMotionBlur();
	void SetMotionBlur(unsigned short state);

	void SetAlphaBlend(int alphablend);
	void SetFrontFace(bool ccw);

	void SetInvertFrontFace(bool invert);

	void SetAnisotropicFiltering(short level);
	short GetAnisotropicFiltering();

	void SetMipmapping(MipmapOption val);
	MipmapOption GetMipmapping();

	void SetOverrideShader(OverrideShaderType type);
	OverrideShaderType GetOverrideShader();
	void ActivateOverrideShaderInstancing(void *matrixoffset, void *positionoffset, unsigned int stride);
	void DesactivateOverrideShaderInstancing();

	/// \see KX_RayCast
	bool RayHit(KX_ClientObjectInfo *client, KX_RayCast *result, RayCastTranform *raytransform);
	/// \see KX_RayCast
	bool NeedRayCast(KX_ClientObjectInfo *info, void *data);

	/**
	 * Render Tools
	 */
	void GetTransform(float *origmat, int objectdrawmode, float mat[16]);

	/**
	 * Renders 2D boxes.
	 * \param xco			Position on the screen (origin in lower left corner).
	 * \param yco			Position on the screen (origin in lower left corner).
	 * \param width			Width of the canvas to draw to.
	 * \param height		Height of the canvas to draw to.
	 * \param percentage	Percentage of bar.
	 */
	void RenderBox2D(int xco, int yco, int width, int height, float percentage);

	/**
	 * Renders 3D text string using BFL.
	 * \param fontid	The id of the font.
	 * \param text		The string to render.
	 * \param size		The size of the text.
	 * \param dpi		The resolution of the text.
	 * \param color		The color of the object.
	 * \param mat		The Matrix of the text object.
	 * \param aspect	A scaling factor to compensate for the size.
	 */
	void RenderText3D(
	        int fontid, const std::string& text, int size, int dpi,
	        const float color[4], const float mat[16], float aspect);

	/**
	 * Renders 2D text string.
	 * \param mode      The type of text
	 * \param text		The string to render.
	 * \param xco		Position on the screen (origin in lower left corner).
	 * \param yco		Position on the screen (origin in lower left corner).
	 * \param width		Width of the canvas to draw to.
	 * \param height	Height of the canvas to draw to.
	 */
	void RenderText2D(
	        RAS_TEXT_RENDER_MODE mode, const std::string& text,
	        int xco, int yco, int width, int height);

	void ProcessLighting(bool uselights, const MT_Transform &trans);

	void PushMatrix();
	void PopMatrix();
	void MultMatrix(const float mat[16]);
	void SetMatrixMode(MatrixMode mode);
	void LoadMatrix(const float mat[16]);
	void LoadIdentity();

	RAS_ILightObject *CreateLight();

	void AddLight(RAS_ILightObject *lightobject);

	void RemoveLight(RAS_ILightObject *lightobject);

	/** Set the current off screen depth to the global depth texture used by materials.
	 * In case of mutlisample off screen a blit to RAS_OFFSCREEN_BLIT_DEPTH is procceed.
	 */
	void UpdateGlobalDepthTexture(RAS_OffScreen *offScreen);
	/// Set the global depth texture to an empty texture.
	void ResetGlobalDepthTexture();

	void MotionBlur();

	void SetClientObject(void *obj);

	void SetAuxilaryClientInfo(void *inf);

	/**
	 * Prints information about what the hardware supports.
	 */
	void PrintHardwareInfo();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_IRasterizer")
#endif
};

#endif  /* __RAS_IRASTERIZER_H__ */
