// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_client.h"
#include "pragma/lua/classes/ldef_entity.h"
#include "luasystem.h"
#include "pragma/entities/components/c_weapon_component.hpp"
#include "pragma/lua/classes/components/c_lentity_components.hpp"
#include "pragma/entities/components/c_character_component.hpp"
#include <pragma/physics/raytraces.h>
#include <pragma/lua/lua_util_component.hpp>
#include <pragma/lua/classes/lproperty.hpp>
#include <pragma/lua/lentity_components_base_types.hpp>
#include <pragma/lua/lentity_components.hpp>
#include <pragma/lua/lua_util_component_stream.hpp>
#include <pragma/lua/converters/game_type_converters_t.hpp>
#include <pragma/entities/components/base_actor_component.hpp>

void Lua::register_cl_character_component(lua_State *l, luabind::module_ &module)
{
	auto def = pragma::lua::create_entity_component_class<pragma::CCharacterComponent, pragma::BaseCharacterComponent>("CharacterComponent");
	module[def];
}
