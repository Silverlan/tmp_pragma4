// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_cengine.h"
#include "pragma/launchparameters.h"
#include "pragma/c_engine.h"

extern DLLCLIENT CEngine *c_engine;
std::optional<bool> g_launchParamWindowedMode {};
std::optional<int> g_launchParamRefreshRate {};
std::optional<bool> g_launchParamNoBorder {};
std::optional<uint32_t> g_launchParamWidth {};
std::optional<uint32_t> g_launchParamHeight {};
std::optional<Color> g_titleBarColor {};
std::optional<Color> g_borderColor {};
bool g_cpuRendering = false;
bool g_windowless = false;
static void LPARAM_windowed(const std::vector<std::string> &argv) { g_launchParamWindowedMode = true; }

static void LPARAM_refresh(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	int freq = atoi(argv[0].c_str());
	if(freq > 0)
		g_launchParamRefreshRate = freq;
}

static void LPARAM_noborder(const std::vector<std::string> &argv) { g_launchParamNoBorder = true; }

static void LPARAM_w(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	g_launchParamWidth = atoi(argv[0].c_str());
}

static void LPARAM_h(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	g_launchParamHeight = atoi(argv[0].c_str());
}

static void LPARAM_fullbright(const std::vector<std::string> &argv) { c_engine->UseFullbrightShader(true); }

static void LPARAM_vk_enable_validation(const std::vector<std::string> &argv) { c_engine->SetGfxAPIValidationEnabled(true); }

static void LPARAM_vk_enable_gfx_diagnostics(const std::vector<std::string> &argv) { c_engine->SetGfxDiagnosticsModeEnabled(true); }

void LPARAM_enable_gfx_api_dump(const std::vector<std::string> &argv);

static void LPARAM_render_api(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	c_engine->SetRenderAPI(argv.front());
}

static void LPARAM_audio_api(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	c_engine->SetAudioAPI(argv.front());
}

extern std::optional<std::vector<std::string>> g_autoExecScripts;
static void LPARAM_auto_exec(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	if(!g_autoExecScripts)
		g_autoExecScripts = std::vector<std::string> {};
	for(auto &arg : argv)
		g_autoExecScripts->push_back(arg);
}

extern std::optional<std::string> g_customWindowIcon;
static void LPARAM_icon(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	g_customWindowIcon = argv.front();
}

static void LPARAM_windowless(const std::vector<std::string> &argv)
{
	auto windowless = true;
	if(!argv.empty())
		windowless = util::to_boolean(argv.front());
	g_windowless = windowless;
}

static void LPARAM_title_bar_color(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	auto strHex = argv.front();
	if(!strHex.empty() && strHex.front() == '#')
		strHex.erase(strHex.begin());
	g_titleBarColor = Color::CreateFromHexColor(strHex);
}

static void LPARAM_border_bar_color(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	auto strHex = argv.front();
	if(!strHex.empty() && strHex.front() == '#')
		strHex.erase(strHex.begin());
	g_borderColor = Color::CreateFromHexColor(strHex);
}

static void LPARAM_cpu_rendering(const std::vector<std::string> &argv) { g_cpuRendering = (argv.empty() || util::to_boolean(argv.front())); }

static void LPARAM_cli(const std::vector<std::string> &argv)
{
	LPARAM_cpu_rendering(argv);
	LPARAM_windowless(argv);
	if(argv.empty() || util::to_boolean(argv.front()))
		LPARAM_audio_api({"dummy"});
}

extern std::optional<std::string> g_waylandLibdecorPlugin;
static void LPARAM_wayland_libdecor_plugin(const std::vector<std::string> &argv)
{
	if(argv.empty())
		return;
	g_waylandLibdecorPlugin = argv.front();
}

REGISTER_LAUNCH_PARAMETER_HELP(-windowed, LPARAM_windowed, "-window -startwindowed -sw", "start in windowed mode");
REGISTER_LAUNCH_PARAMETER(-window, LPARAM_windowed);
REGISTER_LAUNCH_PARAMETER(-startwindowed, LPARAM_windowed);
REGISTER_LAUNCH_PARAMETER(-sw, LPARAM_windowed);

REGISTER_LAUNCH_PARAMETER_HELP(-refresh, LPARAM_refresh, "-refreshrate -freq", "monitor refresh rate in Hz. Only available in fullscreen mode");
REGISTER_LAUNCH_PARAMETER(-refreshrate, LPARAM_refresh);
REGISTER_LAUNCH_PARAMETER(-freq, LPARAM_refresh);

REGISTER_LAUNCH_PARAMETER_HELP(-noborder, LPARAM_noborder, "", "When used with the game set to windowed mode, will make the game act as if in fullscreen mode (no window border).");

REGISTER_LAUNCH_PARAMETER_HELP(-w, LPARAM_w, "<width>", "set the screen width");
REGISTER_LAUNCH_PARAMETER_HELP(-h, LPARAM_h, "<height>", "set the screen height");

REGISTER_LAUNCH_PARAMETER_HELP(-fullbright, LPARAM_fullbright, "", "start in fullbright mode");

REGISTER_LAUNCH_PARAMETER_HELP(-enable_gfx_api_dump, LPARAM_enable_gfx_api_dump, "<1/0>", "Enables or disables graphics API dump.");
REGISTER_LAUNCH_PARAMETER_HELP(-enable_gfx_validation, LPARAM_vk_enable_validation, "<1/0>", "Enables or disables graphics API validation.");
REGISTER_LAUNCH_PARAMETER_HELP(-enable_gfx_diagnostics, LPARAM_vk_enable_gfx_diagnostics, "<1/0>", "Enables or disables GPU diagnostics mode.");
REGISTER_LAUNCH_PARAMETER_HELP(-graphics_api, LPARAM_render_api, "<moduleName>", "Changes the graphics API to use for rendering.");
REGISTER_LAUNCH_PARAMETER_HELP(-audio_api, LPARAM_audio_api, "<moduleName>", "Changes the audio API to use for audio playback.");
REGISTER_LAUNCH_PARAMETER_HELP(-auto_exec, LPARAM_auto_exec, "<script>", "Auto-execute this Lua-script on launch.");
REGISTER_LAUNCH_PARAMETER_HELP(-icon, LPARAM_icon, "<iconPath>", "Path to custom window icon location.");
REGISTER_LAUNCH_PARAMETER_HELP(-windowless, LPARAM_windowless, "<1/0>", "If enabled, Pragma will be launched without a visible window.");
REGISTER_LAUNCH_PARAMETER_HELP(-title_bar_color, LPARAM_title_bar_color, "<hexColor>", "Hex color for the window title bar.");
REGISTER_LAUNCH_PARAMETER_HELP(-border_color, LPARAM_border_bar_color, "<hexColor>", "Hex color for the window border.");
REGISTER_LAUNCH_PARAMETER_HELP(-cpu_rendering, LPARAM_cpu_rendering, "<1/0>", "If enabled, the CPU will be used for rendering instead of GPU.");
REGISTER_LAUNCH_PARAMETER_HELP(-cli, LPARAM_cli, "<1/0>", "If enabled, will automatically enable the options needed to run Pragma in a command-line-interface-only environment.");
REGISTER_LAUNCH_PARAMETER_HELP(-wayland_libdecor_plugin, LPARAM_wayland_libdecor_plugin, "", "If specified, this libdecor plugin will be used for window decoration drawing on Linux with wayland.");
