// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __C_DEBUG_COMPONENT_HPP__
#define __C_DEBUG_COMPONENT_HPP__

#include "pragma/clientdefinitions.h"
#include "pragma/debug/c_debugoverlay.h"
#include "pragma/entities/components/c_toggle_component.hpp"
#include "pragma/entities/components/c_transform_component.hpp"
#include "pragma/entities/components/c_color_component.hpp"
#include <pragma/entities/components/logic_component.hpp>
#include <pragma/entities/components/base_debug_component.hpp>
#include <pragma/entities/baseentity.h>

class CDebugTextComponentHandleWrapper;
namespace pragma {
	template<class TBaseComponent>
	class DLLCLIENT TCBaseDebugComponent : public TBaseComponent {
	  public:
		TCBaseDebugComponent(BaseEntity &ent) : TBaseComponent(ent) {}
		virtual void Initialize() override
		{
			TBaseComponent::Initialize();
			auto &ent = this->GetEntity();
			ent.template AddComponent<LogicComponent>();

			this->BindEventUnhandled(CToggleComponent::EVENT_ON_TURN_ON, [this](std::reference_wrapper<pragma::ComponentEvent> evData) { ReloadDebugObject(); });
			this->BindEventUnhandled(CToggleComponent::EVENT_ON_TURN_OFF, [this](std::reference_wrapper<pragma::ComponentEvent> evData) { ReloadDebugObject(); });
		}
		virtual void OnEntitySpawn() override
		{
			TBaseComponent::OnEntitySpawn();
			ReloadDebugObject();
		}
		virtual void OnRemove() override
		{
			TBaseComponent::OnRemove();
			m_debugObject = nullptr;
		}
		void SetColorOverride(const Color &color) { m_colorOverride = color; }
		void ClearColorOverride() { m_colorOverride = {}; }
		const std::optional<Color> &GetColorOverride() const { return m_colorOverride; }

