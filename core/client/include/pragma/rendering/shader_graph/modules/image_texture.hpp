// SPDX-FileCopyrightText: (c) 2024 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PRAGMA_SHADER_GRAPH_MODULES_IMAGE_TEXTURE_HPP__
#define __PRAGMA_SHADER_GRAPH_MODULES_IMAGE_TEXTURE_HPP__

#include "pragma/clientdefinitions.h"
#include "pragma/rendering/shader_graph/module.hpp"

import pragma.shadergraph;

namespace pragma::rendering::shader_graph {
	class DLLCLIENT ImageTextureModule : public pragma::rendering::ShaderGraphModule {
	  public:
		enum class PBRBinding : uint32_t {
			IrradianceMap = 0u,
			PrefilterMap,
			BRDFMap,

			Count
		};
		ImageTextureModule(ShaderGraph &shader);
		virtual ~ImageTextureModule() override;
		virtual void InitializeGfxPipelineDescriptorSets() override;
		virtual void RecordBindScene(rendering::ShaderProcessor &shaderProcessor, const pragma::CSceneComponent &scene, const pragma::CRasterizationRendererComponent &renderer, ShaderGameWorld::SceneFlags &inOutSceneFlags) const override;
	  private:
		prosper::DescriptorSetInfo m_descSetInfo;
		std::shared_ptr<prosper::IDescriptorSetGroup> m_dsg;
	};
};

#endif
