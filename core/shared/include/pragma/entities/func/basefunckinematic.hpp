// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __BASEFUNCKINEMATIC_HPP__
#define __BASEFUNCKINEMATIC_HPP__

#include "pragma/entities/components/base_entity_component.hpp"

class BaseEntity;
namespace pragma {
	class DLLNETWORK BaseFuncKinematicComponent : public BaseEntityComponent {
	  public:
		using BaseEntityComponent::BaseEntityComponent;
		virtual void Initialize() override;
		virtual void OnEntitySpawn() override;
		virtual void OnTick(double tDelta) override;
	  protected:
		bool m_bMoving = false;
		float m_kvMoveSpeed = 0.f;
		float m_speed = 0.f;
		EntityHandle m_nextNode;
		std::string m_kvFirstNode;
		std::string m_kvStartSound;
		std::shared_ptr<ALSound> m_startSound;

		pragma::NetEventId m_netEvStartForward = pragma::INVALID_NET_EVENT;
		pragma::NetEventId m_netEvStartBackward = pragma::INVALID_NET_EVENT;
	  protected:
		void UpdateTickPolicy();
		virtual void StartForward();
		virtual void StartBackward();
	  private:
		void MoveToTarget(BaseEntity *node, float speed);
	};
};

#endif
