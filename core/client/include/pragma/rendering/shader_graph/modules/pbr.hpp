// SPDX-FileCopyrightText: (c) 2024 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PRAGMA_SHADER_GRAPH_MODULES_PBR_HPP__
#define __PRAGMA_SHADER_GRAPH_MODULES_PBR_HPP__

#include "pragma/clientdefinitions.h"
#include "pragma/rendering/shader_graph/module.hpp"

import pragma.shadergraph;

namespace pragma::rendering::shader_graph {
	class DLLCLIENT PbrModule : public pragma::rendering::ShaderGraphModule {
	  public:
		enum class PBRBinding : uint32_t {
			IrradianceMap = 0u,
			PrefilterMap,
			BRDFMap,

			Count
		};
		PbrModule(ShaderGraph &shader);
		virtual ~PbrModule() override;
		virtual void InitializeGfxPipelineDescriptorSets() override;
		virtual void RecordBindScene(rendering::ShaderProcessor &shaderProcessor, const pragma::CSceneComponent &scene, const pragma::CRasterizationRendererComponent &renderer, ShaderGameWorld::SceneFlags &inOutSceneFlags) const override;
		prosper::IDescriptorSet *GetReflectionProbeDescriptorSet(const pragma::CSceneComponent &scene, float &outIblStrength, ShaderGameWorld::SceneFlags &inOutSceneFlags) const;
		prosper::IDescriptorSet &GetDefaultPbrDescriptorSet() const;
	  private:
		prosper::DescriptorSetInfo m_pbrDescSetInfo;
		static std::shared_ptr<prosper::IDescriptorSetGroup> g_defaultPbrDsg;
		static size_t g_instanceCount;
	};
};

#endif
