// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_client.h"
#include "pragma/lua/classes/c_ltexture.h"
#include "luasystem.h"
#include "pragma/lua/libraries/c_lua_vulkan.h"

std::shared_ptr<prosper::Texture> Lua::Texture::GetVkTexture(lua_State *l, ::Texture &tex)
{
	if(tex.HasValidVkTexture() == false)
		return nullptr;
	return tex.GetVkTexture();
}
