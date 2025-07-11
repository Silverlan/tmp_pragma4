// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_shared.h"
#include <pragma/engine.h>
#include "pragma/game/gamemode/gamemodemanager.h"
#include <pragma/game/game.h>
#include "luasystem.h"
#include "pragma/game/damageinfo.h"
#include "pragma/lua/luafunction_call.h"
#include "pragma/entities/components/base_player_component.hpp"
#include "pragma/entities/components/base_ai_component.hpp"

extern DLLNETWORK Engine *engine;

std::string Game::GetGameModeScriptDirectoryPath() const
{
	auto *info = GetGameMode();
	if(info == nullptr)
		return "";
	auto &id = info->id;
	return "gamemodes\\" + id;
}
std::string Game::GetGameModeScriptDirectoryNetworkPath() const
{
	auto path = GetGameModeScriptDirectoryPath();
	if(path.empty())
		return "";
	return path + '\\' + GetLuaNetworkDirectoryName();
}
std::string Game::GetGameModeScriptFilePath() const
{
	auto path = GetGameModeScriptDirectoryNetworkPath();
	if(path.empty())
		return "";
	return path + '\\' + GetLuaNetworkFileName();
}

void Game::ReloadGameModeScripts()
{
	auto fileName = GetGameModeScriptFilePath();
	if(fileName.empty())
		return;
	ExecuteLuaFile(fileName);
}

bool Game::InitializeGameMode()
{
	auto *info = GetGameMode();
	if(info == nullptr)
		return false;
	ReloadGameModeScripts();

	auto *entGamemode = CreateEntity("gamemode");
	if(entGamemode == nullptr)
		return false;
	m_entGamemode = entGamemode->GetHandle();
	entGamemode->AddComponent(info->component_name);
	entGamemode->Spawn();
	return true;
}

bool Game::IsGameModeInitialized() const
{
	if(m_entGamemode.valid() == false)
		return false;
	return true;
}
