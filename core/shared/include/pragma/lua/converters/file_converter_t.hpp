// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __LUA_FILE_CONVERTER_T_HPP__
#define __LUA_FILE_CONVERTER_T_HPP__

#include "pragma/lua/converters/file_converter.hpp"
#include "pragma/lua/libraries/lfile.h"

template<class U>
int luabind::default_converter<std::shared_ptr<ufile::IFile>, void>::match(lua_State *L, U, int index)
{
	return Lua::IsFile(L, index) ? 0 : no_match;
}

template<class U>
std::shared_ptr<ufile::IFile> luabind::default_converter<std::shared_ptr<ufile::IFile>>::to_cpp(lua_State *L, U u, int index)
{
	auto *f = luabind::object_cast<LFile *>(luabind::object {luabind::from_stack(L, index)});
	return f->GetHandle();
}

#endif
