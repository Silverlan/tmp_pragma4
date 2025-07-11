// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_shared.h"
#include "pragma/entities/environment/effects/env_smoke_trail.h"
#include "pragma/util/util_handled.hpp"
#include "pragma/entities/baseentity_events.hpp"
#include <algorithm>

using namespace pragma;

void BaseEnvSmokeTrailComponent::Initialize()
{
	BaseEntityComponent::Initialize();

	BindEvent(BaseEntity::EVENT_HANDLE_KEY_VALUE, [this](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
		auto &kvData = static_cast<CEKeyValueData &>(evData.get());
		if(ustring::compare<std::string>(kvData.key, "speed", false))
			m_speed = ustring::to_float(kvData.value);
		else if(ustring::compare<std::string>(kvData.key, "distance", false))
			m_distance = ustring::to_float(kvData.value);
		else if(ustring::compare<std::string>(kvData.key, "min_sprite_size", false))
			m_minSpriteSize = ustring::to_float(kvData.value);
		else if(ustring::compare<std::string>(kvData.key, "max_sprite_size", false))
			m_maxSpriteSize = ustring::to_float(kvData.value);
		else if(ustring::compare<std::string>(kvData.key, "material", false))
			m_material = kvData.value;
		else
			return util::EventReply::Unhandled;
		return util::EventReply::Handled;
	});

	auto &ent = GetEntity();
	ent.AddComponent("toggle");
}