		void SetIgnoreDepthBuffer(bool ignoreDepthBuffer)
		{
			m_ignoreDepthBuffer = ignoreDepthBuffer;
			ReloadDebugObject();
		}
		bool ShouldIgnoreDepthBuffer() const { return m_ignoreDepthBuffer; }
	  protected:
		virtual void OnEntityComponentAdded(BaseEntityComponent &component) override
		{
			TBaseComponent::OnEntityComponentAdded(component);
			if(typeid(component) == typeid(CColorComponent)) {
				if(m_colorCallback.IsValid())
					m_colorCallback.Remove();
				m_colorCallback = static_cast<CColorComponent &>(component).GetColorProperty()->AddCallback([this](std::reference_wrapper<const Vector4> oldColor, std::reference_wrapper<const Vector4> color) { ReloadDebugObject(); });
			}
			else if(typeid(component) == typeid(CTransformComponent)) {
				if(m_poseCallback.IsValid())
					m_poseCallback.Remove();
				auto &trC = static_cast<CTransformComponent &>(component);
				m_poseCallback = trC.AddEventCallback(CTransformComponent::EVENT_ON_POSE_CHANGED, [this, &trC](std::reference_wrapper<pragma::ComponentEvent> evData) -> util::EventReply {
					if(umath::is_flag_set(static_cast<pragma::CEOnPoseChanged &>(evData.get()).changeFlags, pragma::TransformChangeFlags::PositionChanged) == false)
						return util::EventReply::Unhandled;
					if(m_debugObject != nullptr)
						m_debugObject->SetPos(trC.GetPosition());
					return util::EventReply::Unhandled;
				});
			}
		}
		virtual void OnEntityComponentRemoved(BaseEntityComponent &component) override
		{
			TBaseComponent::OnEntityComponentRemoved(component);
			if(typeid(component) == typeid(CColorComponent)) {
				if(m_colorCallback.IsValid())
					m_colorCallback.Remove();
			}
			else if(typeid(component) == typeid(CTransformComponent)) {
				if(m_poseCallback.IsValid())
					m_poseCallback.Remove();
			}
		}
		void ReloadDebugObject()
		{
			m_debugObject = nullptr;
			auto &ent = this->GetEntity();
			auto pToggleComponent = ent.template GetComponent<CToggleComponent>();
			if(pToggleComponent.valid() && pToggleComponent->IsTurnedOn() == false)
				return;
			auto color = Color::White.ToVector4();
			if(m_colorOverride)
				color = m_colorOverride->ToVector4();
			else {
				auto pColorComponent = ent.template GetComponent<CColorComponent>();
				if(pColorComponent.valid())
					color = pColorComponent->GetColor();
			}
			auto pTrComponent = ent.template GetComponent<CTransformComponent>();
			auto pos = Vector3 {};
			if(pTrComponent.valid())
				pos = pTrComponent->GetPosition();
			DebugRenderInfo renderInfo {color};
			renderInfo.pose.SetOrigin(pos);
			renderInfo.ignoreDepthBuffer = ShouldIgnoreDepthBuffer();
			ReloadDebugObject(color, pos, renderInfo);
		}
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) = 0;
		std::shared_ptr<DebugRenderer::BaseObject> m_debugObject = nullptr;
		std::optional<Color> m_colorOverride {};
		bool m_ignoreDepthBuffer = false;
		CallbackHandle m_poseCallback = {};
		CallbackHandle m_colorCallback = {};
	};

	////////////////

	class DLLCLIENT CDebugTextComponent final : public TCBaseDebugComponent<BaseDebugTextComponent>, public CBaseNetComponent {
	  public:
		CDebugTextComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugTextComponent>(ent) {}
		virtual void SetText(const std::string &text) override;
		virtual void SetSize(float size) override;
		virtual Bool ReceiveNetEvent(pragma::NetEventId eventId, NetPacket &packet) override;
		virtual void ReceiveData(NetPacket &packet) override;
		virtual bool ShouldTransmitNetData() const override { return true; }
		virtual void InitializeLuaObject(lua_State *l) override;
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugTextComponent>::ReloadDebugObject;
		std::string m_text = {};
	};

	////////////////

	class DLLCLIENT CBaseDebugOutlineComponent : public CBaseNetComponent {
	  public:
		virtual void ReceiveData(NetPacket &packet) override;
		virtual bool ShouldTransmitNetData() const override { return true; }
	};

	////////////////

	class DLLCLIENT CDebugPointComponent final : public TCBaseDebugComponent<BaseDebugPointComponent>, public CBaseNetComponent {
	  public:
		CDebugPointComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugPointComponent>(ent) {}
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void InitializeLuaObject(lua_State *l) override;
		virtual bool ShouldTransmitNetData() const override { return true; }
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugPointComponent>::ReloadDebugObject;
	};

	////////////////

	class DLLCLIENT CDebugLineComponent final : public TCBaseDebugComponent<BaseDebugLineComponent>, public CBaseNetComponent {
	  public:
		CDebugLineComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugLineComponent>(ent) {}
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void InitializeLuaObject(lua_State *l) override;
		virtual bool ShouldTransmitNetData() const override { return true; }
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugLineComponent>::ReloadDebugObject;
	};

	////////////////

	class DLLCLIENT CDebugBoxComponent final : public TCBaseDebugComponent<BaseDebugBoxComponent>, public CBaseDebugOutlineComponent {
	  public:
		CDebugBoxComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugBoxComponent>(ent) {}
		virtual void SetBounds(const Vector3 &min, const Vector3 &max) override;

		virtual Bool ReceiveNetEvent(pragma::NetEventId eventId, NetPacket &packet) override;
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void InitializeLuaObject(lua_State *l) override;
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugBoxComponent>::ReloadDebugObject;
	};

	////////////////

	class DLLCLIENT CDebugSphereComponent final : public TCBaseDebugComponent<BaseDebugSphereComponent>, public CBaseDebugOutlineComponent {
	  public:
		CDebugSphereComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugSphereComponent>(ent) {}
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void InitializeLuaObject(lua_State *l) override;
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugSphereComponent>::ReloadDebugObject;
	};

	////////////////

	class DLLCLIENT CDebugConeComponent final : public TCBaseDebugComponent<BaseDebugConeComponent>, public CBaseDebugOutlineComponent {
	  public:
		CDebugConeComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugConeComponent>(ent) {}
		virtual void SetConeAngle(float angle) override;
		virtual void SetStartRadius(float radius) override;

		virtual Bool ReceiveNetEvent(pragma::NetEventId eventId, NetPacket &packet) override;
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void InitializeLuaObject(lua_State *l) override;
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugConeComponent>::ReloadDebugObject;
	};

	////////////////

	class DLLCLIENT CDebugCylinderComponent final : public TCBaseDebugComponent<BaseDebugCylinderComponent>, public CBaseDebugOutlineComponent {
	  public:
		CDebugCylinderComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugCylinderComponent>(ent) {}
		virtual void SetLength(float length) override;

		virtual Bool ReceiveNetEvent(pragma::NetEventId eventId, NetPacket &packet) override;
		virtual void ReceiveData(NetPacket &packet) override;
		virtual void InitializeLuaObject(lua_State *l) override;
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugCylinderComponent>::ReloadDebugObject;
	};

	////////////////

	class DLLCLIENT CDebugPlaneComponent final : public TCBaseDebugComponent<BaseDebugPlaneComponent>, public CBaseNetComponent {
	  public:
		CDebugPlaneComponent(BaseEntity &ent) : TCBaseDebugComponent<BaseDebugPlaneComponent>(ent) {}
		virtual void InitializeLuaObject(lua_State *l) override;
		virtual void ReceiveData(NetPacket &packet) override;
		virtual bool ShouldTransmitNetData() const override { return true; }
	  protected:
		virtual void ReloadDebugObject(Color color, const Vector3 &pos, DebugRenderInfo renderInfo) override;
		using TCBaseDebugComponent<BaseDebugPlaneComponent>::ReloadDebugObject;
	};
};

#endif
