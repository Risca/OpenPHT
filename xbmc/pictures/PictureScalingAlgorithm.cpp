/*
 *      Copyright (C) 2015 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <algorithm>

extern "C" {
#include "libswscale/swscale.h"
}

#include "PictureScalingAlgorithm.h"
#include "utils/StringUtils.h"

CPictureScalingAlgorithm::Algorithm CPictureScalingAlgorithm::Default = CPictureScalingAlgorithm::Bicubic;

CPictureScalingAlgorithm::AlgorithmMap CPictureScalingAlgorithm::CreateAlgorithmMapping()
{
  AlgorithmMap map;
  map[FastBilinear]     = ScalingAlgorithm("fast_bilinear",     SWS_FAST_BILINEAR);
  map[FastBilinear]     = ScalingAlgorithm("fast_bilinear",     SWS_FAST_BILINEAR);
  map[Bilinear]         = ScalingAlgorithm("bilinear",          SWS_BILINEAR);
  map[Bicubic]          = ScalingAlgorithm("bicubic",           SWS_BICUBIC);
  map[Experimental]     = ScalingAlgorithm("experimental",      SWS_X);
  map[NearestNeighbor]  = ScalingAlgorithm("nearest_neighbor",  SWS_POINT);
  map[AveragingArea]    = ScalingAlgorithm("averaging_area",    SWS_AREA);
  map[Bicublin]         = ScalingAlgorithm("bicublin",          SWS_BICUBLIN);
  map[Gaussian]         = ScalingAlgorithm("gaussian",          SWS_GAUSS);
  map[Sinc]             = ScalingAlgorithm("sinc",              SWS_SINC);
  map[Lanczos]          = ScalingAlgorithm("lanczos",           SWS_LANCZOS);
  map[BicubicSpline]    = ScalingAlgorithm("bicubic_spline",    SWS_SPLINE);
  return map;
}
CPictureScalingAlgorithm::AlgorithmMap CPictureScalingAlgorithm::m_algorithms = CPictureScalingAlgorithm::CreateAlgorithmMapping();

CPictureScalingAlgorithm::Algorithm CPictureScalingAlgorithm::FromString(const std::string& scalingAlgorithm)
{
  for (AlgorithmMap::const_iterator algo = m_algorithms.begin(); algo != m_algorithms.end(); ++algo) {
    if (StringUtils::EqualsNoCase(algo->second.name, scalingAlgorithm))
      return algo->first;
  }
  return NoAlgorithm;
}

std::string CPictureScalingAlgorithm::ToString(Algorithm scalingAlgorithm)
{
  const AlgorithmMap::const_iterator& algorithm = m_algorithms.find(scalingAlgorithm);
  if (algorithm != m_algorithms.end())
    return algorithm->second.name;

  return "";
}

int CPictureScalingAlgorithm::ToSwscale(const std::string& scalingAlgorithm)
{
  return ToSwscale(FromString(scalingAlgorithm));
}

int CPictureScalingAlgorithm::ToSwscale(Algorithm scalingAlgorithm)
{
  const AlgorithmMap::const_iterator& algorithm = m_algorithms.find(scalingAlgorithm);
  if (algorithm != m_algorithms.end())
    return algorithm->second.swscale;

  return ToSwscale(Default);
}
