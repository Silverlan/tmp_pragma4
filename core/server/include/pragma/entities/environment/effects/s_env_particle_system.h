// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __S_ENV_PARTICLE_SYSTEM_H__
#define __S_ENV_PARTICLE_SYSTEM_H__
#include "pragma/serverdefinitions.h"
#include "pragma/entities/s_baseentity.h"
#include "pragma/entities/environment/effects/env_particle_system.h"
#include "pragma/entities/components/s_entity_component.hpp"

namespace pragma {
	class DLLSERVER SParticleSystemComponent final : public BaseEnvParticleSystemComponent, public SBaseNetComponent {
	  public:
		SParticleSystemComponent(BaseEntity &ent) : BaseEnvParticleSystemComponent(ent) {}
		virtual void SendData(NetPacket &packet, networking::ClientRecipientFilter &rp) override;
		virtual void SetContinuous(bool b) override;
		virtual bool ShouldTransmitNetData() const override { return true; }
		virtual void InitializeLuaObject(lua_State *l) override;
	};
};

class DLLSERVER EnvParticleSystem : public SBaseEntity {
  public:
	virtual void Initialize() override;
};

#endif
