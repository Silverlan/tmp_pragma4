// SPDX-FileCopyrightText: (c) 2023 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __CVAR_CALLBACK_HPP__
#define __CVAR_CALLBACK_HPP__

#include "pragma/networkdefinitions.h"
#include <functional>
#include <sharedutils/functioncallback.h>

enum class Type : uint8_t;
class NetworkState;
class ConVar;
class DLLNETWORK CvarCallback {
  public:
	CvarCallback();
	CvarCallback(LuaFunction fc);
	CvarCallback(const std::function<void(NetworkState *, const ConVar &, const void *, const void *)> &f);
	void SetFunction(const std::function<void(NetworkState *, const ConVar &, const void *, const void *)> &f);
  private:
	bool m_isLuaCallback = false;
	CallbackHandle m_callbackHandle;
  public:
	bool IsLuaFunction() const;
	CallbackHandle &GetFunction();
};

#endif
