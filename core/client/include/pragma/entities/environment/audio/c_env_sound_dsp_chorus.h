// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __C_ENV_SOUND_DSP_CHORUS_H__
#define __C_ENV_SOUND_DSP_CHORUS_H__
#include "pragma/clientdefinitions.h"
#include "pragma/entities/c_baseentity.h"
#include "pragma/entities/environment/audio/c_env_sound_dsp.h"
#include "pragma/entities/environment/audio/env_sound_dsp_chorus.h"

namespace pragma {
	class DLLCLIENT CSoundDspChorusComponent final : public CBaseSoundDspComponent, public BaseEnvSoundDspChorus {
	  public:
		CSoundDspChorusComponent(BaseEntity &ent) : CBaseSoundDspComponent(ent) {}
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void OnEntitySpawn() override;
		virtual void InitializeLuaObject(lua_State *l) override;
	};
};

class DLLCLIENT CEnvSoundDspChorus : public CBaseEntity {
  public:
	virtual void Initialize() override;
};

#endif
