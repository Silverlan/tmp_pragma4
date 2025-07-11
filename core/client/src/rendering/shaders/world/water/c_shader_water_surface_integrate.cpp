// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_client.h"
#include "pragma/rendering/shaders/world/water/c_shader_water_surface_integrate.hpp"
#include <shader/prosper_pipeline_create_info.hpp>

using namespace pragma;

decltype(ShaderWaterSurfaceIntegrate::DESCRIPTOR_SET_WATER_PARTICLES) ShaderWaterSurfaceIntegrate::DESCRIPTOR_SET_WATER_PARTICLES = {
  "WATER",
  {prosper::DescriptorSetInfo::Binding {"PARTICLE_DATA", prosper::DescriptorType::StorageBuffer, prosper::ShaderStageFlags::ComputeBit}},
};
decltype(ShaderWaterSurfaceIntegrate::DESCRIPTOR_SET_SURFACE_INFO) ShaderWaterSurfaceIntegrate::DESCRIPTOR_SET_SURFACE_INFO = {&ShaderWaterSurface::DESCRIPTOR_SET_SURFACE_INFO};
ShaderWaterSurfaceIntegrate::ShaderWaterSurfaceIntegrate(prosper::IPrContext &context, const std::string &identifier) : ShaderWaterSurface(context, identifier, "programs/compute/water/water_surface_integrate") {}
void ShaderWaterSurfaceIntegrate::InitializeComputePipeline(prosper::ComputePipelineCreateInfo &pipelineInfo, uint32_t pipelineIdx) { prosper::ShaderCompute::InitializeComputePipeline(pipelineInfo, pipelineIdx); }
void ShaderWaterSurfaceIntegrate::InitializeShaderResources()
{
	prosper::ShaderCompute::InitializeShaderResources();

	AddDescriptorSetGroup(DESCRIPTOR_SET_WATER_PARTICLES);
	AddDescriptorSetGroup(DESCRIPTOR_SET_SURFACE_INFO);
}
