// SPDX-FileCopyrightText: (c) 2024 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __INPUT_MOVEMENT_CONTROLLER_COMPONENT_HPP__
#define __INPUT_MOVEMENT_CONTROLLER_COMPONENT_HPP__

#include "pragma/entities/components/base_entity_component.hpp"

namespace pragma {
	class ActionInputControllerComponent;
	class MovementComponent;
	class DLLNETWORK InputMovementControllerComponent final : public BaseEntityComponent {
	  public:
		InputMovementControllerComponent(BaseEntity &ent);
		virtual void Initialize() override;
		virtual void OnRemove() override;

		void SetActionInputController(ActionInputControllerComponent *controller);
		ActionInputControllerComponent *GetActionInputController();
		const ActionInputControllerComponent *GetActionInputController() const { return const_cast<InputMovementControllerComponent *>(this)->GetActionInputController(); }
	  private:
		virtual void InitializeLuaObject(lua_State *lua) override;
		virtual void OnEntityComponentAdded(BaseEntityComponent &component) override;
		virtual void OnEntityComponentRemoved(BaseEntityComponent &component) override;
		void UpdateMovementProperties();
		ComponentHandle<ActionInputControllerComponent> m_actionInputController {};
		MovementComponent *m_movementComponent = nullptr;
	};
};

#endif
