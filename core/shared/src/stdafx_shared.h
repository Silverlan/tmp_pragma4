// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifdef _WIN32
//#pragma message("Compiling precompiled headers.\n")
#define _WIN32_WINNT 0x0501
#define NOMINMAX
#include <Windows.h>
#endif

#include <deque>
#include <cmath>
#include <climits>
#include <cfloat>
#include <cassert>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <memory.h>
#include <stdarg.h>
#include <stddef.h>
#include <pragma/lua/luaapi.h>
#include <fstream>
#include <cinttypes>
#include <queue>
#include <cstring>
#include <assert.h>
#ifdef __linux__
#include <dlfcn.h>
#endif
#include <cstdio>
#include <any>

#include <mathutil/glmutil.h>
#include <mathutil/eulerangles.h>
#include <mathutil/uquat.h>
#include <mathutil/uvec.h>
#ifdef _WIN32
#include <pragma/engine.h>
#endif
#include "pragma/networkstate/networkstate.h"
#include <pragma/game/game.h>
#include "pragma/entities/baseentity.h"
#include "pragma/physics/physobj.h"
#include <pragma/lua/luaapi.h>
#include "pragma/audio/alsound.h"
#include <pragma/console/convars.h>
#include <pragma/console/conout.h>
#include <sharedutils/util.h>
#include <sharedutils/util_string.h>
#include <mathutil/umath.h>
#include "pragma/entities/entity_component_manager.hpp"
#include "pragma/entities/entity_component_system.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#ifdef __linux__
#include "pragma/lua/policies/core_policies.hpp"
#include "pragma/lua/policies/default_parameter_policy.hpp"
#include "pragma/lua/policies/generic_policy.hpp"
#include "pragma/lua/policies/shared_from_this_policy.hpp"
#ifdef _WIN32
#include <pragma/entities/components/panima_component_channel_submitter_def.hpp>
#endif

#include "pragma/lua/converters/alias_converter_t.hpp"
#include "pragma/lua/converters/cast_converter_t.hpp"
#include "pragma/lua/converters/file_converter_t.hpp"
#include "pragma/lua/converters/game_type_converters_t.hpp"
#include "pragma/lua/converters/optional_converter_t.hpp"
#include "pragma/lua/converters/pair_converter_t.hpp"
#include "pragma/lua/converters/property_converter_t.hpp"
#include "pragma/lua/converters/string_view_converter_t.hpp"
#include "pragma/lua/converters/thread_pool_converter_t.hpp"
#include "pragma/lua/converters/vector_converter_t.hpp"
#ifdef _WIN32
#include <pragma/entities/entity_component_manager_t.hpp>
#endif
#include <pragma/entities/entity_component_system_t.hpp>
#endif
