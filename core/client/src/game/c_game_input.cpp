// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_client.h"
#include "pragma/c_engine.h"
#include "pragma/game/c_game.h"

#ifdef _MSC_VER
namespace pragma::string {
	class Utf8String;
	class Utf8StringView;
	class Utf8StringArg;
};
#endif

#include <pragma/lua/luacallback.h>
#include "luasystem.h"
#include <pragma/lua/luafunction_call.h>
#include <pragma/lua/converters/vector_converter_t.hpp>
#include <sharedutils/util_file.h>
#include <sharedutils/scope_guard.h>
#include <prosper_window.hpp>

import pragma.string.unicode;

extern DLLCLIENT CEngine *c_engine;

Bool CGame::RawMouseInput(pragma::platform::MouseButton button, pragma::platform::KeyState state, pragma::platform::Modifier mods)
{
	if(m_inputCallbackHandler.CallLuaEvents<int, int, int>("OnMouseInput", static_cast<int>(button), static_cast<int>(state), static_cast<int>(mods)) == util::EventReply::Handled)
		return false;
	return true;
}
Bool CGame::RawKeyboardInput(pragma::platform::Key key, int, pragma::platform::KeyState state, pragma::platform::Modifier mods, float magnitude)
{
	if(m_inputCallbackHandler.CallLuaEvents<int, int, int>("OnKeyboardInput", static_cast<int>(key), static_cast<int>(state), static_cast<int>(mods)) == util::EventReply::Handled)
		return false;
	return true;
}
Bool CGame::RawCharInput(unsigned int c)
{
	if(m_inputCallbackHandler.CallLuaEvents<int>("OnCharInput", c) == util::EventReply::Handled)
		return false;
	return true;
}
Bool CGame::RawScrollInput(Vector2 offset)
{
	if(m_inputCallbackHandler.CallLuaEvents<float, float>("OnScrollInput", CFloat(offset.x), CFloat(offset.y)) == util::EventReply::Handled)
		return false;
	return true;
}

Bool CGame::MouseInput(pragma::platform::MouseButton button, pragma::platform::KeyState state, pragma::platform::Modifier mods)
{
	auto r = false;
	int bt = static_cast<int>(button) - static_cast<int>(pragma::platform::Key::Last);
	if(CallLuaCallbacks<bool, int, int, int>("OnMouseInput", &r, bt, static_cast<int>(state), static_cast<int>(mods)) == CallbackReturnType::HasReturnValue)
		return r;
	return true;
}
Bool CGame::KeyboardInput(pragma::platform::Key key, int, pragma::platform::KeyState state, pragma::platform::Modifier mods, float magnitude)
{
	auto r = false;
	if(CallLuaCallbacks<bool, int, int, int>("OnKeyboardInput", &r, static_cast<int>(key), static_cast<int>(state), static_cast<int>(mods)) == CallbackReturnType::HasReturnValue)
		return r;
	return true;
}
Bool CGame::CharInput(unsigned int c)
{
	auto r = false;
	if(CallLuaCallbacks<bool, int>("OnCharInput", &r, c) == CallbackReturnType::HasReturnValue)
		return r;
	return true;
}
Bool CGame::ScrollInput(Vector2 offset)
{
	auto r = false;
	if(CallLuaCallbacks<bool, float, float>("OnScrollInput", &r, CFloat(offset.x), CFloat(offset.y)) == CallbackReturnType::HasReturnValue)
		return r;
	return true;
}

bool CGame::OnWindowShouldClose(prosper::Window &window)
{
	bool ret = true;
	CallLuaCallbacks<bool, prosper::Window *>("OnWindowShouldClose", &ret, &window);
	return ret;
}
void CGame::OnPreedit(prosper::Window &window, const pragma::string::Utf8String &preeditString, const std::vector<int> &blockSizes, int focusedBlock, int caret)
{
	CallLuaCallbacks<void, prosper::Window *, std::string, std::vector<int>, int, int>("OnPreedit", &window, preeditString.cpp_str(), blockSizes, focusedBlock, caret);
}
void CGame::OnIMEStatusChanged(prosper::Window &window, bool imeEnabled) { CallLuaCallbacks<void, prosper::Window *, bool>("OnIMEStatusChanged", &window, imeEnabled); }
void CGame::OnDragEnter(prosper::Window &window) { CallLuaCallbacks<bool, prosper::Window *>("OnWindowDragEnter", &window); }
void CGame::OnDragExit(prosper::Window &window) { CallLuaCallbacks<bool, prosper::Window *>("OnWindowDragExit", &window); }
void CGame::OnFilesDropped(std::vector<std::string> &files)
{
	auto *l = GetLuaState();
	auto t = Lua::CreateTable(l);
	auto &droppedFiles = c_engine->GetDroppedFiles();
	for(auto i = decltype(droppedFiles.size()) {0}; i < droppedFiles.size(); ++i) {
		Lua::PushInt(l, i + 1);
		Lua::PushString(l, droppedFiles[i].fileName);
		Lua::SetTableValue(l, t);
	}
	auto o = luabind::object(luabind::from_stack(l, -1));
	CallLuaCallbacks<void, luabind::object>("OnFilesDropped", o);
}
