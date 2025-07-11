// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __S_MODEL_COMPONENT_HPP__
#define __S_MODEL_COMPONENT_HPP__

#include "pragma/serverdefinitions.h"
#include "pragma/entities/components/s_entity_component.hpp"
#include <pragma/entities/components/base_model_component.hpp>

namespace pragma {
	class DLLSERVER SModelComponent final : public BaseModelComponent, public SBaseNetComponent {
	  public:
		SModelComponent(BaseEntity &ent) : BaseModelComponent(ent) {}
		virtual void Initialize() override;
		virtual void SendData(NetPacket &packet, networking::ClientRecipientFilter &rp) override;
		virtual bool ShouldTransmitNetData() const override { return true; }
		virtual void InitializeLuaObject(lua_State *l) override;

		using BaseModelComponent::SetModel;
		virtual bool SetBodyGroup(UInt32 groupId, UInt32 id) override;
		virtual void SetSkin(unsigned int skin) override;
		virtual void SetMaxDrawDistance(float maxDist) override;
	  protected:
		virtual void OnModelChanged(const std::shared_ptr<Model> &model) override;
	};
};

#endif
