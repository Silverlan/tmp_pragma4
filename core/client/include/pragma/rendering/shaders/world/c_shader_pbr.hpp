// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __C_SHADER_PBR_HPP__
#define __C_SHADER_PBR_HPP__

#include "pragma/rendering/shaders/world/c_shader_textured.hpp"

class Texture;
namespace pragma {
	class DLLCLIENT ShaderPBR : public ShaderGameWorldLightingPass {
	  public:
		static prosper::DescriptorSetInfo DESCRIPTOR_SET_PBR;

		enum class MaterialBinding : uint32_t {
			MaterialSettings = umath::to_integral(ShaderGameWorldLightingPass::MaterialBinding::MaterialSettings),
			AlbedoMap,
			NormalMap,
			RMAMap,
			EmissionMap,
			ParallaxMap,
			WrinkleStretchMap,
			WrinkleCompressMap,
			ExponentMap,

			Count
		};

		enum class PBRBinding : uint32_t {
			IrradianceMap = 0u,
			PrefilterMap,
			BRDFMap,

			Count
		};

		ShaderPBR(prosper::IPrContext &context, const std::string &identifier);
		ShaderPBR(prosper::IPrContext &context, const std::string &identifier, const std::string &vsShader, const std::string &fsShader, const std::string &gsShader = "");

		prosper::IDescriptorSet &GetDefaultPbrDescriptorSet() const;

		//
		virtual void RecordBindScene(rendering::ShaderProcessor &shaderProcessor, const pragma::CSceneComponent &scene, const pragma::CRasterizationRendererComponent &renderer, prosper::IDescriptorSet &dsScene, prosper::IDescriptorSet &dsRenderer, prosper::IDescriptorSet &dsRenderSettings,
		  prosper::IDescriptorSet &dsShadows, const Vector4 &drawOrigin, ShaderGameWorld::SceneFlags &inOutSceneFlags) const override;

		prosper::IDescriptorSet *GetReflectionProbeDescriptorSet(const pragma::CSceneComponent &scene, float &outIblStrength, ShaderGameWorld::SceneFlags &inOutSceneFlags) const;
	  protected:
		using ShaderGameWorldLightingPass::RecordDraw;
		void RecordBindSceneDescriptorSets(rendering::ShaderProcessor &shaderProcessor, const pragma::CSceneComponent &scene, const pragma::CRasterizationRendererComponent &renderer, prosper::IDescriptorSet &dsScene, prosper::IDescriptorSet &dsRenderer,
		  prosper::IDescriptorSet &dsRenderSettings, prosper::IDescriptorSet &dsShadows, ShaderGameWorld::SceneFlags &inOutSceneFlags, float &outIblStrength) const;
		virtual void OnPipelinesInitialized() override;
		virtual void InitializeGfxPipeline(prosper::GraphicsPipelineCreateInfo &pipelineInfo, uint32_t pipelineIdx) override;
		virtual void InitializeMaterialData(const CMaterial &mat, const rendering::shader_material::ShaderMaterial &shaderMat, pragma::rendering::ShaderInputData &inOutMatData) override;
		virtual void UpdateRenderFlags(CModelSubMesh &mesh, SceneFlags &inOutFlags) override;
		virtual void InitializeGfxPipelineDescriptorSets() override;
		std::shared_ptr<prosper::IDescriptorSetGroup> InitializeMaterialDescriptorSet(CMaterial &mat, const prosper::DescriptorSetInfo &descSetInfo);

		SceneFlags m_extRenderFlags = SceneFlags::None;
		std::shared_ptr<prosper::IDescriptorSetGroup> m_defaultPbrDsg = nullptr;
	};

	class DLLCLIENT ShaderPBRBlend : public ShaderPBR {
	  public:
		static prosper::ShaderGraphics::VertexBinding VERTEX_BINDING_ALPHA;
		static prosper::ShaderGraphics::VertexAttribute VERTEX_ATTRIBUTE_ALPHA;

		enum class MaterialBinding : uint32_t { AlbedoMap2 = umath::to_integral(ShaderPBR::MaterialBinding::Count), AlbedoMap3 };

#pragma pack(push, 1)
		struct PushConstants {
			int32_t alphaCount;
		};
#pragma pack(pop)

		ShaderPBRBlend(prosper::IPrContext &context, const std::string &identifier);
		virtual bool GetRenderBufferTargets(CModelSubMesh &mesh, uint32_t pipelineIdx, std::vector<prosper::IBuffer *> &outBuffers, std::vector<prosper::DeviceSize> &outOffsets, std::optional<prosper::IndexBufferInfo> &outIndexBufferInfo) const override;
	  protected:
		virtual void InitializeGfxPipelinePushConstantRanges() override;
		virtual void InitializeGfxPipelineVertexAttributes() override;
		virtual bool OnRecordDrawMesh(rendering::ShaderProcessor &shaderProcessor, CModelSubMesh &mesh) const override;
	};
};

#endif
