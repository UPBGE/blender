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
 * Contributor(s): Pierluigi Grassi, Porteries Tristan.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Rasterizer/RAS_2DFilterManager.cpp
 *  \ingroup bgerast
 */

#include "RAS_ICanvas.h"
#include "RAS_2DFilterManager.h"
#include "RAS_2DFilter.h"

#include "BLI_utildefines.h" // for STRINGIFY

#include <iostream>

#include "glew-mx.h"

#include "RAS_OpenGLFilters/RAS_Blur2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Sharpen2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Dilation2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Erosion2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Laplacian2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Sobel2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Prewitt2DFilter.h"
#include "RAS_OpenGLFilters/RAS_GrayScale2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Sepia2DFilter.h"
#include "RAS_OpenGLFilters/RAS_Invert2DFilter.h"


RAS_2DFilterManager::RAS_2DFilterManager()
{
}

RAS_2DFilterManager::~RAS_2DFilterManager()
{
	for (RAS_PassTo2DFilter::iterator it = m_filters.begin(), end = m_filters.end(); it != end; ++it) {
		RAS_2DFilter *filter = it->second;
		delete filter;
	}
}

RAS_2DFilter *RAS_2DFilterManager::AddFilter(RAS_2DFilterData& filterData)
{
	RAS_2DFilter *filter = CreateFilter(filterData);

	m_filters[filterData.filterPassIndex] = filter;

	return filter;
}

void RAS_2DFilterManager::RemoveFilterPass(unsigned int passIndex)
{
	m_filters.erase(passIndex);
}

RAS_2DFilter *RAS_2DFilterManager::GetFilterPass(unsigned int passIndex)
{
	RAS_PassTo2DFilter::iterator it = m_filters.find(passIndex);
	return (it != m_filters.end()) ? it->second : NULL;
}

void RAS_2DFilterManager::RenderFilters(RAS_IRasterizer *rasty, RAS_ICanvas *canvas)
{
	for (RAS_PassTo2DFilter::iterator it = m_filters.begin(), end = m_filters.end(); it != end; ++it) {
		RAS_2DFilter *filter = it->second;
		filter->Start(rasty, canvas);
		filter->End();
	}
}

RAS_2DFilter *RAS_2DFilterManager::CreateFilter(RAS_2DFilterData& filterData)
{
	RAS_2DFilter *result = NULL;
	const char *shaderSource = NULL;
	switch(filterData.filterMode) {
		case RAS_2DFilterManager::FILTER_MOTIONBLUR:
			break;
		case RAS_2DFilterManager::FILTER_BLUR:
			shaderSource = BlurFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_SHARPEN:
			shaderSource = SharpenFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_DILATION:
			shaderSource = DilationFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_EROSION:
			shaderSource = ErosionFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_LAPLACIAN:
			shaderSource = LaplacianFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_SOBEL:
			shaderSource = SobelFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_PREWITT:
			shaderSource = PrewittFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_GRAYSCALE:
			shaderSource = GrayScaleFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_SEPIA:
			shaderSource = SepiaFragmentShader;
			break;
		case RAS_2DFilterManager::FILTER_INVERT:
			shaderSource = InvertFragmentShader;
			break;
	}
	if (!shaderSource) {
		if(filterData.filterMode == RAS_2DFilterManager::FILTER_CUSTOMFILTER) {
			result = NewFilter(filterData);
		}
		else {
			std::cout << "Cannot create filter for mode: " << filterData.filterMode << "." << std::endl;
		}
	}
	else {
		filterData.shaderText = shaderSource;
		result = NewFilter(filterData);
	}
	return result;
}

RAS_2DFilter *RAS_2DFilterManager::NewFilter(RAS_2DFilterData& filterData)
{
	return new RAS_2DFilter(filterData);
}
