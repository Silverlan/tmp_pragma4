// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __C_LIQUID_SURFACE_SIMULATION_COMPONENT_HPP__
#define __C_LIQUID_SURFACE_SIMULATION_COMPONENT_HPP__

#include "pragma/clientdefinitions.h"
#include "pragma/entities/components/c_entity_component.hpp"
#include <pragma/entities/components/liquid/base_liquid_surface_simulation_component.hpp>

class PhysWaterSurfaceSimulator;
class CWaterSurface;
namespace pragma {
	class DLLCLIENT CLiquidSurfaceSimulationComponent final : public BaseLiquidSurfaceSimulationComponent, public CBaseNetComponent {
	  public:
		CLiquidSurfaceSimulationComponent(BaseEntity &ent);
		virtual ~CLiquidSurfaceSimulationComponent() override;
		virtual bool ShouldTransmitNetData() const override { return true; }
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void Initialize() override;
		virtual void InitializeLuaObject(lua_State *l) override;
		virtual void ReloadSurfaceSimulator() override;
		CWaterSurface *GetSurfaceEntity() const;
	  protected:
		virtual void OnEntitySpawn() override;
		virtual std::shared_ptr<PhysWaterSurfaceSimulator> InitializeSurfaceSimulator(const Vector2 &min, const Vector2 &max, float originY) override;
		mutable EntityHandle m_hWaterSurface = {};
	};
};

#endif
