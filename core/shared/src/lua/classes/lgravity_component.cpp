// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_shared.h"
#include "pragma/lua/lentity_components.hpp"
#include "pragma/lua/converters/game_type_converters_t.hpp"

#include "pragma/lua/ostream_operator_alias.hpp"
#include "pragma/lua/lua_util_component.hpp"

namespace Lua {
	namespace Gravity {
		static void CalcBallisticVelocity(lua_State *l, pragma::GravityComponent &hEnt, const Vector3 &origin, const Vector3 &destPos, float fireAngle, float maxSpeed, float spread, float maxPitch, float maxYaw);
	};
};

#ifdef __linux__
DEFINE_OSTREAM_OPERATOR_NAMESPACE_ALIAS(pragma, BaseEntityComponent);
#endif

void Lua::register_gravity_component(luabind::module_ &module)
{
	auto def = pragma::lua::create_entity_component_class<pragma::GravityComponent, pragma::BaseEntityComponent>("GravityComponent");
	def.def("SetGravityScale", &pragma::GravityComponent::SetGravityScale);
	def.def("SetGravityOverride", static_cast<void (pragma::GravityComponent::*)(const Vector3 &, float)>(&pragma::GravityComponent::SetGravityOverride));
	def.def("SetGravityOverride", static_cast<void (pragma::GravityComponent::*)(const Vector3 &)>(&pragma::GravityComponent::SetGravityOverride));
	def.def("SetGravityOverride", static_cast<void (pragma::GravityComponent::*)(float)>(&pragma::GravityComponent::SetGravityOverride));
	def.def("SetGravityOverride", static_cast<void (pragma::GravityComponent::*)()>(&pragma::GravityComponent::SetGravityOverride));
	def.def("HasGravityForceOverride", &pragma::GravityComponent::HasGravityForceOverride);
	def.def("HasGravityDirectionOverride", &pragma::GravityComponent::HasGravityDirectionOverride);
	def.def("GetGravityDirection", &pragma::GravityComponent::GetGravityDirection);
	def.def("GetGravity", &pragma::GravityComponent::GetGravity);
	def.def("GetGravityForce", &pragma::GravityComponent::GetGravityForce);
	def.def("CalcBallisticVelocity", &Lua::Gravity::CalcBallisticVelocity);
	module[def];
}

void Lua::Gravity::CalcBallisticVelocity(lua_State *l, pragma::GravityComponent &hEnt, const Vector3 &origin, const Vector3 &destPos, float fireAngle, float maxSpeed, float spread, float maxPitch, float maxYaw)
{
	Vector3 vel;
	auto b = hEnt.CalcBallisticVelocity(origin, destPos, fireAngle, maxSpeed, spread, maxPitch, maxYaw, vel);
	Lua::PushBool(l, b);
	if(b == true)
		Lua::Push<Vector3>(l, vel);
}
