// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_client.h"
#include "pragma/lua/classes/c_lshader.h"
#include "pragma/rendering/shaders/c_shader_lua.hpp"
#include <shader/prosper_pipeline_create_info.hpp>
#include <buffers/prosper_buffer.hpp>
#include <prosper_command_buffer.hpp>

void Lua::ComputePipelineCreateInfo::AddSpecializationConstant(lua_State *l, prosper::ComputePipelineCreateInfo &pipelineInfo, uint32_t constantId, ::DataStream &ds) { Lua::PushBool(l, pipelineInfo.AddSpecializationConstant(constantId, ds->GetSize(), ds->GetData())); }

void Lua::Shader::Compute::RecordDispatch(lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState, uint32_t x, uint32_t y, uint32_t z) { Lua::PushBool(l, shader.RecordDispatch(bindState, x, y, z)); }
void Lua::Shader::Compute::RecordBeginCompute(lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState, uint32_t pipelineIdx) { Lua::PushBool(l, shader.RecordBeginCompute(bindState, pipelineIdx)); }
void Lua::Shader::Compute::RecordCompute(lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState)
{
	shader.RecordCompute(bindState);
	Lua::PushBool(l, true);
}
void Lua::Shader::Compute::RecordEndCompute(lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState)
{
	shader.RecordEndCompute(bindState);
	Lua::PushBool(l, true);
}
