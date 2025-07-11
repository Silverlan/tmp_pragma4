// SPDX-FileCopyrightText: (c) 2019 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_client.h"
#include "pragma/game/c_game.h"
#include "pragma/entities/c_listener.h"
#include "pragma/lua/classes/c_lentity.h"
#include "pragma/lua/classes/c_llistener.h"
#include "pragma/lua/classes/c_lshaderinfo.h"
#include "pragma/lua/classes/c_lshader.h"
#include "pragma/lua/classes/c_lmaterial.h"
#include "pragma/lua/classes/c_lpoint_rendertarget.h"
#include "pragma/lua/converters/shader_converter_t.hpp"
#include "pragma/rendering/shaders/c_shader_lua.hpp"
#include "pragma/rendering/render_processor.hpp"
#include "pragma/rendering/render_queue.hpp"
#include "pragma/lua/classes/lmaterial.h"
#include "pragma/lua/classes/lentity.h"
#include "pragma/entities/point/c_point_rendertarget.h"
#include "pragma/lua/classes/c_lshaderinfo.h"
#include "pragma/lua/classes/lshaderinfo.h"
#include "pragma/rendering/shader_input_data.hpp"
#include "pragma/rendering/shaders/c_shader_cubemap_to_equirectangular.hpp"
#include "pragma/rendering/shaders/image/c_shader_merge_images.hpp"
#include "pragma/rendering/shaders/image/c_shader_merge_2d_image_into_equirectangular.hpp"
#include "pragma/entities/components/renderers/c_renderer_component.hpp"
#include "pragma/entities/components/renderers/c_rasterization_renderer_component.hpp"
#include "pragma/entities/environment/lights/c_env_light.h"
#include "pragma/entities/environment/lights/c_env_light_spot.h"
#include "pragma/entities/environment/lights/c_env_light_point.h"
#include "pragma/entities/environment/lights/c_env_light_directional.h"
#include "pragma/entities/c_wheel.hpp"
#include "pragma/model/c_side.h"
#include "pragma/lua/classes/lmodel.h"
#include "pragma/lua/classes/c_lmodel.h"
#include "pragma/lua/classes/lmodelmesh.h"
#include "pragma/lua/classes/c_lmodelmesh.h"
#include "pragma/model/c_model.h"
#include "pragma/model/c_modelmesh.h"
#include "pragma/lua/classes/c_ltexture.h"
#include "luasystem.h"
#include "textureinfo.h"
#include "pragma/model/brush/c_brushmesh.h"
#include <pragma/game/damageinfo.h>
#include "pragma/lua/classes/c_lua_entity.h"
#include "pragma/entities/environment/effects/c_env_particle_system.h"
#include "pragma/debug/c_debugoverlay.h"
#include "pragma/lua/libraries/c_ldebugoverlay.h"
#include "pragma/lua/libraries/c_lua_vulkan.h"
#include "pragma/lua/libraries/c_lutil.h"
#include "pragma/lua/classes/c_lworldenvironment.hpp"
#include "pragma/asset/c_util_model.hpp"
#include "pragma/rendering/shaders/util/c_shader_compose_rma.hpp"
#include "pragma/rendering/shader_material/shader_material.hpp"
#include "pragma/rendering/shader_graph/manager.hpp"
#include "pragma/lua/libraries/ludm.hpp"
#include <shader/prosper_pipeline_create_info.hpp>
#include <wgui/fontmanager.h>
#include <wgui/shaders/wishader_textured.hpp>
#include "pragma/rendering/scene/util_draw_scene_info.hpp"
#include <pragma/lua/converters/pair_converter_t.hpp>
#include <pragma/lua/converters/game_type_converters_t.hpp>
#include <pragma/lua/converters/optional_converter_t.hpp>
#include <pragma/lua/converters/vector_converter_t.hpp>
#include <pragma/entities/components/liquid/base_liquid_component.hpp>
#include <shader/prosper_shader_flip_image.hpp>
#include <prosper_prepared_command_buffer.hpp>
#include <prosper_command_buffer.hpp>
#include <prosper_descriptor_set_group.hpp>
#include <prosper_render_pass.hpp>
#include <shader/prosper_shader_t.hpp>
#include <pragma/lua/lua_call.hpp>
#include <pragma/lua/policies/default_parameter_policy.hpp>
#include <pragma/lua/converters/string_view_converter_t.hpp>
#include <pragma/logging.hpp>
#include <luainterface.hpp>
#include <luabind/copy_policy.hpp>
#include <luabind/return_reference_to_policy.hpp>
#include <material_property_block_view.hpp>
#include <cmaterialmanager.h>
#include <cmaterial_manager2.hpp>
#include <wgui/wgui.h>

import pragma.shadergraph;

extern DLLCLIENT CEngine *c_engine;
extern DLLCLIENT ClientState *client;
extern DLLCLIENT CGame *c_game;

static spdlog::logger &LOGGER_SG = pragma::register_logger("shadergraph");

#pragma optimize("", off)
static void reload_textures(CMaterial &mat)
{
	std::unordered_map<std::string, std::string> textureMappings;
	for(auto &name : msys::MaterialPropertyBlockView {mat}) {
		auto type = mat.GetPropertyType(name);
		if(type != msys::PropertyType::Texture)
			continue;
		std::string tex;
		if(!mat.GetProperty(name, &tex))
			continue;
		textureMappings[std::string {name}] = tex;
	}

	for(auto &pair : textureMappings)
		mat.SetTexture(pair.first, pair.second);
}

static luabind::object shader_mat_value_to_lua_object(lua_State *l, const pragma::shadergraph::Value &val)
{
	return pragma::shadergraph::visit(val.GetType(), [l, &val](auto tag) {
		using T = typename decltype(tag)::type;
		T value;
		if(!val.Get(value))
			return Lua::nil;
		if constexpr(std::is_same_v<T, udm::Half>)
			return luabind::object {l, static_cast<float>(value)};
		else
			return luabind::object {l, value};
	});
}

static void register_shader_graph(lua_State *l, luabind::module_ &modShader)
{
	modShader[luabind::def(
	  "get_test_node_register", +[]() -> std::shared_ptr<pragma::shadergraph::NodeRegistry> {
		  auto reg = std::make_shared<pragma::shadergraph::NodeRegistry>();
		  reg->RegisterNode<pragma::shadergraph::MathNode>("math");
		  return reg;
	  })];

	auto defGraph = luabind::class_<pragma::shadergraph::Graph>("ShaderGraph");
	defGraph.scope[luabind::def("get_shader_file_path", &pragma::rendering::ShaderGraphManager::GetShaderFilePath)];
	defGraph.scope[luabind::def("get_file_path", &pragma::rendering::ShaderGraphManager::GetShaderGraphFilePath)];
	defGraph.scope[luabind::def(
	  "load", +[](const std::string &type, const std::string &name) -> std::pair<std::shared_ptr<pragma::shadergraph::Graph>, std::optional<std::string>> {
		  auto filePath = pragma::rendering::ShaderGraphManager::GetShaderGraphFilePath(type, name);
		  auto reg = c_engine->GetShaderGraphManager().GetNodeRegistry(type);
		  if(!reg) {
			  LOGGER_SG.error("Failed to load shader graph of type '{}' with name '{}' - no registry found!", type, name);
			  return {std::shared_ptr<pragma::shadergraph::Graph> {}, std::optional<std::string> {}};
		  }
		  auto graph = std::make_shared<pragma::shadergraph::Graph>(reg);
		  std::string err;
		  auto result = graph->Load(filePath, err);
		  if(!result)
			  return {std::shared_ptr<pragma::shadergraph::Graph> {}, std::optional<std::string> {err}};
		  return {graph, {}};
	  })];
	defGraph.scope[luabind::def(
	  "load", +[](udm::AssetData &assetData, const std::shared_ptr<pragma::shadergraph::NodeRegistry> &nodeReg) -> std::pair<std::shared_ptr<pragma::shadergraph::Graph>, std::optional<std::string>> {
		  auto graph = std::make_shared<pragma::shadergraph::Graph>(nodeReg);
		  std::string err;
		  auto data = assetData.GetData();
		  auto result = graph->Load(data, err);
		  if(!result)
			  return {std::shared_ptr<pragma::shadergraph::Graph> {}, std::optional<std::string> {err}};
		  return {graph, {}};
	  })];
	defGraph.def("__tostring", +[](const pragma::shadergraph::Graph &graph) -> std::string { return "ShaderGraph"; });
	defGraph.def(
	  "Save", +[](const pragma::shadergraph::Graph &graph, udm::AssetData &assetData) -> std::pair<bool, std::optional<std::string>> {
		  std::string err;
		  if(!graph.Save(assetData, err))
			  return {false, err};
		  return {true, std::optional<std::string> {}};
	  });
	defGraph.def(
	  "Save", +[](const pragma::shadergraph::Graph &graph, const std::string &type, const std::string &name) -> std::pair<bool, std::optional<std::string>> {
		  auto filePath = pragma::rendering::ShaderGraphManager::GetShaderGraphFilePath(type, name);
		  std::string err;
		  filemanager::create_path(ufile::get_path_from_filename(filePath));
		  if(!graph.Save(filePath, err))
			  return {false, err};
		  return {true, std::optional<std::string> {}};
	  });
	defGraph.def(
	  "Copy", +[](const pragma::shadergraph::Graph &graph) -> std::shared_ptr<pragma::shadergraph::Graph> {
		  auto cpy = std::make_shared<pragma::shadergraph::Graph>(graph.GetNodeRegistry());
		  cpy->Merge(graph);
		  return cpy;
	  });
	defGraph.def(
	  "AddNode", +[](pragma::shadergraph::Graph &graph, const std::string &type) -> std::shared_ptr<pragma::shadergraph::GraphNode> {
		  auto node = graph.AddNode(type);
		  if(!node) {
			  LOGGER_SG.error("Failed to add node of type '{}'!", type);
			  return nullptr;
		  }
		  return node;
	  });
	defGraph.def("RemoveNode", &pragma::shadergraph::Graph::RemoveNode);
	defGraph.def("GetNode", &pragma::shadergraph::Graph::GetNode);
	defGraph.def(
	  "GetNodes", +[](lua_State *l, const pragma::shadergraph::Graph &graph) -> luabind::object {
		  auto t = luabind::newtable(l);
		  uint32_t idx = 1;
		  for(auto &node : graph.GetNodes())
			  t[idx++] = node.get();
		  return t;
	  });
	defGraph.def("GenerateGlsl", &pragma::shadergraph::Graph::GenerateGlsl);
	defGraph.def("DebugPrint", &pragma::shadergraph::Graph::DebugPrint);

	auto defNode = luabind::class_<pragma::shadergraph::Node>("Node");
	defNode.def("GetType", &pragma::shadergraph::Node::GetType);
	defNode.def("GetCategory", &pragma::shadergraph::Node::GetCategory);
	defNode.def(
	  "GetInputs", +[](lua_State *l, const pragma::shadergraph::Node &node) -> luabind::object {
		  auto t = luabind::newtable(l);
		  uint32_t idx = 1;
		  for(auto &socket : node.GetInputs())
			  t[idx++] = &socket;
		  return t;
	  });
	defNode.def(
	  "GetOutputs", +[](lua_State *l, const pragma::shadergraph::Node &node) -> luabind::object {
		  auto t = luabind::newtable(l);
		  uint32_t idx = 1;
		  for(auto &socket : node.GetOutputs())
			  t[idx++] = &socket;
		  return t;
	  });
	modShader[defNode];

	auto defEnumSet = luabind::class_<pragma::shadergraph::EnumSet>("EnumSet");
	defEnumSet.def("IsEmpty", &pragma::shadergraph::EnumSet::empty);
	defEnumSet.def("Exists", static_cast<bool (pragma::shadergraph::EnumSet::*)(const std::string &) const>(&pragma::shadergraph::EnumSet::exists));
	defEnumSet.def("Exists", static_cast<bool (pragma::shadergraph::EnumSet::*)(int32_t) const>(&pragma::shadergraph::EnumSet::exists));
	defEnumSet.def("FindValue", &pragma::shadergraph::EnumSet::findValue);
	defEnumSet.def("FindName", &pragma::shadergraph::EnumSet::findName);
	defEnumSet.def(
	  "GetNameToValue", +[](lua_State *l, const pragma::shadergraph::EnumSet &enumSet) -> Lua::map<std::string, int32_t> {
		  auto t = luabind::newtable(l);
		  for(auto &pair : enumSet.getNameToValue())
			  t[pair.first] = pair.second;
		  return t;
	  });
	defEnumSet.def(
	  "GetValueToName", +[](lua_State *l, const pragma::shadergraph::EnumSet &enumSet) -> Lua::map<int32_t, std::string> {
		  auto t = luabind::newtable(l);
		  for(auto &pair : enumSet.getValueToName())
			  t[pair.first] = pair.second;
		  return t;
	  });

	auto defParameter = luabind::class_<pragma::shadergraph::Parameter>("Parameter");
	defParameter.def_readonly("name", &pragma::shadergraph::Parameter::name);
	defParameter.def_readonly("type", &pragma::shadergraph::Parameter::type);
	defParameter.def_readonly("min", &pragma::shadergraph::Parameter::min);
	defParameter.def_readonly("max", &pragma::shadergraph::Parameter::max);
	defParameter.property("defaultValue", +[](lua_State *l, const pragma::shadergraph::Parameter &param) -> luabind::object { return shader_mat_value_to_lua_object(l, param.defaultValue); });
	defParameter.def(
	  "__tostring", +[](const pragma::shadergraph::Parameter &param) -> std::string {
		  std::stringstream ss;
		  ss << "Parameter";
		  ss << "[" << param.name << "]";
		  ss << "[Type:" << magic_enum::enum_name(param.type) << "]";
		  return ss.str();
	  });
	defParameter.def(
	  "GetOptions", +[](lua_State *l, const pragma::shadergraph::Parameter &param) -> std::optional<std::unordered_map<std::string, luabind::object>> {
		  if(!param.enumSet)
			  return {};
		  std::unordered_map<std::string, luabind::object> options {};
		  auto &nameToVal = param.enumSet->getNameToValue();
		  options.reserve(nameToVal.size());
		  for(auto &[name, value] : nameToVal)
			  options[name] = luabind::object {l, value};
		  return options;
	  });
	modShader[defParameter];

	auto defSocket = luabind::class_<pragma::shadergraph::Socket>("Socket");
	defSocket.add_static_constant("FLAG_NONE", umath::to_integral(pragma::shadergraph::Socket::Flags::None));
	defSocket.add_static_constant("FLAG_LINKABLE", umath::to_integral(pragma::shadergraph::Socket::Flags::Linkable));
	defSocket.add_static_constant("TYPE_BOOLEAN", umath::to_integral(pragma::shadergraph::DataType::Boolean));
	defSocket.add_static_constant("TYPE_INT", umath::to_integral(pragma::shadergraph::DataType::Int));
	defSocket.add_static_constant("TYPE_UINT", umath::to_integral(pragma::shadergraph::DataType::UInt));
	defSocket.add_static_constant("TYPE_FLOAT", umath::to_integral(pragma::shadergraph::DataType::Float));
	defSocket.add_static_constant("TYPE_COLOR", umath::to_integral(pragma::shadergraph::DataType::Color));
	defSocket.add_static_constant("TYPE_VECTOR", umath::to_integral(pragma::shadergraph::DataType::Vector));
	defSocket.add_static_constant("TYPE_POINT", umath::to_integral(pragma::shadergraph::DataType::Point));
	defSocket.add_static_constant("TYPE_NORMAL", umath::to_integral(pragma::shadergraph::DataType::Normal));
	defSocket.add_static_constant("TYPE_POINT2", umath::to_integral(pragma::shadergraph::DataType::Point2));
	defSocket.add_static_constant("TYPE_STRING", umath::to_integral(pragma::shadergraph::DataType::String));
	defSocket.add_static_constant("TYPE_TRANSFORM", umath::to_integral(pragma::shadergraph::DataType::Transform));
	defSocket.add_static_constant("TYPE_ENUM", umath::to_integral(pragma::shadergraph::DataType::Enum));
	defSocket.add_static_constant("TYPE_INVALID", umath::to_integral(pragma::shadergraph::DataType::Invalid));
	defSocket.scope[luabind::def("udm_to_data_type", &pragma::shadergraph::to_data_type)];
	defSocket.scope[luabind::def("to_udm_type", &pragma::shadergraph::to_udm_type)];
	defSocket.scope[luabind::def("to_glsl_type", &pragma::shadergraph::to_glsl_type)];

	defSocket.def_readonly("name", &pragma::shadergraph::Socket::name);
	defSocket.def_readonly("type", &pragma::shadergraph::Socket::type);
	defSocket.def_readonly("min", &pragma::shadergraph::Socket::min);
	defSocket.def_readonly("max", &pragma::shadergraph::Socket::max);
	defSocket.def_readonly("flags", &pragma::shadergraph::Socket::flags);
	defSocket.property(
	  "defaultValue", +[](lua_State *l, const pragma::shadergraph::Socket &socket) -> luabind::object {
		  auto udmType = pragma::shadergraph::to_udm_type(socket.defaultValue.GetType());
		  if(udmType == udm::Type::Invalid)
			  return Lua::nil;
		  udm::Property prop {};
		  prop.type = udmType;
		  prop.value = socket.defaultValue.GetData();
		  udm::LinkedPropertyWrapper lp {prop};
		  auto val = Lua::udm::udm_to_value(l, lp);
		  prop.value = nullptr; // We don't want to free the data
		  return val;
	  });
	defSocket.property("enumSet", +[](lua_State *l, const pragma::shadergraph::Socket &socket) -> pragma::shadergraph::EnumSet * { return socket.enumSet.get(); });
	defSocket.def("IsLinkable", &pragma::shadergraph::Socket::IsLinkable);
	defSocket.scope[defEnumSet];
	modShader[defSocket];

	auto defNodeRegistry = luabind::class_<pragma::shadergraph::NodeRegistry>("NodeRegistry");
	defNodeRegistry.def("GetNode", &pragma::shadergraph::NodeRegistry::GetNode);
	defNodeRegistry.def(
	  "GetNodeTypes", +[](const pragma::shadergraph::NodeRegistry &reg) -> std::vector<std::string> {
		  std::vector<std::string> names;
		  reg.GetNodeTypes(names);
		  return names;
	  });
	modShader[defNodeRegistry];

	auto defGraphNode = luabind::class_<pragma::shadergraph::GraphNode>("GraphNode");
	defGraphNode.def("GetName", &pragma::shadergraph::GraphNode::GetName);
	defGraphNode.def("GetNode", +[](const pragma::shadergraph::GraphNode &graphNode) -> const pragma::shadergraph::Node * { return &graphNode.node; });
	defGraphNode.def("SetPos", &pragma::shadergraph::GraphNode::SetPos);
	defGraphNode.def("GetPos", &pragma::shadergraph::GraphNode::GetPos, luabind::copy_policy<0> {});
	defGraphNode.def("SetDisplayName", &pragma::shadergraph::GraphNode::SetDisplayName);
	defGraphNode.def("GetDisplayName", &pragma::shadergraph::GraphNode::GetDisplayName);
	defGraphNode.def("ClearInputValue", &pragma::shadergraph::GraphNode::ClearInputValue);
	defGraphNode.def(
	  "SetInputValue", +[](pragma::shadergraph::GraphNode &graphNode, const std::string_view &inputName, luabind::object value) -> bool {
		  auto type = Lua::udm::determine_udm_type(value);
		  return ::udm::visit(type, [&graphNode, &inputName, &value](auto tag) {
			  using T = typename decltype(tag)::type;
			  if constexpr(pragma::shadergraph::is_data_type<T>()) {
				  auto val = luabind::object_cast<T>(value);
				  return graphNode.SetInputValue(inputName, val);
			  }
			  return false;
		  });
	  });
	defGraphNode.def(
	  "GetInputValue", +[](lua_State *l, const pragma::shadergraph::GraphNode &graphNode, const std::string_view &inputName) -> Lua::udm_type {
		  auto *input = graphNode.FindInput(inputName);
		  if(!input)
			  return Lua::nil;
		  auto type = pragma::shadergraph::to_udm_type(input->GetSocket().type);
		  if(type == udm::Type::Invalid)
			  return Lua::nil;
		  return ::udm::visit(type, [l, &graphNode, &inputName](auto tag) {
			  using T = typename decltype(tag)::type;
			  if constexpr(pragma::shadergraph::is_data_type<T>()) {
				  T val;
				  auto res = graphNode.GetInputValue(inputName, val);
				  if(!res)
					  return Lua::nil;
				  return luabind::object {l, val};
			  }
			  return Lua::nil;
		  });
	  });
	defGraphNode.def("CanLink", static_cast<bool (pragma::shadergraph::GraphNode::*)(const std::string_view &, pragma::shadergraph::GraphNode &, const std::string_view &) const>(&pragma::shadergraph::GraphNode::CanLink));
	defGraphNode.def(
	  "Link", +[](pragma::shadergraph::GraphNode &graphNode, const std::string_view &outputName, pragma::shadergraph::GraphNode &linkTarget, const std::string_view &inputName) -> std::pair<bool, std::optional<std::string>> {
		  std::string err;
		  auto res = graphNode.Link(outputName, linkTarget, inputName, &err);
		  if(!res)
			  LOGGER_SG.debug("Failed to link output '{}' of node '{}' ({}) to input '{}' of node '{}' ({}): {}", outputName, graphNode.GetName(), graphNode->GetType(), inputName, linkTarget.GetName(), linkTarget->GetType(), err);
		  return {res, err.empty() ? std::optional<std::string> {} : std::optional<std::string> {err}};
	  });
	defGraphNode.def("Disconnect", static_cast<bool (pragma::shadergraph::GraphNode ::*)(const std::string_view &)>(&pragma::shadergraph::GraphNode::Disconnect));
	defGraphNode.def("IsOutputLinked", &pragma::shadergraph::GraphNode::IsOutputLinked);
	defGraphNode.def("FindOutputIndex", &pragma::shadergraph::GraphNode::FindOutputIndex);
	defGraphNode.def("FindInputIndex", &pragma::shadergraph::GraphNode::FindInputIndex);
	defGraphNode.def(
	  "GetInputs", +[](lua_State *l, const pragma::shadergraph::GraphNode &graphNode) -> luabind::object {
		  auto t = luabind::newtable(l);
		  uint32_t idx = 1;
		  for(auto &input : graphNode.inputs)
			  t[idx++] = &input;
		  return t;
	  });
	defGraphNode.def(
	  "GetOutputs", +[](lua_State *l, const pragma::shadergraph::GraphNode &graphNode) -> luabind::object {
		  auto t = luabind::newtable(l);
		  uint32_t idx = 1;
		  for(auto &output : graphNode.outputs)
			  t[idx++] = &output;
		  return t;
	  });
	modShader[defGraphNode];

	auto defInputSocket = luabind::class_<pragma::shadergraph::InputSocket>("InputSocket");
	defInputSocket.def("GetSocket", +[](const pragma::shadergraph::InputSocket &inputSocket) -> const pragma::shadergraph ::Socket * { return &inputSocket.GetSocket(); });
	defInputSocket.def("GetNode", +[](const pragma::shadergraph::InputSocket &inputSocket) -> pragma::shadergraph ::GraphNode * { return inputSocket.parent; });
	defInputSocket.def("GetLinkedOutput", +[](const pragma::shadergraph::InputSocket &inputSocket) -> pragma::shadergraph ::OutputSocket * { return inputSocket.link; });
	defInputSocket.def("ClearValue", &pragma::shadergraph::InputSocket::ClearValue);
	defInputSocket.def("HasValue", &pragma::shadergraph::InputSocket::HasValue);
	defInputSocket.def_readonly("socketIndex", &pragma::shadergraph::InputSocket::inputIndex);
	modShader[defInputSocket];

	auto defOutputSocket = luabind::class_<pragma::shadergraph::OutputSocket>("OutputSocket");
	defOutputSocket.def("GetSocket", +[](const pragma::shadergraph::OutputSocket &outputSocket) -> const pragma::shadergraph ::Socket * { return &outputSocket.GetSocket(); });
	defOutputSocket.def("GetNode", +[](const pragma::shadergraph::OutputSocket &outputSocket) -> pragma::shadergraph ::GraphNode * { return outputSocket.parent; });
	defOutputSocket.def("GetLinkedInputs", +[](const pragma::shadergraph::OutputSocket &outputSocket) -> std::vector<pragma::shadergraph::InputSocket *> { return outputSocket.links; });
	defOutputSocket.def_readonly("socketIndex", &pragma::shadergraph::OutputSocket::outputIndex);
	modShader[defOutputSocket];

	modShader[defGraph];

	auto oClass = luabind::globals(l);
	luabind::object oGraph = oClass["shader"]["ShaderGraph"];
	oGraph["EXTENSION_BINARY"] = pragma::shadergraph::Graph::EXTENSION_BINARY;
	oGraph["EXTENSION_ASCII"] = pragma::shadergraph::Graph::EXTENSION_ASCII;
	oGraph["PSG_IDENTIFIER"] = pragma::shadergraph::Graph::PSG_IDENTIFIER;
	oGraph["PSG_VERSION"] = pragma::shadergraph::Graph::PSG_VERSION;
	oGraph["ROOT_PATH"] = pragma::rendering::ShaderGraphManager::ROOT_GRAPH_PATH;

	oGraph["CATEGORY_INPUT_PARAMETER"] = pragma::shadergraph::CATEGORY_INPUT_PARAMETER;
	oGraph["CATEGORY_INPUT_SYSTEM"] = pragma::shadergraph::CATEGORY_INPUT_SYSTEM;
	oGraph["CATEGORY_MATH"] = pragma::shadergraph::CATEGORY_MATH;
	oGraph["CATEGORY_VECTOR_MATH"] = pragma::shadergraph::CATEGORY_VECTOR_MATH;
	oGraph["CATEGORY_COLOR"] = pragma::shadergraph::CATEGORY_COLOR;
	oGraph["CATEGORY_TEXTURE"] = pragma::shadergraph::CATEGORY_TEXTURE;
	oGraph["CATEGORY_SHADER"] = pragma::shadergraph::CATEGORY_SHADER;
	oGraph["CATEGORY_SCENE"] = pragma::shadergraph::CATEGORY_SCENE;
	oGraph["CATEGORY_ENVIRONMENT"] = pragma::shadergraph::CATEGORY_ENVIRONMENT;
	oGraph["CATEGORY_OUTPUT"] = pragma::shadergraph::CATEGORY_OUTPUT;
	oGraph["CATEGORY_UTILITY"] = pragma::shadergraph::CATEGORY_UTILITY;
}

void ClientState::RegisterSharedLuaClasses(Lua::Interface &lua, bool bGUI)
{
	auto &modEngine = lua.RegisterLibrary("engine");
	auto defFontInfo = luabind::class_<FontInfo>("FontInfo");
	modEngine[defFontInfo];

	auto &modUtil = lua.RegisterLibrary("util");
	auto defTexture = luabind::class_<Texture>("Texture");
	defTexture.def(
	  "__tostring", +[](const Texture &tex) -> std::string {
		  std::stringstream ss;
		  ss << "Texture";
		  ss << "[" << tex.GetName() << "]";
		  ss << "[" << tex.GetWidth() << "x" << tex.GetHeight() << "]";
		  ss << "[" << magic_enum::enum_name(tex.GetFlags()) << "]";
		  return ss.str();
	  });
	defTexture.def("GetName", &Texture::GetName, luabind::copy_policy<0> {});
	defTexture.def("GetWidth", &Texture::GetWidth);
	defTexture.def("GetHeight", &Texture::GetHeight);
	defTexture.def("GetVkTexture", &Lua::Texture::GetVkTexture);
	modUtil[defTexture];

	auto defTexInfo = luabind::class_<TextureInfo>("TextureInfo");
	defTexInfo.def("GetTexture", &Lua::TextureInfo::GetTexture);
	defTexInfo.def("GetSize", &Lua::TextureInfo::GetSize);
	defTexInfo.def("GetWidth", &Lua::TextureInfo::GetWidth);
	defTexInfo.def("GetHeight", &Lua::TextureInfo::GetHeight);
	defTexInfo.def("GetName", static_cast<std::string (*)(lua_State *, TextureInfo &)>([](lua_State *l, TextureInfo &textureInfo) { return textureInfo.name; }));
	modUtil[defTexInfo];

	auto &modGame = lua.RegisterLibrary("game");
	auto materialClassDef = luabind::class_<Material>("Material");

	auto spriteSheetDef = luabind::class_<SpriteSheetAnimation>("SpriteSheetAnimation");

	auto sequenceDef = luabind::class_<SpriteSheetAnimation::Sequence>("Sequence");
	sequenceDef.def("GetDuration", &SpriteSheetAnimation::Sequence::GetDuration);
	sequenceDef.def("GetFrameOffset", &SpriteSheetAnimation::Sequence::GetFrameOffset);

	auto frameDef = luabind::class_<SpriteSheetAnimation::Sequence::Frame>("Frame");
	frameDef.def("GetUVBounds", static_cast<std::pair<Vector2, Vector2> (*)(lua_State *, SpriteSheetAnimation::Sequence::Frame &)>([](lua_State *l, SpriteSheetAnimation::Sequence::Frame &frame) { return std::pair<Vector2, Vector2> {frame.uvStart, frame.uvEnd}; }));
	frameDef.def("GetDuration", static_cast<float (*)(lua_State *, SpriteSheetAnimation::Sequence::Frame &)>([](lua_State *l, SpriteSheetAnimation::Sequence::Frame &frame) -> float { return frame.duration; }));
	sequenceDef.scope[frameDef];

	sequenceDef.def("GetFrameCount", static_cast<uint32_t (*)(lua_State *, SpriteSheetAnimation::Sequence &)>([](lua_State *l, SpriteSheetAnimation::Sequence &sequence) -> uint32_t { return sequence.frames.size(); }));
	sequenceDef.def("GetFrame", static_cast<SpriteSheetAnimation::Sequence::Frame *(*)(lua_State *, SpriteSheetAnimation::Sequence &, uint32_t)>([](lua_State *l, SpriteSheetAnimation::Sequence &sequence, uint32_t frameIdx) -> SpriteSheetAnimation::Sequence::Frame * {
		if(frameIdx >= sequence.frames.size())
			return nullptr;
		auto &frame = sequence.frames.at(frameIdx);
		return &frame;
	}));
	sequenceDef.def("GetFrames", static_cast<luabind::tableT<SpriteSheetAnimation::Sequence::Frame> (*)(lua_State *, SpriteSheetAnimation::Sequence &)>([](lua_State *l, SpriteSheetAnimation::Sequence &sequence) -> luabind::tableT<SpriteSheetAnimation::Sequence::Frame> {
		auto &frames = sequence.frames;
		auto t = luabind::newtable(l);
		uint32_t frameIndex = 1;
		for(auto &frame : frames)
			t[frameIndex++] = &frame;
		return t;
	}));
	sequenceDef.def("IsLooping", static_cast<bool (*)(lua_State *, SpriteSheetAnimation::Sequence &)>([](lua_State *l, SpriteSheetAnimation::Sequence &sequence) { return sequence.loop; }));
	sequenceDef.def("GetInterpolatedFrameData",
	  static_cast<luabind::optional<luabind::mult<uint32_t, uint32_t, float>> (*)(lua_State *, SpriteSheetAnimation::Sequence &, float)>([](lua_State *l, SpriteSheetAnimation::Sequence &sequence, float ptTime) -> luabind::optional<luabind::mult<uint32_t, uint32_t, float>> {
		  uint32_t frameIndex0, frameIndex1;
		  float interpFactor;
		  if(sequence.GetInterpolatedFrameData(ptTime, frameIndex0, frameIndex1, interpFactor) == false)
			  return Lua::nil;
		  return luabind::mult<uint32_t, uint32_t, float> {l, frameIndex0, frameIndex1, interpFactor};
	  }));

	spriteSheetDef.scope[sequenceDef];

	spriteSheetDef.def("GetSequenceCount", static_cast<uint32_t (*)(lua_State *, SpriteSheetAnimation &)>([](lua_State *l, SpriteSheetAnimation &spriteSheetAnim) -> uint32_t { return spriteSheetAnim.sequences.size(); }));
	spriteSheetDef.def("GetSequence", static_cast<SpriteSheetAnimation::Sequence *(*)(lua_State *, SpriteSheetAnimation &, uint32_t)>([](lua_State *l, SpriteSheetAnimation &spriteSheetAnim, uint32_t seqIdx) -> SpriteSheetAnimation::Sequence * {
		if(seqIdx >= spriteSheetAnim.sequences.size())
			return nullptr;
		auto &seq = spriteSheetAnim.sequences.at(seqIdx);
		return &seq;
	}));
	spriteSheetDef.def("GetSequences", static_cast<luabind::tableT<SpriteSheetAnimation::Sequence> (*)(lua_State *, SpriteSheetAnimation &)>([](lua_State *l, SpriteSheetAnimation &spriteSheetAnim) -> luabind::tableT<SpriteSheetAnimation::Sequence> {
		auto &sequences = spriteSheetAnim.sequences;
		auto t = luabind::newtable(l);
		uint32_t seqIdx = 1;
		for(auto &seq : sequences)
			t[seqIdx++] = &seq;
		return t;
	}));
	materialClassDef.scope[spriteSheetDef];

	Lua::Material::register_class(materialClassDef);
	materialClassDef.def("SetTexture", static_cast<void (*)(lua_State *, Material *, const std::string &, const std::string &)>(&Lua::Material::Client::SetTexture));
	materialClassDef.def("SetTexture", static_cast<void (*)(lua_State *, Material *, const std::string &, Texture &)>(&Lua::Material::Client::SetTexture));
	materialClassDef.def("SetTexture", static_cast<void (*)(lua_State *, Material *, const std::string &, Lua::Vulkan::Texture &)>(&Lua::Material::Client::SetTexture));
	materialClassDef.def("SetTexture", static_cast<void (*)(lua_State *, Material *, const std::string &, Lua::Vulkan::Texture &, const std::string &)>(&Lua::Material::Client::SetTexture));
	materialClassDef.def("GetTextureInfo", &Lua::Material::Client::GetTexture);
	materialClassDef.def("ReloadTextures", &reload_textures);
	materialClassDef.def("InitializeShaderDescriptorSet", static_cast<void (*)(lua_State *, ::Material *, bool)>(&Lua::Material::Client::InitializeShaderData));
	materialClassDef.def("InitializeShaderDescriptorSet", static_cast<void (*)(lua_State *, ::Material *)>(&Lua::Material::Client::InitializeShaderData));
	materialClassDef.def("ClearSpriteSheetAnimation", static_cast<void (*)(lua_State *, ::Material &)>([](lua_State *l, ::Material &mat) { static_cast<CMaterial &>(mat).ClearSpriteSheetAnimation(); }));
	materialClassDef.def("GetSpriteSheetAnimation", static_cast<void (*)(lua_State *, ::Material &)>([](lua_State *l, ::Material &mat) {
		auto *spriteSheetAnim = static_cast<CMaterial &>(mat).GetSpriteSheetAnimation();
		if(spriteSheetAnim == nullptr)
			return;
		Lua::Push<SpriteSheetAnimation *>(l, spriteSheetAnim);
	}));
	materialClassDef.def("SetShader", static_cast<void (*)(lua_State *, ::Material &, const std::string &)>([](lua_State *l, ::Material &mat, const std::string &shader) {
		auto db = mat.GetPropertyDataBlock();
		if(db == nullptr)
			return;
		auto shaderInfo = c_engine->GetShaderManager().PreRegisterShader(shader);
		static_cast<CMaterial &>(mat).ClearDescriptorSets();
		mat.Initialize(shaderInfo, db);
		mat.SetUserData2(nullptr);
		mat.SetLoaded(true);
		mat.UpdateTextures(true);
		c_game->ReloadMaterialShader(static_cast<CMaterial *>(&mat));
	}));
	materialClassDef.def(
	  "GetPrimaryShader", +[](lua_State *l, ::Material &mat) -> luabind::object {
		  auto *shader = static_cast<CMaterial &>(mat).GetPrimaryShader();
		  if(!shader)
			  return Lua::nil;
		  Lua::shader::push_shader(l, *shader);
		  auto o = luabind::object {luabind::from_stack(l, -1)};
		  Lua::Pop(l, 1);
		  return o;
	  });
	modGame[materialClassDef];

	// prosper TODO
	auto &modShader = lua.RegisterLibrary("shader",
	  {{"register",
	     [](lua_State *l) {
		     auto *className = Lua::CheckString(l, 1);
		     luaL_checkuserdata(l, 2);
		     auto o = luabind::object(luabind::from_stack(l, 2));
		     if(o) {
			     auto &manager = c_game->GetLuaShaderManager();
			     manager.RegisterShader(className, o);
		     }
		     return 0;
	     }},
	    {"get",
	      [](lua_State *l) {
		      auto *className = Lua::CheckString(l, 1);
		      auto whShader = c_engine->GetShader(className);
		      if(whShader.expired())
			      return 0;
		      Lua::shader::push_shader(l, *whShader.get());
		      return 1;
	      }},
	    {"get_all",
	      [](lua_State *l) {
		      auto t = luabind::newtable(l);
		      uint32_t idx = 1;
		      for(auto &shader : c_engine->GetShaderManager().GetShaders()) {
			      Lua::shader::push_shader(l, *shader);
			      luabind::object o {luabind::from_stack(l, -1)};
			      Lua::Pop(l, 1);
			      t[idx++] = o;
		      }
		      t.push(l);
		      return 1;
	      }},
	    {"cubemap_to_equirectangular_texture", [](lua_State *l) {
		     auto *shader = static_cast<pragma::ShaderCubemapToEquirectangular *>(c_engine->GetShader("cubemap_to_equirectangular").get());
		     if(!shader)
			     return 0;
		     auto cubemap = Lua::Check<std::shared_ptr<prosper::Texture>>(l, 1);
		     uint32_t width = 1'600;
		     uint32_t height = 800;
		     if(Lua::IsSet(l, 2))
			     width = Lua::CheckInt(l, 2);
		     if(Lua::IsSet(l, 3))
			     height = Lua::CheckInt(l, 3);
		     umath::Degree range = 360.f;
		     if(Lua::IsSet(l, 4))
			     range = Lua::CheckNumber(l, 4);
		     auto layout = prosper::ImageLayout::ShaderReadOnlyOptimal;
		     if(Lua::IsSet(l, 5))
			     layout = static_cast<prosper::ImageLayout>(Lua::CheckInt(l, 5));
		     auto tex = shader->CubemapToEquirectangularTexture(*cubemap, width, height, range, layout);
		     if(!tex)
			     return 0;
		     Lua::Push(l, tex);
		     return 1;
	     }}});

	modShader[luabind::def(
	  "register_graph", +[](const std::string &type, const std::string &identifier) -> std::shared_ptr<pragma::shadergraph::Graph> {
		  auto &manager = c_engine->GetShaderGraphManager();
		  return manager.RegisterGraph(type, identifier);
	  })];
	modShader[luabind::def(
	  "create_graph", +[](const std::string &type) -> std::shared_ptr<pragma::shadergraph::Graph> {
		  auto &manager = c_engine->GetShaderGraphManager();
		  return manager.CreateGraph(type);
	  })];
	modShader[luabind::def(
	  "get_graph", +[](const std::string &identifier) -> std::shared_ptr<pragma::shadergraph::Graph> {
		  auto &manager = c_engine->GetShaderGraphManager();
		  auto graphData = manager.GetGraph(identifier);
		  if(!graphData)
			  return nullptr;
		  return graphData->GetGraph();
	  })];
	modShader[luabind::def(
	  "reload_graph_shader", +[](const std::string &identifier) {
		  auto &manager = c_engine->GetShaderGraphManager();
		  manager.ReloadShader(identifier);
	  })];
	modShader[luabind::def(
	  "get_graph_node_registry", +[](const std::string &type) -> std::shared_ptr<pragma::shadergraph::NodeRegistry> {
		  auto &manager = c_engine->GetShaderGraphManager();
		  return manager.GetNodeRegistry(type);
	  })];
	modShader[luabind::def(
	  "load_shader_graph", +[](const std::string &identifier) -> std::pair<std::shared_ptr<pragma::shadergraph::Graph>, std::optional<std::string>> {
		  auto &manager = c_engine->GetShaderGraphManager();
		  std::string err;
		  auto graph = manager.LoadShader(identifier, err, true);
		  if(!graph)
			  return {nullptr, std::optional<std::string> {err}};
		  return {graph, std::optional<std::string> {}};
	  })];
	modShader[luabind::def(
	  "sync_shader_graph", +[](const std::string &type, const std::string &identifier, const pragma::shadergraph::Graph &graph) {
		  auto &manager = c_engine->GetShaderGraphManager();
		  manager.SyncGraph(type, identifier, graph);
	  })];

	// These have to match shaders/modules/fs_tonemapping.gls!
	enum class ToneMapping : uint8_t {
		None = 0,
		GammaCorrection,
		Reinhard,
		HejilRichard,
		Uncharted,
		Aces,
		GranTurismo,
		Hdr,

		Count
	};
	Lua::RegisterLibraryEnums(lua.GetState(), "shader",
	  {{"TONE_MAPPING_NONE", umath::to_integral(ToneMapping::None)}, {"TONE_MAPPING_GAMMA_CORRECTION", umath::to_integral(ToneMapping::GammaCorrection)}, {"TONE_MAPPING_REINHARD", umath::to_integral(ToneMapping::Reinhard)},
	    {"TONE_MAPPING_HEJIL_RICHARD", umath::to_integral(ToneMapping::HejilRichard)}, {"TONE_MAPPING_UNCHARTED", umath::to_integral(ToneMapping::Uncharted)}, {"TONE_MAPPING_ACES", umath::to_integral(ToneMapping::Aces)},
	    {"TONE_MAPPING_GRAN_TURISMO", umath::to_integral(ToneMapping::GranTurismo)}, {"TONE_MAPPING_HDR", umath::to_integral(ToneMapping::Hdr)},

	    {"TONE_MAPPING_COUNT", umath::to_integral(ToneMapping::Count)}});

	auto defShaderInfo = luabind::class_<util::ShaderInfo>("Info");
	//defShaderInfo.def("GetID",&Lua_ShaderInfo_GetID);
	defShaderInfo.def("GetName", &::util::ShaderInfo::GetIdentifier);
	modShader[defShaderInfo];

	auto defBindState = luabind::class_<prosper::ShaderBindState>("BindState");
	defBindState.def(luabind::constructor<prosper::ICommandBuffer &>());
	modShader[defBindState];

	auto defMat = luabind::class_<pragma::rendering::shader_material::ShaderMaterial>("ShaderMaterial");
	defMat.def("__tostring", +[](const pragma::rendering::shader_material::ShaderMaterial &shaderMat) -> std::string { return "ShaderMaterial"; });
	defMat.def("FindProperty", +[](const pragma::rendering::shader_material::ShaderMaterial &shaderMat, const std::string &name) -> const pragma::rendering::Property * { return shaderMat.FindProperty(name.c_str()); });
	defMat.def("FindTexture", +[](const pragma::rendering::shader_material::ShaderMaterial &shaderMat, const std::string &name) -> const pragma::rendering::shader_material::Texture * { return shaderMat.FindTexture(name.c_str()); });
	defMat.def(
	  "GetProperties", +[](const pragma::rendering::shader_material::ShaderMaterial &shaderMat) -> std::vector<const pragma::rendering::Property *> {
		  std::vector<const pragma::rendering::Property *> props;
		  props.reserve(shaderMat.properties.size());
		  for(auto &prop : shaderMat.properties)
			  props.push_back(&prop);
		  return props;
	  });
	defMat.def(
	  "GetTextures", +[](const pragma::rendering::shader_material::ShaderMaterial &shaderMat) -> std::vector<const pragma::rendering::shader_material::Texture *> {
		  std::vector<const pragma::rendering::shader_material::Texture *> textures;
		  textures.reserve(shaderMat.textures.size());
		  for(auto &tex : shaderMat.textures)
			  textures.push_back(&tex);
		  return textures;
	  });

	auto defProp = luabind::class_<pragma::rendering::Property>("Property");
	defProp.add_static_constant("FLAG_NONE", umath::to_integral(pragma::rendering::Property::Flags::None));
	defProp.add_static_constant("FLAG_HIDE_IN_EDITOR_BIT", umath::to_integral(pragma::rendering::Property::Flags::HideInEditor));
	defProp.def(
	  "__tostring", +[](const pragma::rendering::Property &prop) -> std::string {
		  std::stringstream ss;
		  ss << "Property";
		  ss << "[" << prop.parameter.name << "]";
		  ss << "[Type:" << magic_enum::enum_name(prop.parameter.type) << "]";
		  return ss.str();
	  });
	defProp.def_readonly("parameter", &pragma::rendering::Property::parameter);
	defProp.def_readonly("propertyFlags", &pragma::rendering::Property::propertyFlags);
	defProp.property("specializationType", +[](const pragma::rendering::Property &prop) -> std::optional<std::string> { return prop.specializationType ? *prop.specializationType : std::optional<std::string> {}; });
	defProp.def_readonly("offset", &pragma::rendering::Property::offset);
	defProp.def("GetSize", &pragma::rendering::Property::GetSize);
	defProp.def(
	  "GetFlags", +[](const pragma::rendering::Property &prop) -> std::optional<std::unordered_map<std::string, uint32_t>> {
		  if(!prop.flags)
			  return {};
		  return *prop.flags;
	  });
	defMat.scope[defProp];

	auto defTex = luabind::class_<pragma::rendering::shader_material::Texture>("Texture");
	defTex.def(
	  "__tostring", +[](const pragma::rendering::shader_material::Texture &tex) -> std::string {
		  std::stringstream ss;
		  ss << "Texture";
		  ss << "[" << tex.name << "]";
		  return ss.str();
	  });
	defTex.property("name", +[](const pragma::rendering::shader_material::Texture &tex) -> std::string { return tex.name; });
	defTex.property("specializationType", +[](const pragma::rendering::shader_material::Texture &tex) -> std::optional<std::string> { return tex.specializationType ? *tex.specializationType : std::optional<std::string> {}; });
	defTex.def_readonly("defaultTexturePath", &pragma::rendering::shader_material::Texture::defaultTexturePath);
	defTex.def_readonly("cubemap", &pragma::rendering::shader_material::Texture::cubemap);
	defTex.def_readonly("colorMap", &pragma::rendering::shader_material::Texture::colorMap);
	defTex.def_readonly("required", &pragma::rendering::shader_material::Texture::required);
	defMat.scope[defTex];

	modShader[defMat];

	auto defMatData = luabind::class_<pragma::rendering::ShaderInputData>("ShaderInputData");
	modShader[defMatData];

	register_shader_graph(lua.GetState(), modShader);

	auto defShader = luabind::class_<prosper::Shader>("Shader");
	defShader.def(
	  "GetWrapper", +[](lua_State *l, prosper::Shader &shader) -> luabind::object {
		  auto *lshader = dynamic_cast<pragma::LShaderBase *>(&shader);
		  if(!lshader)
			  return {};
		  return lshader->GetWrapperObject();
	  });
	defShader.def(
	  "RecordBindDescriptorSet",
	  +[](lua_State *l, prosper::Shader &shader, prosper::ShaderBindState &bindState, Lua::Vulkan::DescriptorSet &ds, uint32_t firstSet, luabind::object dynamicOffsets) { Lua::Shader::RecordBindDescriptorSet(l, shader, bindState, ds, firstSet, dynamicOffsets, 5); });
	defShader.def("RecordBindDescriptorSet", +[](lua_State *l, prosper::Shader &shader, prosper::ShaderBindState &bindState, Lua::Vulkan::DescriptorSet &ds, uint32_t firstSet) { Lua::Shader::RecordBindDescriptorSet(l, shader, bindState, ds, firstSet, {}); });
	defShader.def("RecordBindDescriptorSet", +[](lua_State *l, prosper::Shader &shader, prosper::ShaderBindState &bindState, Lua::Vulkan::DescriptorSet &ds) { Lua::Shader::RecordBindDescriptorSet(l, shader, bindState, ds, 0u, {}); });
	defShader.def("RecordBindDescriptorSets", &Lua::Shader::RecordBindDescriptorSets);
	defShader.def("RecordBindDescriptorSets", +[](lua_State *l, prosper::Shader &shader, prosper::ShaderBindState &bindState, luabind::object descSets, uint32_t firstSet) { Lua::Shader::RecordBindDescriptorSets(l, shader, bindState, descSets, firstSet, {}); });
	defShader.def("RecordBindDescriptorSets", +[](lua_State *l, prosper::Shader &shader, prosper::ShaderBindState &bindState, luabind::object descSets) { Lua::Shader::RecordBindDescriptorSets(l, shader, bindState, descSets, 0u, {}); });
	defShader.def(
	  "RecordBindDescriptorSet",
	  +[](lua_State *l, prosper::Shader &shader, prosper::util::PreparedCommandBuffer &pcb, Lua::Vulkan::DescriptorSet &ds, uint32_t firstSet, luabind::object dynamicOffsets) { Lua::Shader::RecordBindDescriptorSet(l, shader, pcb, ds, firstSet, dynamicOffsets, 5); });
	defShader.def("RecordPushConstants", static_cast<void (*)(lua_State *, prosper::Shader &, const LuaShaderRecordTarget &, ::DataStream &, uint32_t)>(&Lua::Shader::RecordPushConstants));
	defShader.def("RecordPushConstants", static_cast<void (*)(lua_State *, prosper::Shader &, prosper::util::PreparedCommandBuffer &, udm::Type, const Lua::Vulkan::PreparedCommandLuaArg &, uint32_t)>(&Lua::Shader::RecordPushConstants));
	defShader.def("RecordPushConstants", static_cast<void (*)(lua_State *, prosper::Shader &, const LuaShaderRecordTarget &, ::DataStream &, uint32_t)>(&Lua::Shader::RecordPushConstants), luabind::default_parameter_policy<5, static_cast<uint32_t>(0)> {});
	defShader.def("RecordPushConstants", static_cast<void (*)(lua_State *, prosper::Shader &, prosper::util::PreparedCommandBuffer &, udm::Type, const Lua::Vulkan::PreparedCommandLuaArg &, uint32_t)>(&Lua::Shader::RecordPushConstants),
	  luabind::default_parameter_policy<6, static_cast<uint32_t>(0)> {});
	defShader.def("GetEntrypointName", &Lua::Shader::GetEntrypointName);
	defShader.def("GetEntrypointName", static_cast<void (*)(lua_State *, prosper::Shader &, uint32_t)>([](lua_State *l, prosper::Shader &shader, uint32_t shaderStage) { Lua::Shader::GetEntrypointName(l, shader, shaderStage, 0u); }));
	defShader.def("CreateDescriptorSet", &Lua::Shader::CreateDescriptorSetGroup);
	defShader.def("GetPipelineInfo", &Lua::Shader::GetPipelineInfo);
	defShader.def("GetPipelineInfo", static_cast<void (*)(lua_State *, prosper::Shader &, uint32_t, uint32_t)>([](lua_State *l, prosper::Shader &shader, uint32_t shaderStage, uint32_t pipelineIdx) { Lua::Shader::GetPipelineInfo(l, shader, shaderStage, 0u); }));
	defShader.def("GetGlslSourceCode", &Lua::Shader::GetGlslSourceCode);
	defShader.def("GetGlslSourceCode", static_cast<void (*)(lua_State *, prosper::Shader &, uint32_t, uint32_t)>([](lua_State *l, prosper::Shader &shader, uint32_t shaderStage, uint32_t pipelineIdx) { Lua::Shader::GetGlslSourceCode(l, shader, shaderStage, 0u); }));

	defShader.def("IsGraphicsShader", &Lua::Shader::IsGraphicsShader);
	defShader.def("IsComputeShader", &Lua::Shader::IsComputeShader);
	defShader.def("GetPipelineBindPoint", &Lua::Shader::GetPipelineBindPoint);
	defShader.def("IsValid", &Lua::Shader::IsValid);
	defShader.def("GetIdentifier", &Lua::Shader::GetIdentifier);
	defShader.def("GetSourceFilePath", &Lua::Shader::GetSourceFilePath);
	defShader.def("GetSourceFilePaths", &Lua::Shader::GetSourceFilePaths);
	defShader.def("FindDescriptorSetIndex", &prosper::Shader::FindDescriptorSetIndex);
	modShader[defShader];

	auto defShaderGraphics = luabind::class_<prosper::ShaderGraphics, prosper::Shader>("Graphics");
	defShaderGraphics.def("RecordBindVertexBuffer", &Lua::Shader::Graphics::RecordBindVertexBuffer);
	defShaderGraphics.def(
	  "RecordBindVertexBuffer", +[](lua_State *l, prosper::ShaderGraphics &shader, prosper::ShaderBindState &bindState, Lua::Vulkan::Buffer &buffer, uint32_t startBinding) { Lua::Shader::Graphics::RecordBindVertexBuffer(l, shader, bindState, buffer, startBinding, 0u); });
	defShaderGraphics.def("RecordBindVertexBuffer", +[](lua_State *l, prosper::ShaderGraphics &shader, prosper::ShaderBindState &bindState, Lua::Vulkan::Buffer &buffer) { Lua::Shader::Graphics::RecordBindVertexBuffer(l, shader, bindState, buffer, 0u, 0u); });

	defShaderGraphics.def("RecordBindVertexBuffers", &Lua::Shader::Graphics::RecordBindVertexBuffers);
	defShaderGraphics.def(
	  "RecordBindVertexBuffers", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, luabind::object buffers, uint32_t startBinding) { Lua::Shader::Graphics::RecordBindVertexBuffers(l, shader, recordTarget, buffers, startBinding, {}); });
	defShaderGraphics.def("RecordBindVertexBuffers", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, luabind::object buffers) { Lua::Shader::Graphics::RecordBindVertexBuffers(l, shader, recordTarget, buffers, 0u, {}); });
	defShaderGraphics.def("RecordBindIndexBuffer", &Lua::Shader::Graphics::RecordBindIndexBuffer);
	defShaderGraphics.def(
	  "RecordBindIndexBuffer", +[](lua_State *l, prosper::ShaderGraphics &shader, prosper::ShaderBindState &bindState, Lua::Vulkan::Buffer &indexBuffer, uint32_t indexType) { Lua::Shader::Graphics::RecordBindIndexBuffer(l, shader, bindState, indexBuffer, indexType, 0u); });
	defShaderGraphics.def("RecordDraw", static_cast<void (*)(lua_State *, prosper::ShaderGraphics &, const LuaShaderRecordTarget &, uint32_t, uint32_t, uint32_t, uint32_t)>(&Lua::Shader::Graphics::RecordDraw));
	defShaderGraphics.def(
	  "RecordDraw", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, uint32_t vertCount, uint32_t instanceCount, uint32_t firstVertex) { Lua::Shader::Graphics::RecordDraw(l, shader, recordTarget, vertCount, instanceCount, firstVertex, 0u); });
	defShaderGraphics.def("RecordDraw", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, uint32_t vertCount, uint32_t instanceCount) { Lua::Shader::Graphics::RecordDraw(l, shader, recordTarget, vertCount, instanceCount, 0u, 0u); });
	defShaderGraphics.def("RecordDraw", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, uint32_t vertCount) { Lua::Shader::Graphics::RecordDraw(l, shader, recordTarget, vertCount, 1u, 0u, 0u); });
	defShaderGraphics.def("RecordDrawIndexed", &Lua::Shader::Graphics::RecordDrawIndexed);
	defShaderGraphics.def(
	  "RecordDrawIndexed",
	  +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex) { Lua::Shader::Graphics::RecordDrawIndexed(l, shader, recordTarget, indexCount, instanceCount, firstIndex, 0u); });
	defShaderGraphics.def(
	  "RecordDrawIndexed", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, uint32_t indexCount, uint32_t instanceCount) { Lua::Shader::Graphics::RecordDrawIndexed(l, shader, recordTarget, indexCount, instanceCount, 0u, 0); });
	defShaderGraphics.def("RecordDrawIndexed", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget, uint32_t indexCount) { Lua::Shader::Graphics::RecordDrawIndexed(l, shader, recordTarget, indexCount, 1u, 0u, 0); });
	defShaderGraphics.def("RecordBeginDraw", &Lua::Shader::Graphics::RecordBeginDraw);
	defShaderGraphics.def("RecordBeginDraw", +[](lua_State *l, prosper::ShaderGraphics &shader, const LuaShaderRecordTarget &recordTarget) { Lua::Shader::Graphics::RecordBeginDraw(l, shader, recordTarget, 0u); });
	defShaderGraphics.def("RecordDraw", static_cast<void (*)(lua_State *, prosper::ShaderGraphics &, const LuaShaderRecordTarget &)>(&Lua::Shader::Graphics::RecordDraw));
	defShaderGraphics.def("RecordEndDraw", &Lua::Shader::Graphics::RecordEndDraw);
	defShaderGraphics.def("GetRenderPass", static_cast<void (*)(lua_State *, prosper::ShaderGraphics &, uint32_t)>(&Lua::Shader::Graphics::GetRenderPass));
	defShaderGraphics.def("GetRenderPass", static_cast<void (*)(lua_State *, prosper::ShaderGraphics &)>([](lua_State *l, prosper::ShaderGraphics &shader) { Lua::Shader::Graphics::GetRenderPass(l, shader, 0u); }));
	defShaderGraphics.scope[luabind::def("get_render_pass", static_cast<void (*)(lua_State *)>([](lua_State *l) {
		auto &rp = prosper::ShaderGraphics::GetRenderPass<prosper::ShaderGraphics>(c_engine->GetRenderContext());
		if(rp == nullptr)
			return;
		Lua::Push(l, rp);
	}))];
	modShader[defShaderGraphics];

	auto defShaderGUITextured = luabind::class_<wgui::ShaderTextured, luabind::bases<prosper::ShaderGraphics, prosper::Shader>>("GUITextured");
	modShader[defShaderGUITextured];

	auto defShaderScene = luabind::class_<pragma::ShaderScene, luabind::bases<prosper::ShaderGraphics, prosper::Shader>>("Scene3D");
	defShaderScene.scope[luabind::def("get_render_pass", &Lua::Shader::Scene3D::GetRenderPass)];
	defShaderScene.scope[luabind::def("get_render_pass", static_cast<void (*)(lua_State *)>([](lua_State *l) { Lua::Shader::Scene3D::GetRenderPass(l, 0u); }))];
	defShaderScene.add_static_constant("RENDER_PASS_COLOR_FORMAT", umath::to_integral(pragma::ShaderScene::RENDER_PASS_FORMAT));
	defShaderScene.add_static_constant("RENDER_PASS_BLOOM_FORMAT", umath::to_integral(pragma::ShaderScene::RENDER_PASS_FORMAT));
	defShaderScene.add_static_constant("RENDER_PASS_DEPTH_FORMAT", umath::to_integral(pragma::ShaderScene::RENDER_PASS_DEPTH_FORMAT));
	defShaderScene.def("GetCameraDescriptorSetIndex", &pragma::ShaderScene::GetCameraDescriptorSetIndex);
	defShaderScene.def("GetRendererDescriptorSetIndex", &pragma::ShaderScene::GetRendererDescriptorSetIndex);
	modShader[defShaderScene];

	auto defShaderSceneLit = luabind::class_<pragma::ShaderSceneLit, luabind::bases<pragma::ShaderScene, prosper::ShaderGraphics, prosper::Shader>>("SceneLit3D");
	modShader[defShaderSceneLit];

	auto defShaderEntity = luabind::class_<pragma::ShaderEntity, luabind::bases<pragma::ShaderSceneLit, pragma::ShaderScene, prosper::ShaderGraphics, prosper::Shader>>("ShaderEntity");
	modShader[defShaderEntity];

	auto defShaderGameWorld = luabind::class_<pragma::ShaderGameWorld, luabind::bases<pragma::ShaderEntity, pragma::ShaderSceneLit, pragma::ShaderScene, prosper::ShaderGraphics, prosper::Shader>>("GameWorld");
	defShaderGameWorld.def("GetMaterialDescriptorSetIndex", &pragma::ShaderGameWorld::GetMaterialDescriptorSetIndex);
	defShaderGameWorld.def("GetInstanceDescriptorSetIndex", &pragma::ShaderGameWorld::GetInstanceDescriptorSetIndex);
	defShaderGameWorld.def("GetRenderSettingsDescriptorSetIndex", &pragma::ShaderGameWorld::GetRenderSettingsDescriptorSetIndex);
	modShader[defShaderGameWorld];

	auto defShaderTextured3D = luabind::class_<pragma::ShaderGameWorldLightingPass, luabind::bases<pragma::ShaderGameWorld, pragma::ShaderEntity, pragma::ShaderSceneLit, pragma::ShaderScene, prosper::ShaderGraphics, prosper::Shader>>("TexturedLit3D");
	defShaderTextured3D.add_static_constant("PUSH_CONSTANTS_SIZE", sizeof(pragma::ShaderGameWorldLightingPass::PushConstants));
	defShaderTextured3D.add_static_constant("PUSH_CONSTANTS_USER_DATA_OFFSET", sizeof(pragma::ShaderGameWorldLightingPass::PushConstants));
	defShaderTextured3D.def("GetShaderMaterial", &pragma::ShaderGameWorldLightingPass::GetShaderMaterial);
	defShaderTextured3D.def("GetShaderMaterialName", &pragma::ShaderGameWorldLightingPass::GetShaderMaterialName);
	modShader[defShaderTextured3D];

	auto defShaderCompute = luabind::class_<prosper::ShaderCompute, prosper::Shader>("Compute");
	defShaderCompute.def("RecordDispatch", &Lua::Shader::Compute::RecordDispatch);
	defShaderCompute.def("RecordDispatch", +[](lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState, uint32_t x, uint32_t y) { Lua::Shader::Compute::RecordDispatch(l, shader, bindState, x, y, 1u); });
	defShaderCompute.def("RecordDispatch", +[](lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState, uint32_t x) { Lua::Shader::Compute::RecordDispatch(l, shader, bindState, x, 1u, 1u); });
	defShaderCompute.def("RecordDispatch", +[](lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState) { Lua::Shader::Compute::RecordDispatch(l, shader, bindState, 1u, 1u, 1u); });
	defShaderCompute.def("RecordBeginCompute", &Lua::Shader::Compute::RecordBeginCompute);
	defShaderCompute.def("RecordBeginCompute", +[](lua_State *l, prosper::ShaderCompute &shader, prosper::ShaderBindState &bindState, Lua::Vulkan::CommandBuffer &hCommandBuffer) { Lua::Shader::Compute::RecordBeginCompute(l, shader, bindState, 0u); });
	defShaderCompute.def("RecordCompute", &Lua::Shader::Compute::RecordCompute);
	defShaderCompute.def("RecordEndCompute", &Lua::Shader::Compute::RecordEndCompute);
	modShader[defShaderCompute];

	// Utility shaders
	auto defShaderComposeRMA = luabind::class_<pragma::ShaderComposeRMA, luabind::bases<prosper::ShaderGraphics, prosper::Shader>>("ComposeRMA");
	defShaderComposeRMA.add_static_constant("FLAG_NONE", umath::to_integral(pragma::ShaderComposeRMA::Flags::None));
	defShaderComposeRMA.add_static_constant("FLAG_USE_SPECULAR_WORKFLOW_BIT", umath::to_integral(pragma::ShaderComposeRMA::Flags::UseSpecularWorkflow));
	defShaderComposeRMA.def("ComposeRMA",
	  static_cast<void (*)(lua_State *, pragma::ShaderComposeRMA &, prosper::Texture *, prosper::Texture *, prosper::Texture *, uint32_t)>(
	    [](lua_State *l, pragma::ShaderComposeRMA &shader, prosper::Texture *roughnessMap, prosper::Texture *metalnessMap, prosper::Texture *aoMap, uint32_t flags) {
		    auto rma = shader.ComposeRMA(c_engine->GetRenderContext(), roughnessMap, metalnessMap, aoMap, static_cast<pragma::ShaderComposeRMA::Flags>(flags));
		    if(rma == nullptr)
			    return;
		    Lua::Push(l, rma);
	    }));
	modShader[defShaderComposeRMA];

	auto defShaderFlipImage = luabind::class_<prosper::ShaderFlipImage, luabind::bases<prosper::ShaderGraphics, prosper::Shader>>("FlipImage");
	defShaderFlipImage.def(
	  "RecordDraw", +[](prosper::ShaderFlipImage &shader, Lua::Vulkan::CommandBuffer &hCommandBuffer, Lua::Vulkan::DescriptorSet &ds, bool flipHorizontally, bool flipVertically) { return shader.RecordDraw(hCommandBuffer, *ds.GetDescriptorSet(), flipHorizontally, flipVertically); });
	modShader[defShaderFlipImage];

	auto defShaderMergeImages = luabind::class_<pragma::ShaderMergeImages, luabind::bases<prosper::ShaderGraphics, prosper::Shader>>("MergeImages");
	defShaderMergeImages.def("RecordDraw", +[](pragma::ShaderMergeImages &shader, Lua::Vulkan::CommandBuffer &hCommandBuffer, Lua::Vulkan::DescriptorSet &ds, Lua::Vulkan::DescriptorSet &ds2) { return shader.RecordDraw(hCommandBuffer, *ds.GetDescriptorSet(), *ds2.GetDescriptorSet()); });
	modShader[defShaderMergeImages];

	auto defShaderMergeIntoEquirect = luabind::class_<pragma::ShaderMerge2dImageIntoEquirectangular, luabind::bases<prosper::ShaderGraphics, prosper::Shader>>("Merge2dImageIntoEquirectangular");
	defShaderMergeIntoEquirect.add_static_constant("CUBE_FACE_POSITIVE_X", sizeof(pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace::PositiveX));
	defShaderMergeIntoEquirect.add_static_constant("CUBE_FACE_NEGATIVE_X", sizeof(pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace::NegativeX));
	defShaderMergeIntoEquirect.add_static_constant("CUBE_FACE_POSITIVE_Y", sizeof(pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace::PositiveY));
	defShaderMergeIntoEquirect.add_static_constant("CUBE_FACE_NEGATIVE_Y", sizeof(pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace::NegativeY));
	defShaderMergeIntoEquirect.add_static_constant("CUBE_FACE_POSITIVE_Z", sizeof(pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace::PositiveZ));
	defShaderMergeIntoEquirect.add_static_constant("CUBE_FACE_NEGATIVE_Z", sizeof(pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace::NegativeZ));
	static_assert(umath::to_integral(pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace::Count) == 6);
	defShaderMergeIntoEquirect.def(
	  "RecordDraw",
	  +[](pragma::ShaderMerge2dImageIntoEquirectangular &shader, Lua::Vulkan::CommandBuffer &hCommandBuffer, Lua::Vulkan::DescriptorSet &dsEquirect, Lua::Vulkan::DescriptorSet &ds2d) { return shader.RecordDraw(hCommandBuffer, *dsEquirect.GetDescriptorSet(), *ds2d.GetDescriptorSet()); });
	defShaderMergeIntoEquirect.def(
	  "RecordDraw", +[](pragma::ShaderMerge2dImageIntoEquirectangular &shader, Lua::Vulkan::CommandBuffer &hCommandBuffer, Lua::Vulkan::DescriptorSet &dsEquirect, Lua::Vulkan::DescriptorSet &ds2d, pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace cubeFace) {
		  return shader.RecordDraw(hCommandBuffer, *dsEquirect.GetDescriptorSet(), *ds2d.GetDescriptorSet(), cubeFace);
	  });
	defShaderMergeIntoEquirect.def(
	  "RecordDraw", +[](pragma::ShaderMerge2dImageIntoEquirectangular &shader, Lua::Vulkan::CommandBuffer &hCommandBuffer, Lua::Vulkan::DescriptorSet &dsEquirect, Lua::Vulkan::DescriptorSet &ds2d, pragma::ShaderMerge2dImageIntoEquirectangular::CubeFace cubeFace, umath::Degree range) {
		  return shader.RecordDraw(hCommandBuffer, *dsEquirect.GetDescriptorSet(), *ds2d.GetDescriptorSet(), cubeFace, range);
	  });
	modShader[defShaderMergeIntoEquirect];

	// Custom Shaders
	auto defVertexBinding = luabind::class_<pragma::LuaVertexBinding>("VertexBinding");
	defVertexBinding.def(luabind::constructor<>());
	defVertexBinding.def(luabind::constructor<uint32_t, uint32_t>());
	defVertexBinding.def(luabind::constructor<uint32_t>());
	defVertexBinding.def_readwrite("inputRate", reinterpret_cast<uint32_t pragma::LuaVertexBinding::*>(&pragma::LuaVertexBinding::inputRate));
	defVertexBinding.def_readwrite("stride", &pragma::LuaVertexBinding::stride);
	modShader[defVertexBinding];

	auto defVertexAttribute = luabind::class_<pragma::LuaVertexAttribute>("VertexAttribute");
	defVertexAttribute.def(luabind::constructor<>());
	defVertexAttribute.def(luabind::constructor<uint32_t, uint32_t, uint32_t>());
	defVertexAttribute.def(luabind::constructor<uint32_t, uint32_t>());
	defVertexAttribute.def(luabind::constructor<uint32_t>());
	defVertexAttribute.def_readwrite("format", reinterpret_cast<uint32_t pragma::LuaVertexAttribute::*>(&pragma::LuaVertexAttribute::format));
	defVertexAttribute.def_readwrite("offset", &pragma::LuaVertexAttribute::offset);
	defVertexAttribute.def_readwrite("location", &pragma::LuaVertexAttribute::location);
	modShader[defVertexAttribute];

	auto defDescriptorSetInfo = luabind::class_<pragma::LuaDescriptorSetInfo>("DescriptorSetInfo");
	defDescriptorSetInfo.def(luabind::constructor<>());
	defDescriptorSetInfo.def(luabind::constructor<const std::string &, luabind::object, uint32_t>());
	defDescriptorSetInfo.def(luabind::constructor<const std::string &, luabind::object>());
	defDescriptorSetInfo.def(luabind::constructor<luabind::object, uint32_t>());
	defDescriptorSetInfo.def(luabind::constructor<luabind::object>());
	defDescriptorSetInfo.def_readwrite("setIndex", &pragma::LuaDescriptorSetInfo::setIndex);
	modShader[defDescriptorSetInfo];

	auto defDescriptorSetBinding = luabind::class_<pragma::LuaDescriptorSetBinding>("DescriptorSetBinding");
	defDescriptorSetBinding.def(luabind::constructor<const std::string &, uint32_t, uint32_t, uint32_t, uint32_t>());
	defDescriptorSetBinding.def(luabind::constructor<const std::string &, uint32_t, uint32_t, uint32_t>());
	defDescriptorSetBinding.def(luabind::constructor<const std::string &, uint32_t, uint32_t>());
	defDescriptorSetBinding.def(luabind::constructor<uint32_t, uint32_t, uint32_t, uint32_t>());
	defDescriptorSetBinding.def(luabind::constructor<uint32_t, uint32_t, uint32_t>());
	defDescriptorSetBinding.def(luabind::constructor<uint32_t, uint32_t>());
	defDescriptorSetBinding.def_readwrite("type", reinterpret_cast<uint32_t pragma::LuaDescriptorSetBinding::*>(&pragma::LuaDescriptorSetBinding::type));
	defDescriptorSetBinding.def_readwrite("shaderStages", reinterpret_cast<uint32_t pragma::LuaDescriptorSetBinding::*>(&pragma::LuaDescriptorSetBinding::shaderStages));
	defDescriptorSetBinding.def_readwrite("bindingIndex", &pragma::LuaDescriptorSetBinding::bindingIndex);
	defDescriptorSetBinding.def_readwrite("descriptorArraySize", &pragma::LuaDescriptorSetBinding::descriptorArraySize);
	modShader[defDescriptorSetBinding];

	auto defShaderBasePipelineCreateInfo = luabind::class_<prosper::BasePipelineCreateInfo>("BasePipelineCreateInfo");
	modShader[defShaderBasePipelineCreateInfo];

	auto defShaderGraphicsPipelineCreateInfo = luabind::class_<prosper::GraphicsPipelineCreateInfo, prosper::BasePipelineCreateInfo>("GraphicsPipelineCreateInfo");
	defShaderGraphicsPipelineCreateInfo.def("SetBlendingProperties", &Lua::GraphicsPipelineCreateInfo::SetBlendingProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetCommonAlphaBlendProperties", &Lua::GraphicsPipelineCreateInfo::SetCommonAlphaBlendProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetColorBlendAttachmentProperties", &Lua::GraphicsPipelineCreateInfo::SetColorBlendAttachmentProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetMultisamplingProperties", &Lua::GraphicsPipelineCreateInfo::SetMultisamplingProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetSampleCount", &Lua::GraphicsPipelineCreateInfo::SetSampleCount);
	defShaderGraphicsPipelineCreateInfo.def("SetSampleShadingEnabled", &Lua::GraphicsPipelineCreateInfo::SetSampleShadingEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetSampleMaskEnabled", &Lua::GraphicsPipelineCreateInfo::SetSampleMaskEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetDynamicScissorBoxesCount", &Lua::GraphicsPipelineCreateInfo::SetDynamicScissorBoxesCount);
	defShaderGraphicsPipelineCreateInfo.def("SetDynamicViewportsCount", &Lua::GraphicsPipelineCreateInfo::SetDynamicViewportsCount);
	defShaderGraphicsPipelineCreateInfo.def("SetPrimitiveTopology", &Lua::GraphicsPipelineCreateInfo::SetPrimitiveTopology);
	defShaderGraphicsPipelineCreateInfo.def("SetRasterizationProperties", &Lua::GraphicsPipelineCreateInfo::SetRasterizationProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetPolygonMode", &Lua::GraphicsPipelineCreateInfo::SetPolygonMode);
	defShaderGraphicsPipelineCreateInfo.def("SetCullMode", &Lua::GraphicsPipelineCreateInfo::SetCullMode);
	defShaderGraphicsPipelineCreateInfo.def("SetFrontFace", &Lua::GraphicsPipelineCreateInfo::SetFrontFace);
	defShaderGraphicsPipelineCreateInfo.def("SetLineWidth", &Lua::GraphicsPipelineCreateInfo::SetLineWidth);
	defShaderGraphicsPipelineCreateInfo.def("SetScissorBoxProperties", &Lua::GraphicsPipelineCreateInfo::SetScissorBoxProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetStencilTestProperties", &Lua::GraphicsPipelineCreateInfo::SetStencilTestProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetViewportProperties", &Lua::GraphicsPipelineCreateInfo::SetViewportProperties);
	defShaderGraphicsPipelineCreateInfo.def("AreDepthWritesEnabled", &Lua::GraphicsPipelineCreateInfo::AreDepthWritesEnabled);
	defShaderGraphicsPipelineCreateInfo.def("GetBlendingProperties", &Lua::GraphicsPipelineCreateInfo::GetBlendingProperties);
	defShaderGraphicsPipelineCreateInfo.def("GetColorBlendAttachmentProperties", &Lua::GraphicsPipelineCreateInfo::GetColorBlendAttachmentProperties);
	defShaderGraphicsPipelineCreateInfo.def("GetDepthBiasState", &Lua::GraphicsPipelineCreateInfo::GetDepthBiasState);
	defShaderGraphicsPipelineCreateInfo.def("GetDepthBiasConstantFactor", &Lua::GraphicsPipelineCreateInfo::GetDepthBiasConstantFactor);
	defShaderGraphicsPipelineCreateInfo.def("GetDepthBiasClamp", &Lua::GraphicsPipelineCreateInfo::GetDepthBiasClamp);
	defShaderGraphicsPipelineCreateInfo.def("GetDepthBiasSlopeFactor", &Lua::GraphicsPipelineCreateInfo::GetDepthBiasSlopeFactor);
	defShaderGraphicsPipelineCreateInfo.def("GetDepthBoundsState", &Lua::GraphicsPipelineCreateInfo::GetDepthBoundsState);
	defShaderGraphicsPipelineCreateInfo.def("GetMinDepthBounds", &Lua::GraphicsPipelineCreateInfo::GetMinDepthBounds);
	defShaderGraphicsPipelineCreateInfo.def("GetMaxDepthBounds", &Lua::GraphicsPipelineCreateInfo::GetMaxDepthBounds);
	defShaderGraphicsPipelineCreateInfo.def("GetDepthClamp", &Lua::GraphicsPipelineCreateInfo::GetDepthClamp);
	defShaderGraphicsPipelineCreateInfo.def("GetDepthTestState", &Lua::GraphicsPipelineCreateInfo::GetDepthTestState);
	defShaderGraphicsPipelineCreateInfo.def("GetDynamicStates", &Lua::GraphicsPipelineCreateInfo::GetDynamicStates);
	defShaderGraphicsPipelineCreateInfo.def("GetScissorCount", &Lua::GraphicsPipelineCreateInfo::GetScissorCount);
	defShaderGraphicsPipelineCreateInfo.def("GetViewportCount", &Lua::GraphicsPipelineCreateInfo::GetViewportCount);
	defShaderGraphicsPipelineCreateInfo.def("GetVertexAttributeCount", &Lua::GraphicsPipelineCreateInfo::GetVertexAttributeCount);
	defShaderGraphicsPipelineCreateInfo.def("GetLogicOpState", &Lua::GraphicsPipelineCreateInfo::GetLogicOpState);
	defShaderGraphicsPipelineCreateInfo.def("GetMultisamplingProperties", &Lua::GraphicsPipelineCreateInfo::GetMultisamplingProperties);
	defShaderGraphicsPipelineCreateInfo.def("GetSampleCount", &Lua::GraphicsPipelineCreateInfo::GetSampleCount);
	defShaderGraphicsPipelineCreateInfo.def("GetMinSampleShading", &Lua::GraphicsPipelineCreateInfo::GetMinSampleShading);
	defShaderGraphicsPipelineCreateInfo.def("GetSampleMask", &Lua::GraphicsPipelineCreateInfo::GetSampleMask);
	defShaderGraphicsPipelineCreateInfo.def("GetDynamicScissorBoxesCount", &Lua::GraphicsPipelineCreateInfo::GetDynamicScissorBoxesCount);
	defShaderGraphicsPipelineCreateInfo.def("GetDynamicViewportsCount", &Lua::GraphicsPipelineCreateInfo::GetDynamicViewportsCount);
	defShaderGraphicsPipelineCreateInfo.def("GetScissorBoxesCount", &Lua::GraphicsPipelineCreateInfo::GetScissorBoxesCount);
	defShaderGraphicsPipelineCreateInfo.def("GetViewportsCount", &Lua::GraphicsPipelineCreateInfo::GetViewportsCount);
	defShaderGraphicsPipelineCreateInfo.def("GetPrimitiveTopology", &Lua::GraphicsPipelineCreateInfo::GetPrimitiveTopology);
	defShaderGraphicsPipelineCreateInfo.def("GetPushConstantRanges", &Lua::GraphicsPipelineCreateInfo::GetPushConstantRanges);
	defShaderGraphicsPipelineCreateInfo.def("GetRasterizationProperties", &Lua::GraphicsPipelineCreateInfo::GetRasterizationProperties);
	defShaderGraphicsPipelineCreateInfo.def("GetPolygonMode", &Lua::GraphicsPipelineCreateInfo::GetPolygonMode);
	defShaderGraphicsPipelineCreateInfo.def("GetCullMode", &Lua::GraphicsPipelineCreateInfo::GetCullMode);
	defShaderGraphicsPipelineCreateInfo.def("GetFrontFace", &Lua::GraphicsPipelineCreateInfo::GetFrontFace);
	defShaderGraphicsPipelineCreateInfo.def("GetLineWidth", &Lua::GraphicsPipelineCreateInfo::GetLineWidth);
	defShaderGraphicsPipelineCreateInfo.def("GetSampleShadingState", &Lua::GraphicsPipelineCreateInfo::GetSampleShadingState);
	defShaderGraphicsPipelineCreateInfo.def("GetScissorBoxProperties", &Lua::GraphicsPipelineCreateInfo::GetScissorBoxProperties);
	defShaderGraphicsPipelineCreateInfo.def("GetStencilTestProperties", &Lua::GraphicsPipelineCreateInfo::GetStencilTestProperties);
	defShaderGraphicsPipelineCreateInfo.def("GetSubpassId", &Lua::GraphicsPipelineCreateInfo::GetSubpassId);
	defShaderGraphicsPipelineCreateInfo.def("GetVertexAttributeProperties", &Lua::GraphicsPipelineCreateInfo::GetVertexAttributeProperties);
	defShaderGraphicsPipelineCreateInfo.def("GetViewportProperties", &Lua::GraphicsPipelineCreateInfo::GetViewportProperties);
	defShaderGraphicsPipelineCreateInfo.def("IsAlphaToCoverageEnabled", &Lua::GraphicsPipelineCreateInfo::IsAlphaToCoverageEnabled);
	defShaderGraphicsPipelineCreateInfo.def("IsAlphaToOneEnabled", &Lua::GraphicsPipelineCreateInfo::IsAlphaToOneEnabled);
	defShaderGraphicsPipelineCreateInfo.def("IsDepthClampEnabled", &Lua::GraphicsPipelineCreateInfo::IsDepthClampEnabled);
	defShaderGraphicsPipelineCreateInfo.def("IsPrimitiveRestartEnabled", &Lua::GraphicsPipelineCreateInfo::IsPrimitiveRestartEnabled);
	defShaderGraphicsPipelineCreateInfo.def("IsRasterizerDiscardEnabled", &Lua::GraphicsPipelineCreateInfo::IsRasterizerDiscardEnabled);
	defShaderGraphicsPipelineCreateInfo.def("IsSampleMaskEnabled", &Lua::GraphicsPipelineCreateInfo::IsSampleMaskEnabled);
	defShaderGraphicsPipelineCreateInfo.def("AddSpecializationConstant", &Lua::GraphicsPipelineCreateInfo::AddSpecializationConstant);
	defShaderGraphicsPipelineCreateInfo.def("SetAlphaToCoverageEnabled", &Lua::GraphicsPipelineCreateInfo::SetAlphaToCoverageEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetAlphaToOneEnabled", &Lua::GraphicsPipelineCreateInfo::SetAlphaToOneEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthBiasEnabled", &Lua::GraphicsPipelineCreateInfo::SetDepthBiasEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthBiasProperties", &Lua::GraphicsPipelineCreateInfo::SetDepthBiasProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthBiasConstantFactor", &Lua::GraphicsPipelineCreateInfo::SetDepthBiasConstantFactor);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthBiasClamp", &Lua::GraphicsPipelineCreateInfo::SetDepthBiasClamp);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthBiasSlopeFactor", &Lua::GraphicsPipelineCreateInfo::SetDepthBiasSlopeFactor);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthBoundsTestEnabled", &Lua::GraphicsPipelineCreateInfo::SetDepthBoundsTestEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthBoundsTestProperties", &Lua::GraphicsPipelineCreateInfo::SetDepthBoundsTestProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetMinDepthBounds", &Lua::GraphicsPipelineCreateInfo::SetMinDepthBounds);
	defShaderGraphicsPipelineCreateInfo.def("SetMaxDepthBounds", &Lua::GraphicsPipelineCreateInfo::SetMaxDepthBounds);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthClampEnabled", &Lua::GraphicsPipelineCreateInfo::SetDepthClampEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthTestProperties", &Lua::GraphicsPipelineCreateInfo::SetDepthTestProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthTestEnabled", &Lua::GraphicsPipelineCreateInfo::SetDepthTestEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetDepthWritesEnabled", &Lua::GraphicsPipelineCreateInfo::SetDepthWritesEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetDynamicStates", &Lua::GraphicsPipelineCreateInfo::SetDynamicStates);
	defShaderGraphicsPipelineCreateInfo.def("SetDynamicStateEnabled", &Lua::GraphicsPipelineCreateInfo::SetDynamicStateEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetLogicOpProperties", &Lua::GraphicsPipelineCreateInfo::SetLogicOpProperties);
	defShaderGraphicsPipelineCreateInfo.def("SetLogicOpEnabled", &Lua::GraphicsPipelineCreateInfo::SetLogicOpEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetPrimitiveRestartEnabled", &Lua::GraphicsPipelineCreateInfo::SetPrimitiveRestartEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetRasterizerDiscardEnabled", &Lua::GraphicsPipelineCreateInfo::SetRasterizerDiscardEnabled);
	defShaderGraphicsPipelineCreateInfo.def("SetSampleMask", &Lua::GraphicsPipelineCreateInfo::SetSampleMask);
	defShaderGraphicsPipelineCreateInfo.def("SetMinSampleShading", &Lua::GraphicsPipelineCreateInfo::SetMinSampleShading);
	defShaderGraphicsPipelineCreateInfo.def("SetStencilTestEnabled", &Lua::GraphicsPipelineCreateInfo::SetStencilTestEnabled);
	modShader[defShaderGraphicsPipelineCreateInfo];

	auto defShaderComputePipelineCreateInfo = luabind::class_<prosper::ComputePipelineCreateInfo, prosper::BasePipelineCreateInfo>("ComputePipelineCreateInfo");
	defShaderComputePipelineCreateInfo.def("AddSpecializationConstant", &Lua::ComputePipelineCreateInfo::AddSpecializationConstant);
	modShader[defShaderComputePipelineCreateInfo];

	auto defShaderModule = luabind::class_<pragma::LuaShaderWrapperBase>("BaseModule");
	defShaderModule.def("SetShaderSource", &Lua::Shader::SetStageSourceFilePath);
	defShaderModule.def("SetPipelineCount", &Lua::Shader::SetPipelineCount);
	defShaderModule.def("GetShader", static_cast<prosper::Shader &(pragma::LuaShaderWrapperBase::*)() const>(&pragma::LuaShaderWrapperBase::GetShader));
	defShaderModule.def("AttachPushConstantRange", &Lua::Shader::AttachPushConstantRange);
	defShaderModule.def("AttachDescriptorSetInfo", &Lua::Shader::AttachDescriptorSetInfo);

	defShaderModule.def("InitializePipeline", &pragma::LuaShaderWrapperBase::Lua_InitializePipeline, &pragma::LuaShaderWrapperBase::Lua_default_InitializePipeline);
	defShaderModule.def("InitializeShaderResources", &pragma::LuaShaderWrapperBase::Lua_InitializeShaderResources, &pragma::LuaShaderWrapperBase::Lua_default_InitializeShaderResources);
	defShaderModule.def("OnInitialized", &pragma::LuaShaderWrapperBase::Lua_OnInitialized, &pragma::LuaShaderWrapperBase::Lua_default_OnInitialized);
	defShaderModule.def("OnPipelinesInitialized", &pragma::LuaShaderWrapperBase::Lua_OnPipelinesInitialized, &pragma::LuaShaderWrapperBase::Lua_default_OnPipelinesInitialized);
	defShaderModule.def("OnPipelineInitialized", &pragma::LuaShaderWrapperBase::Lua_OnPipelineInitialized, &pragma::LuaShaderWrapperBase::Lua_default_OnPipelineInitialized);
	modShader[defShaderModule];

	auto defShaderGraphicsModule = luabind::class_<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>("BaseGraphicsModule");
	defShaderGraphicsModule.def("AttachVertexAttribute", &Lua::Shader::Graphics::AttachVertexAttribute);
	modShader[defShaderGraphicsModule];

	auto defShaderComputeModule = luabind::class_<pragma::LuaShaderWrapperComputeBase, pragma::LuaShaderWrapperBase>("BaseComputeModule");
	// defShaderComputeModule.def("GetCurrentComputeCommandBuffer",&Lua::Shader::GetCurrentComputeCommandBuffer);
	modShader[defShaderComputeModule];

	auto defShaderGraphicsBase = luabind::class_<pragma::LuaShaderWrapperGraphics, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BaseGraphics");
	defShaderGraphicsBase.def(luabind::constructor<>());
	modShader[defShaderGraphicsBase];

	auto defShaderComputeBase = luabind::class_<pragma::LuaShaderWrapperCompute, luabind::bases<pragma::LuaShaderWrapperComputeBase, pragma::LuaShaderWrapperBase>>("BaseCompute");
	defShaderComputeBase.def(luabind::constructor<>());
	modShader[defShaderComputeBase];

	using PcbArg = prosper::util::PreparedCommand::Argument;
	auto defShaderGUIBase = luabind::class_<pragma::LuaShaderWrapperGUI, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BaseGUI");
	defShaderGUIBase.def(luabind::constructor<>());
	defShaderGUIBase.def(
	  "RecordBeginDraw", +[](pragma::LuaShaderWrapperGUI &shader, prosper::ShaderBindState &bindState, wgui::DrawState &drawState, uint32_t width, uint32_t height, wgui::StencilPipeline pipelineIdx, bool msaa, uint32_t testStencilLevel) -> bool {
		  return static_cast<pragma::LShaderGui *>(&shader.GetShader())->RecordBeginDraw(bindState, drawState, width, height, pipelineIdx, msaa, testStencilLevel);
	  });
	defShaderGUIBase.def(
	  "RecordBeginDraw", +[](lua_State *l, pragma::LuaShaderWrapperGUI &shader, prosper::util::PreparedCommandBuffer &pcb) {
		  auto hShader = shader.GetShader().GetHandle();
		  pcb.PushCommand(
		    [hShader](const prosper::util::PreparedCommandBufferRecordState &recordState) -> bool {
			    if(hShader.expired()) // We only have to check the validity in BeginDraw!
				    return false;
			    auto &shader = *static_cast<pragma::LShaderGui *>(hShader.get());
			    recordState.shaderBindState = std::make_unique<prosper::ShaderBindState>(recordState.commandBuffer);
			    auto &drawState = recordState.userData.Get<wgui::DrawState>(ustring::string_switch::hash("guiDrawState"));
			    return shader.RecordBeginDraw(*recordState.shaderBindState, drawState, recordState.GetArgument<uint32_t>(0), recordState.GetArgument<uint32_t>(1), static_cast<wgui::StencilPipeline>(recordState.GetArgument<std::underlying_type_t<wgui::StencilPipeline>>(2)),
			      recordState.GetArgument<bool>(3), recordState.GetArgument<uint32_t>(4));
		    },
		    util::make_vector<PcbArg>(PcbArg {"w"}, PcbArg {"h"}, PcbArg {"stencilPipeline"}, PcbArg {"msaa"}, PcbArg {"testStencilLevel"}));
	  });
	modShader[defShaderGUIBase];

	auto defShaderGUITexturedBase = luabind::class_<pragma::LuaShaderWrapperGUITextured, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BaseGUITextured");
	defShaderGUITexturedBase.def(luabind::constructor<>());
	modShader[defShaderGUITexturedBase];

	auto defShaderParticleBase = luabind::class_<pragma::LuaShaderWrapperParticle2D, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BaseParticle2D");
	defShaderParticleBase.scope[luabind::def("get_depth_pipeline_render_pass", static_cast<void (*)(lua_State *)>([](lua_State *l) {
		auto &rp = prosper::ShaderGraphics::GetRenderPass<pragma::ShaderParticle2DBase>(c_engine->GetRenderContext(), pragma::ShaderParticle2DBase::GetDepthPipelineIndex());
		if(rp == nullptr)
			return;
		Lua::Push(l, rp);
	}))];
	defShaderParticleBase.def(luabind::constructor<>());
	defShaderParticleBase.add_static_constant("PUSH_CONSTANTS_SIZE", sizeof(pragma::ShaderParticle2DBase::PushConstants));
	defShaderParticleBase.add_static_constant("PUSH_CONSTANTS_USER_DATA_OFFSET", sizeof(pragma::ShaderParticle2DBase::PushConstants));
	defShaderParticleBase.def(
	  "RecordBeginDraw", +[](lua_State *l, pragma::LuaShaderWrapperParticle2D &shader, prosper::ShaderBindState &bindState, pragma::CParticleSystemComponent &ps, uint32_t renderFlags) {
		  return static_cast<pragma::LShaderParticle2D *>(&shader.GetShader())->RecordBeginDraw(bindState, ps, static_cast<pragma::ParticleRenderFlags>(renderFlags));
	  });

	modShader[defShaderParticleBase];

	auto defShaderPostProcessingBase = luabind::class_<pragma::LuaShaderWrapperPostProcessing, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BasePostProcessing");
	defShaderPostProcessingBase.def(luabind::constructor<>());
	modShader[defShaderPostProcessingBase];

	auto defShaderImageProcessing = luabind::class_<pragma::LuaShaderWrapperImageProcessing, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BaseImageProcessing");
	defShaderImageProcessing.add_static_constant("DESCRIPTOR_SET_TEXTURE", 0);
	defShaderImageProcessing.add_static_constant("DESCRIPTOR_SET_TEXTURE_BINDING_TEXTURE", 0);
	defShaderImageProcessing.def(luabind::constructor<>());
	defShaderImageProcessing.def(
	  "RecordDraw", +[](pragma::LuaShaderWrapperImageProcessing &shader, prosper::ShaderBindState &bindState, prosper::IDescriptorSetGroup &dsg) -> bool { return static_cast<prosper::ShaderBaseImageProcessing &>(shader.GetShader()).RecordDraw(bindState, *dsg.GetDescriptorSet()); });
	modShader[defShaderImageProcessing];

	auto defShaderTextured3DBase = luabind::class_<pragma::LuaShaderWrapperTextured3D, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BaseTexturedLit3D");
	defShaderTextured3DBase.add_static_constant("SPECIALIZATION_CONSTANT_FLAG_NONE", umath::to_integral(pragma::GameShaderSpecializationConstantFlag::None));
	defShaderTextured3DBase.add_static_constant("SPECIALIZATION_CONSTANT_ENABLE_LIGHT_MAPS_BIT", umath::to_integral(pragma::GameShaderSpecializationConstantFlag::EnableLightMapsBit));
	defShaderTextured3DBase.add_static_constant("SPECIALIZATION_CONSTANT_ENABLE_ANIMATION_BIT", umath::to_integral(pragma::GameShaderSpecializationConstantFlag::EnableAnimationBit));
	defShaderTextured3DBase.add_static_constant("SPECIALIZATION_CONSTANT_ENABLE_MORPH_TARGET_ANIMATION_BIT", umath::to_integral(pragma::GameShaderSpecializationConstantFlag::EnableMorphTargetAnimationBit));
	defShaderTextured3DBase.add_static_constant("SPECIALIZATION_CONSTANT_ENABLE_TRANSLUCENCY_BIT", umath::to_integral(pragma::GameShaderSpecializationConstantFlag::EnableTranslucencyBit));
	static_assert(umath::to_integral(pragma::GameShaderSpecializationConstantFlag::Last) == 8, "Update this list when adding new flags!");

	defShaderTextured3DBase.def(luabind::constructor<>());
	defShaderTextured3DBase.def("InitializeGfxPipelineVertexAttributes", &pragma::LuaShaderWrapperTextured3D::Lua_InitializeGfxPipelineVertexAttributes, &pragma::LuaShaderWrapperTextured3D::Lua_default_InitializeGfxPipelineVertexAttributes);
	defShaderTextured3DBase.def("InitializeGfxPipelinePushConstantRanges", &pragma::LuaShaderWrapperTextured3D::Lua_InitializeGfxPipelinePushConstantRanges, &pragma::LuaShaderWrapperTextured3D::Lua_default_InitializeGfxPipelinePushConstantRanges);
	defShaderTextured3DBase.def("InitializeGfxPipelineDescriptorSets", &pragma::LuaShaderWrapperTextured3D::Lua_InitializeGfxPipelineDescriptorSets, &pragma::LuaShaderWrapperTextured3D::Lua_default_InitializeGfxPipelineDescriptorSets);
	defShaderTextured3DBase.def("InitializeMaterialDescriptorSet", &pragma::LuaShaderWrapperTextured3D::Lua_InitializeMaterialDescriptorSet, &pragma::LuaShaderWrapperTextured3D::Lua_default_InitializeMaterialDescriptorSet);
	defShaderTextured3DBase.def("InitializeMaterialData", &pragma::LuaShaderWrapperTextured3D::Lua_InitializeMaterialData, &pragma::LuaShaderWrapperTextured3D::Lua_default_InitializeMaterialData);
	defShaderTextured3DBase.def("SetPushConstants", &pragma::LuaShaderWrapperTextured3D::SetPushConstants);
	defShaderTextured3DBase.def("GetBindPcb", &pragma::LuaShaderWrapperTextured3D::GetBindPcb);
	defShaderTextured3DBase.def("InitializeMaterialBuffer", &pragma::LuaShaderWrapperTextured3D::InitializeMaterialBuffer);

	defShaderTextured3DBase.def("OnBindMaterial", &pragma::LuaShaderWrapperTextured3D::Lua_OnBindMaterial, &pragma::LuaShaderWrapperTextured3D::Lua_default_OnBindMaterial);
	defShaderTextured3DBase.def("OnDraw", &pragma::LuaShaderWrapperTextured3D::Lua_OnDraw, &pragma::LuaShaderWrapperTextured3D::Lua_default_OnDraw);
	defShaderTextured3DBase.def("OnBindEntity", &pragma::LuaShaderWrapperTextured3D::Lua_OnBindEntity, &pragma::LuaShaderWrapperTextured3D::Lua_default_OnBindEntity);
	defShaderTextured3DBase.def("OnBindScene", &pragma::LuaShaderWrapperTextured3D::Lua_OnBindScene, &pragma::LuaShaderWrapperTextured3D::Lua_default_OnBindScene);
	defShaderTextured3DBase.def("OnBeginDraw", &pragma::LuaShaderWrapperTextured3D::Lua_OnBeginDraw, &pragma::LuaShaderWrapperTextured3D::Lua_default_OnBeginDraw);
	defShaderTextured3DBase.def("OnEndDraw", &pragma::LuaShaderWrapperTextured3D::Lua_OnEndDraw, &pragma::LuaShaderWrapperTextured3D::Lua_default_OnEndDraw);
	defShaderTextured3DBase.def("IsDepthPrepassEnabled", +[](const pragma::LuaShaderWrapperTextured3D &shader) { return static_cast<pragma::ShaderGameWorldLightingPass &>(shader.GetShader()).IsDepthPrepassEnabled(); });
	defShaderTextured3DBase.def("SetDepthPrepassEnabled", +[](pragma::LuaShaderWrapperTextured3D &shader, bool depthPrepassEnabled) { static_cast<pragma::ShaderGameWorldLightingPass &>(shader.GetShader()).SetDepthPrepassEnabled(depthPrepassEnabled); });
	defShaderTextured3DBase.def("SetShaderMaterialName", +[](pragma::LuaShaderWrapperTextured3D &shader, const std::optional<std::string> &shaderMatName) { static_cast<pragma::ShaderGameWorldLightingPass &>(shader.GetShader()).SetShaderMaterialName(shaderMatName); });
	defShaderTextured3DBase.def("GetShaderMaterialName", +[](pragma::LuaShaderWrapperTextured3D &shader) -> std::optional<std::string> { return static_cast<pragma::ShaderGameWorldLightingPass &>(shader.GetShader()).GetShaderMaterialName(); });
	modShader[defShaderTextured3DBase];

	auto defShaderPbr = luabind::class_<pragma::LuaShaderWrapperPbr, luabind::bases<pragma::LuaShaderWrapperGraphicsBase, pragma::LuaShaderWrapperBase>>("BasePbr");
	defShaderPbr.def(luabind::constructor<>());
	defShaderPbr.def("InitializeGfxPipelineVertexAttributes", &pragma::LuaShaderWrapperPbr::Lua_InitializeGfxPipelineVertexAttributes, &pragma::LuaShaderWrapperPbr::Lua_default_InitializeGfxPipelineVertexAttributes);
	defShaderPbr.def("InitializeGfxPipelinePushConstantRanges", &pragma::LuaShaderWrapperPbr::Lua_InitializeGfxPipelinePushConstantRanges, &pragma::LuaShaderWrapperPbr::Lua_default_InitializeGfxPipelinePushConstantRanges);
	defShaderPbr.def("InitializeGfxPipelineDescriptorSets", &pragma::LuaShaderWrapperPbr::Lua_InitializeGfxPipelineDescriptorSets, &pragma::LuaShaderWrapperPbr::Lua_default_InitializeGfxPipelineDescriptorSets);

	defShaderPbr.def("OnBindMaterial", &pragma::LuaShaderWrapperPbr::Lua_OnBindMaterial, &pragma::LuaShaderWrapperPbr::Lua_default_OnBindMaterial);
	defShaderPbr.def("OnDraw", &pragma::LuaShaderWrapperPbr::Lua_OnDraw, &pragma::LuaShaderWrapperPbr::Lua_default_OnDraw);
	defShaderPbr.def("OnBindEntity", &pragma::LuaShaderWrapperPbr::Lua_OnBindEntity, &pragma::LuaShaderWrapperPbr::Lua_default_OnBindEntity);
	defShaderPbr.def("OnBindScene", &pragma::LuaShaderWrapperPbr::Lua_OnBindScene, &pragma::LuaShaderWrapperPbr::Lua_default_OnBindScene);
	defShaderPbr.def("OnBeginDraw", &pragma::LuaShaderWrapperPbr::Lua_OnBeginDraw, &pragma::LuaShaderWrapperPbr::Lua_default_OnBeginDraw);
	defShaderPbr.def("OnEndDraw", &pragma::LuaShaderWrapperPbr::Lua_OnEndDraw, &pragma::LuaShaderWrapperPbr::Lua_default_OnEndDraw);
	modShader[defShaderPbr];
}

void CGame::RegisterLuaClasses()
{
	Game::RegisterLuaClasses();
	ClientState::RegisterSharedLuaClasses(GetLuaInterface());

	auto debugMod = luabind::module(GetLuaState(), "debug");
	auto defDebugRendererObject = luabind::class_<DebugRenderer::BaseObject>("RendererObject");
	defDebugRendererObject.def("Remove", &::DebugRenderer::BaseObject::Remove);
	defDebugRendererObject.def("IsValid", &::DebugRenderer::BaseObject::IsValid);
	defDebugRendererObject.def("SetPos", &::DebugRenderer::BaseObject::SetPos);
	defDebugRendererObject.def("GetPos", &::DebugRenderer::BaseObject::GetPos, luabind::copy_policy<0> {});
	defDebugRendererObject.def("SetRotation", &::DebugRenderer::BaseObject::SetRotation);
	defDebugRendererObject.def("GetRotation", &::DebugRenderer::BaseObject::GetRotation, luabind::copy_policy<0> {});
	defDebugRendererObject.def("SetAngles", &::DebugRenderer::BaseObject::SetAngles);
	defDebugRendererObject.def("GetAngles", &::DebugRenderer::BaseObject::GetAngles);
	defDebugRendererObject.def("IsVisible", &::DebugRenderer::BaseObject::IsVisible);
	defDebugRendererObject.def("SetVisible", &::DebugRenderer::BaseObject::SetVisible);
	defDebugRendererObject.def("ShouldIgnoreDepth", &::DebugRenderer::BaseObject::ShouldIgnoreDepth);
	defDebugRendererObject.def("SetIgnoreDepth", &::DebugRenderer::BaseObject::SetIgnoreDepth);
	defDebugRendererObject.def("SetScale", &::DebugRenderer::BaseObject::SetScale);
	defDebugRendererObject.def("GetScale", &::DebugRenderer::BaseObject::GetScale, luabind::copy_policy<0> {});
	defDebugRendererObject.def("SetPose", &::DebugRenderer::BaseObject::SetPose);
	defDebugRendererObject.def(
	  "GetVertexCount", +[](DebugRenderer::BaseObject &bo) -> size_t {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return 0;
		  return o->GetVertexCount();
	  });
	defDebugRendererObject.def(
	  "GetVertexPosition", +[](DebugRenderer::BaseObject &bo, size_t index) -> std::optional<Vector3> {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return {};
		  return o->GetVertexPosition(index);
	  });
	defDebugRendererObject.def(
	  "SetVertexPosition", +[](DebugRenderer::BaseObject &bo, size_t index, const Vector3 &pos) {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return;
		  o->SetVertexPosition(index, pos);
	  });
	defDebugRendererObject.def(
	  "UpdateVertexBuffer", +[](DebugRenderer::BaseObject &bo) {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return;
		  o->UpdateVertexBuffer();
	  });
	defDebugRendererObject.def(
	  "SetColor", +[](DebugRenderer::BaseObject &bo, const Vector4 &color) {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return;
		  o->SetColor(color);
	  });
	defDebugRendererObject.def(
	  "GetColor", +[](DebugRenderer::BaseObject &bo, const Vector4 &color) -> std::optional<Vector4> {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return {};
		  return o->GetColor();
	  });
	defDebugRendererObject.def(
	  "SetOutlineColor", +[](DebugRenderer::BaseObject &bo, const Vector4 &color) {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return;
		  o->SetOutlineColor(color);
	  });
	defDebugRendererObject.def(
	  "GetOutlineColor", +[](DebugRenderer::BaseObject &bo, const Vector4 &color) -> std::optional<Vector4> {
		  auto *o = dynamic_cast<DebugRenderer::WorldObject *>(&bo);
		  if(!o)
			  return {};
		  return o->GetOutlineColor();
	  });
	defDebugRendererObject.def("GetPose", static_cast<const umath::ScaledTransform &(::DebugRenderer::BaseObject::*)() const>(&::DebugRenderer::BaseObject::GetPose), luabind::copy_policy<0> {});
	debugMod[defDebugRendererObject];

	auto &modGame = GetLuaInterface().RegisterLibrary("game");
	auto defRenderPassStats = luabind::class_<RenderPassStats>("RenderPassStats");
	defRenderPassStats.def(luabind::constructor<>());
	defRenderPassStats.def(luabind::self + luabind::const_self);
	defRenderPassStats.add_static_constant("COUNTER_SHADER_STATE_CHANGES", umath::to_integral(RenderPassStats::Counter::ShaderStateChanges));
	defRenderPassStats.add_static_constant("COUNTER_MATERIAL_STATE_CHANGES", umath::to_integral(RenderPassStats::Counter::MaterialStateChanges));
	defRenderPassStats.add_static_constant("COUNTER_ENTITY_STATE_CHANGES", umath::to_integral(RenderPassStats::Counter::EntityStateChanges));
	defRenderPassStats.add_static_constant("COUNTER_DRAW_CALLS", umath::to_integral(RenderPassStats::Counter::DrawCalls));
	defRenderPassStats.add_static_constant("COUNTER_DRAWN_MESHES", umath::to_integral(RenderPassStats::Counter::DrawnMeshes));
	defRenderPassStats.add_static_constant("COUNTER_DRAWN_VERTICES", umath::to_integral(RenderPassStats::Counter::DrawnVertices));
	defRenderPassStats.add_static_constant("COUNTER_DRAWN_TRIANGLES", umath::to_integral(RenderPassStats::Counter::DrawnTriangles));
	defRenderPassStats.add_static_constant("COUNTER_ENTITY_BUFFER_UPDATES", umath::to_integral(RenderPassStats::Counter::EntityBufferUpdates));
	defRenderPassStats.add_static_constant("COUNTER_INSTANCE_SETS", umath::to_integral(RenderPassStats::Counter::InstanceSets));
	defRenderPassStats.add_static_constant("COUNTER_INSTANCE_SET_MESHES", umath::to_integral(RenderPassStats::Counter::InstanceSetMeshes));
	defRenderPassStats.add_static_constant("COUNTER_INSTANCED_MESHES", umath::to_integral(RenderPassStats::Counter::InstancedMeshes));
	defRenderPassStats.add_static_constant("COUNTER_INSTANCED_SKIPPED_RENDER_ITEMS", umath::to_integral(RenderPassStats::Counter::InstancedSkippedRenderItems));
	defRenderPassStats.add_static_constant("COUNTER_ENTITIES_WITHOUT_INSTANCING", umath::to_integral(RenderPassStats::Counter::EntitiesWithoutInstancing));
	defRenderPassStats.add_static_constant("COUNTER_COUNT", umath::to_integral(RenderPassStats::Counter::Count));
	static_assert(umath::to_integral(RenderPassStats::Counter::Count) == 13);

	defRenderPassStats.add_static_constant("TIMER_GPU_EXECUTION", umath::to_integral(RenderPassStats::Timer::GpuExecution));
	defRenderPassStats.add_static_constant("TIMER_RENDER_THREAD_WAIT", umath::to_integral(RenderPassStats::Timer::RenderThreadWait));
	defRenderPassStats.add_static_constant("TIMER_CPU_EXECUTION", umath::to_integral(RenderPassStats::Timer::CpuExecution));
	defRenderPassStats.add_static_constant("TIMER_MATERIAL_BIND", umath::to_integral(RenderPassStats::Timer::MaterialBind));
	defRenderPassStats.add_static_constant("TIMER_ENTITY_BIND", umath::to_integral(RenderPassStats::Timer::EntityBind));
	defRenderPassStats.add_static_constant("TIMER_DRAW_CALL", umath::to_integral(RenderPassStats::Timer::DrawCall));
	defRenderPassStats.add_static_constant("TIMER_SHADER_BIND", umath::to_integral(RenderPassStats::Timer::ShaderBind));
	defRenderPassStats.add_static_constant("TIMER_COUNT", umath::to_integral(RenderPassStats::Timer::Count));
	defRenderPassStats.add_static_constant("TIMER_GPU_START", umath::to_integral(RenderPassStats::Timer::GpuStart));
	defRenderPassStats.add_static_constant("TIMER_GPU_END", umath::to_integral(RenderPassStats::Timer::GpuEnd));
	defRenderPassStats.add_static_constant("TIMER_CPU_START", umath::to_integral(RenderPassStats::Timer::CpuStart));
	defRenderPassStats.add_static_constant("TIMER_CPU_END", umath::to_integral(RenderPassStats::Timer::CpuEnd));
	defRenderPassStats.add_static_constant("TIMER_GPU_COUNT", umath::to_integral(RenderPassStats::Timer::GpuCount));
	defRenderPassStats.add_static_constant("TIMER_CPU_COUNT", umath::to_integral(RenderPassStats::Timer::CpuCount));
	static_assert(umath::to_integral(RenderPassStats::Timer::Count) == 7);
	defRenderPassStats.def("Copy", static_cast<RenderPassStats (*)(lua_State *, RenderPassStats &)>([](lua_State *l, RenderPassStats &renderStats) -> RenderPassStats { return renderStats; }));
	defRenderPassStats.def("GetCount", static_cast<uint32_t (*)(lua_State *, RenderPassStats &, RenderPassStats::Counter)>([](lua_State *l, RenderPassStats &renderStats, RenderPassStats::Counter counter) -> uint32_t { return renderStats->GetCount(counter); }));
	defRenderPassStats.def("GetTime",
	  static_cast<long double (*)(lua_State *, RenderPassStats &, RenderPassStats::Timer)>([](lua_State *l, RenderPassStats &renderStats, RenderPassStats::Timer timer) -> long double { return renderStats->GetTime(timer).count() / static_cast<long double>(1'000'000.0); }));
	modGame[defRenderPassStats];

	auto defRenderStats = luabind::class_<RenderStats>("RenderStats");
	defRenderStats.def(luabind::constructor<>());
	defRenderStats.def(luabind::self + luabind::const_self);
	defRenderStats.add_static_constant("RENDER_PASS_LIGHTING_PASS", umath::to_integral(RenderStats::RenderPass::LightingPass));
	defRenderStats.add_static_constant("RENDER_PASS_LIGHTING_PASS_TRANSLUCENT", umath::to_integral(RenderStats::RenderPass::LightingPassTranslucent));
	defRenderStats.add_static_constant("RENDER_PASS_PREPASS", umath::to_integral(RenderStats::RenderPass::Prepass));
	defRenderStats.add_static_constant("RENDER_PASS_SHADOW_PASS", umath::to_integral(RenderStats::RenderPass::ShadowPass));
	defRenderStats.add_static_constant("RENDER_PASS_GLOW_PASS", umath::to_integral(RenderStats::RenderPass::GlowPass));
	defRenderStats.add_static_constant("RENDER_PASS_COUNT", umath::to_integral(RenderStats::RenderPass::Count));
	static_assert(umath::to_integral(RenderStats::RenderPass::Count) == 5);

	defRenderStats.add_static_constant("TIMER_LIGHT_CULLING_GPU", umath::to_integral(RenderStats::RenderStage::LightCullingGpu));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU", umath::to_integral(RenderStats::RenderStage::PostProcessingGpu));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU_FOG", umath::to_integral(RenderStats::RenderStage::PostProcessingGpuFog));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU_DOF", umath::to_integral(RenderStats::RenderStage::PostProcessingGpuDoF));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU_BLOOM", umath::to_integral(RenderStats::RenderStage::PostProcessingGpuBloom));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU_GLOW", umath::to_integral(RenderStats::RenderStage::PostProcessingGpuGlow));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU_TONE_MAPPING", umath::to_integral(RenderStats::RenderStage::PostProcessingGpuToneMapping));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU_FXAA", umath::to_integral(RenderStats::RenderStage::PostProcessingGpuFxaa));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_GPU_SSAO", umath::to_integral(RenderStats::RenderStage::PostProcessingGpuSsao));
	defRenderStats.add_static_constant("TIMER_RENDER_SCENE_GPU", umath::to_integral(RenderStats::RenderStage::RenderSceneGpu));
	defRenderStats.add_static_constant("TIMER_RENDERER_GPU", umath::to_integral(RenderStats::RenderStage::RendererGpu));
	defRenderStats.add_static_constant("TIMER_UPDATE_RENDER_BUFFERS_GPU", umath::to_integral(RenderStats::RenderStage::UpdateRenderBuffersGpu));
	defRenderStats.add_static_constant("TIMER_UPDATE_PREPASS_RENDER_BUFFERS_GPU", umath::to_integral(RenderStats::RenderStage::UpdatePrepassRenderBuffersGpu));
	defRenderStats.add_static_constant("TIMER_RENDER_SHADOWS_GPU", umath::to_integral(RenderStats::RenderStage::RenderShadowsGpu));
	defRenderStats.add_static_constant("TIMER_RENDER_PARTICLES_GPU", umath::to_integral(RenderStats::RenderStage::RenderParticlesGpu));

	defRenderStats.add_static_constant("TIMER_LIGHT_CULLING_CPU", umath::to_integral(RenderStats::RenderStage::LightCullingCpu));
	defRenderStats.add_static_constant("TIMER_PREPASS_EXECUTION_CPU", umath::to_integral(RenderStats::RenderStage::PrepassExecutionCpu));
	defRenderStats.add_static_constant("TIMER_LIGHTING_PASS_EXECUTION_CPU", umath::to_integral(RenderStats::RenderStage::LightingPassExecutionCpu));
	defRenderStats.add_static_constant("TIMER_POST_PROCESSING_EXECUTION_CPU", umath::to_integral(RenderStats::RenderStage::PostProcessingExecutionCpu));
	defRenderStats.add_static_constant("TIMER_UPDATE_RENDER_BUFFERS_CPU", umath::to_integral(RenderStats::RenderStage::UpdateRenderBuffersCpu));
	defRenderStats.add_static_constant("TIMER_RENDER_SCENE_CPU", umath::to_integral(RenderStats::RenderStage::RenderSceneCpu));
	defRenderStats.add_static_constant("TIMER_COUNT", umath::to_integral(RenderStats::RenderStage::Count));
	static_assert(umath::to_integral(RenderStats::RenderStage::Count) == 21);
	defRenderStats.def("Copy", static_cast<RenderStats (*)(lua_State *, RenderStats &)>([](lua_State *l, RenderStats &renderStats) -> RenderStats { return renderStats; }));
	defRenderStats.def("GetPassStats", static_cast<RenderPassStats *(*)(lua_State *, RenderStats &, RenderStats::RenderPass)>([](lua_State *l, RenderStats &renderStats, RenderStats::RenderPass pass) -> RenderPassStats * { return &renderStats.GetPassStats(pass); }));
	defRenderStats.def("GetTime",
	  static_cast<long double (*)(lua_State *, RenderStats &, RenderStats::RenderStage)>([](lua_State *l, RenderStats &renderStats, RenderStats::RenderStage timer) -> long double { return renderStats->GetTime(timer).count() / static_cast<long double>(1'000'000.0); }));
	modGame[defRenderStats];

	auto defDrawSceneInfo = luabind::class_<::util::DrawSceneInfo>("DrawSceneInfo");
	defDrawSceneInfo.add_static_constant("FLAG_FLIP_VERTICALLY_BIT", umath::to_integral(::util::DrawSceneInfo::Flags::FlipVertically));
	defDrawSceneInfo.add_static_constant("FLAG_DISABLE_RENDER_BIT", umath::to_integral(::util::DrawSceneInfo::Flags::DisableRender));
	defDrawSceneInfo.add_static_constant("FLAG_REFLECTION_BIT", umath::to_integral(::util::DrawSceneInfo::Flags::Reflection));
	defDrawSceneInfo.add_static_constant("FLAG_DISABLE_PREPASS_BIT", umath::to_integral(::util::DrawSceneInfo::Flags::DisablePrepass));
	defDrawSceneInfo.add_static_constant("FLAG_DISABLE_LIGHTING_PASS_BIT", umath::to_integral(::util::DrawSceneInfo::Flags::DisableLightingPass));
	defDrawSceneInfo.def(luabind::constructor<>());
	defDrawSceneInfo.property("scene", static_cast<luabind::object (*)(const ::util::DrawSceneInfo &)>([](const ::util::DrawSceneInfo &drawSceneInfo) -> luabind::object { return drawSceneInfo.scene.valid() ? drawSceneInfo.scene->GetLuaObject() : luabind::object {}; }),
	  static_cast<void (*)(lua_State *, ::util::DrawSceneInfo &, luabind::object)>([](lua_State *l, ::util::DrawSceneInfo &drawSceneInfo, luabind::object o) {
		  if(Lua::IsSet(l, 2) == false) {
			  drawSceneInfo.scene = decltype(drawSceneInfo.scene) {};
			  return;
		  }
		  auto &scene = Lua::Check<pragma::CSceneComponent>(l, 2);
		  drawSceneInfo.scene = scene.GetHandle<pragma::CSceneComponent>();
	  }));
	defDrawSceneInfo.property("toneMapping", static_cast<luabind::object (*)(lua_State *, ::util::DrawSceneInfo &)>([](lua_State *l, ::util::DrawSceneInfo &drawSceneInfo) -> luabind::object {
		return drawSceneInfo.toneMapping.has_value() ? luabind::object {l, umath::to_integral(*drawSceneInfo.toneMapping)} : luabind::object {};
	}),
	  static_cast<void (*)(lua_State *, ::util::DrawSceneInfo &, luabind::object)>([](lua_State *l, ::util::DrawSceneInfo &drawSceneInfo, luabind::object o) {
		  if(Lua::IsSet(l, 2) == false) {
			  drawSceneInfo.toneMapping = {};
			  return;
		  }
		  drawSceneInfo.toneMapping = static_cast<pragma::rendering::ToneMapping>(Lua::CheckInt(l, 2));
	  }));
	defDrawSceneInfo.property("commandBuffer", static_cast<std::shared_ptr<Lua::Vulkan::CommandBuffer> (*)(::util::DrawSceneInfo &)>([](::util::DrawSceneInfo &drawSceneInfo) -> std::shared_ptr<Lua::Vulkan::CommandBuffer> { return drawSceneInfo.commandBuffer; }),
	  static_cast<void (*)(::util::DrawSceneInfo &, Lua::Vulkan::CommandBuffer &)>([](::util::DrawSceneInfo &drawSceneInfo, Lua::Vulkan::CommandBuffer &commandBuffer) {
		  if(commandBuffer.IsPrimary() == false)
			  return;
		  drawSceneInfo.commandBuffer = std::dynamic_pointer_cast<prosper::IPrimaryCommandBuffer>(commandBuffer.shared_from_this());
	  }));
	static_assert(sizeof(::util::DrawSceneInfo::flags) == sizeof(uint8_t));
	defDrawSceneInfo.def_readwrite("flags", reinterpret_cast<uint8_t util::DrawSceneInfo::*>(&::util::DrawSceneInfo::flags));
	static_assert(sizeof(::util::DrawSceneInfo::renderFlags) == sizeof(uint32_t));
	defDrawSceneInfo.def_readwrite("renderFlags", reinterpret_cast<uint32_t util::DrawSceneInfo::*>(&::util::DrawSceneInfo::renderFlags));
	defDrawSceneInfo.def_readwrite("renderTarget", &::util::DrawSceneInfo::renderTarget);
	defDrawSceneInfo.def_readwrite("exclusionMask", &::util::DrawSceneInfo::exclusionMask);
	defDrawSceneInfo.def_readwrite("inclusionMask", &::util::DrawSceneInfo::inclusionMask);
	defDrawSceneInfo.def_readwrite("outputImage", &::util::DrawSceneInfo::outputImage);
	static_assert(sizeof(::util::DrawSceneInfo::outputLayerId) == sizeof(uint32_t));
	defDrawSceneInfo.def_readwrite("outputLayerId", reinterpret_cast<uint32_t util::DrawSceneInfo::*>(&::util::DrawSceneInfo::outputLayerId));
	defDrawSceneInfo.property("clearColor", static_cast<Color (*)(::util::DrawSceneInfo &)>([](::util::DrawSceneInfo &drawSceneInfo) -> Color { return *drawSceneInfo.clearColor; }),
	  static_cast<void (*)(lua_State *, ::util::DrawSceneInfo &, const luabind::object &)>([](lua_State *l, ::util::DrawSceneInfo &drawSceneInfo, const luabind::object &color) {
		  if(luabind::type(color) == LUA_TNIL)
			  drawSceneInfo.clearColor = {};
		  else
			  drawSceneInfo.clearColor = Lua::Check<Color>(l, 2);
	  }));
	defDrawSceneInfo.property("clipPlane", static_cast<Vector4 (*)(::util::DrawSceneInfo &)>([](::util::DrawSceneInfo &drawSceneInfo) -> Vector4 { return *drawSceneInfo.clipPlane; }),
	  static_cast<void (*)(lua_State *, ::util::DrawSceneInfo &, const luabind::object &)>([](lua_State *l, ::util::DrawSceneInfo &drawSceneInfo, const luabind::object &clipPlane) {
		  if(luabind::type(clipPlane) == LUA_TNIL)
			  drawSceneInfo.clipPlane = {};
		  else
			  drawSceneInfo.clipPlane = Lua::Check<Vector4>(l, 2);
	  }));
	defDrawSceneInfo.property("pvsOrigin", static_cast<Vector3 (*)(::util::DrawSceneInfo &)>([](::util::DrawSceneInfo &drawSceneInfo) -> Vector3 { return *drawSceneInfo.pvsOrigin; }),
	  static_cast<void (*)(lua_State *, ::util::DrawSceneInfo &, const luabind::object &)>([](lua_State *l, ::util::DrawSceneInfo &drawSceneInfo, const luabind::object &pvsOrigin) {
		  if(luabind::type(pvsOrigin) == LUA_TNIL)
			  drawSceneInfo.pvsOrigin = {};
		  else
			  drawSceneInfo.pvsOrigin = Lua::Check<Vector3>(l, 2);
	  }));
	defDrawSceneInfo.property("renderStats", static_cast<luabind::object (*)(lua_State *, ::util::DrawSceneInfo &)>([](lua_State *l, ::util::DrawSceneInfo &drawSceneInfo) -> luabind::object {
		if(drawSceneInfo.renderStats == nullptr)
			return {};
		return luabind::object {l, drawSceneInfo.renderStats.get()};
	}));
	defDrawSceneInfo.def("AddSubPass", &::util::DrawSceneInfo::AddSubPass);
	defDrawSceneInfo.def("SetEntityRenderFilter", static_cast<void (*)(lua_State *, util::DrawSceneInfo &, luabind::object)>([](lua_State *l, util::DrawSceneInfo &drawSceneInfo, luabind::object f) {
		Lua::CheckFunction(l, 2);
		drawSceneInfo.renderFilter = [f, l](CBaseEntity &ent) -> bool {
			auto r = Lua::CallFunction(
			  l,
			  [&f, &ent](lua_State *l) {
				  f.push(l);
				  ent.GetLuaObject().push(l);
				  return Lua::StatusCode::Ok;
			  },
			  1);
			if(r == Lua::StatusCode::Ok) {
				if(Lua::IsSet(l, -1) == false)
					return false;
				return Lua::CheckBool(l, -1);
			}
			return true;
		};
	}));
	defDrawSceneInfo.def("SetEntityPrepassFilter", static_cast<void (*)(lua_State *, util::DrawSceneInfo &, luabind::object)>([](lua_State *l, util::DrawSceneInfo &drawSceneInfo, luabind::object f) {
		Lua::CheckFunction(l, 2);
		drawSceneInfo.prepassFilter = [f, l](CBaseEntity &ent) -> bool {
			auto r = Lua::CallFunction(
			  l,
			  [&f, &ent](lua_State *l) {
				  f.push(l);
				  ent.GetLuaObject().push(l);
				  return Lua::StatusCode::Ok;
			  },
			  1);
			if(r == Lua::StatusCode::Ok) {
				if(Lua::IsSet(l, -1) == false)
					return false;
				return Lua::CheckBool(l, -1);
			}
			return true;
		};
	}));
	modGame[defDrawSceneInfo];

	Lua::RegisterLibraryEnums(GetLuaState(), "game",
	  {{"RENDER_MASK_NONE", umath::to_integral(pragma::rendering::RenderMask::None)},

	    {"SCENE_RENDER_PASS_NONE", umath::to_integral(pragma::rendering::SceneRenderPass::None)}, {"SCENE_RENDER_PASS_WORLD", umath::to_integral(pragma::rendering::SceneRenderPass::World)}, {"SCENE_RENDER_PASS_VIEW", umath::to_integral(pragma::rendering::SceneRenderPass::View)},
	    {"SCENE_RENDER_PASS_SKY", umath::to_integral(pragma::rendering::SceneRenderPass::Sky)}, {"SCENE_RENDER_PASS_COUNT", umath::to_integral(pragma::rendering::SceneRenderPass::Count)}});

	auto defRenderQueue = luabind::class_<pragma::rendering::RenderQueue>("RenderQueue");
	defRenderQueue.def("WaitForCompletion", static_cast<void (*)(lua_State *, const pragma::rendering::RenderQueue &)>([](lua_State *l, const pragma::rendering::RenderQueue &renderQueue) { renderQueue.WaitForCompletion(); }));
	defRenderQueue.def("IsComplete", static_cast<bool (*)(lua_State *, const pragma::rendering::RenderQueue &)>([](lua_State *l, const pragma::rendering::RenderQueue &renderQueue) -> bool { return renderQueue.IsComplete(); }));
	modGame[defRenderQueue];

	auto defBaseRenderProcessor = luabind::class_<pragma::rendering::BaseRenderProcessor>("BaseRenderProcessor");
	defBaseRenderProcessor.def("SetDepthBias", static_cast<void (*)(lua_State *, pragma::rendering::BaseRenderProcessor &, float, float)>([](lua_State *l, pragma::rendering::BaseRenderProcessor &processor, float d, float delta) { processor.SetDepthBias(d, delta); }));
	modGame[defBaseRenderProcessor];

	auto defDepthStageRenderProcessor = luabind::class_<pragma::rendering::DepthStageRenderProcessor, luabind::bases<pragma::rendering::BaseRenderProcessor>>("DepthStageRenderProcessor");
	defDepthStageRenderProcessor.def("Render",
	  static_cast<void (*)(lua_State *, pragma::rendering::DepthStageRenderProcessor &, const pragma::rendering::RenderQueue &)>(
	    [](lua_State *l, pragma::rendering::DepthStageRenderProcessor &processor, const pragma::rendering::RenderQueue &renderQueue) { processor.Render(renderQueue, pragma::rendering::RenderPass::Prepass); }));
	modGame[defDepthStageRenderProcessor];

	auto defLightingStageRenderProcessor = luabind::class_<pragma::rendering::LightingStageRenderProcessor, luabind::bases<pragma::rendering::BaseRenderProcessor>>("LightingStageRenderProcessor");
	defLightingStageRenderProcessor.def("Render",
	  static_cast<void (*)(lua_State *, pragma::rendering::LightingStageRenderProcessor &, const pragma::rendering::RenderQueue &)>(
	    [](lua_State *l, pragma::rendering::LightingStageRenderProcessor &processor, const pragma::rendering::RenderQueue &renderQueue) { processor.Render(renderQueue); }));
	modGame[defLightingStageRenderProcessor];

	auto modelMeshClassDef = luabind::class_<ModelMesh>("Mesh");
	Lua::ModelMesh::register_class(modelMeshClassDef);
	modelMeshClassDef.scope[luabind::def("Create", &Lua::ModelMesh::Client::Create)];

	auto subModelMeshClassDef = luabind::class_<ModelSubMesh>("Sub");
	Lua::ModelSubMesh::register_class(subModelMeshClassDef);
	subModelMeshClassDef.def("GetTangents", &Lua::ModelSubMesh::Client::GetTangents);
	subModelMeshClassDef.def("GetBiTangents", &Lua::ModelSubMesh::Client::GetBiTangents);
	subModelMeshClassDef.def("GetVertexBuffer", &Lua::ModelSubMesh::Client::GetVertexBuffer);
	subModelMeshClassDef.def("GetVertexWeightBuffer", &Lua::ModelSubMesh::Client::GetVertexWeightBuffer);
	subModelMeshClassDef.def("GetAlphaBuffer", &Lua::ModelSubMesh::Client::GetAlphaBuffer);
	subModelMeshClassDef.def("GetIndexBuffer", &Lua::ModelSubMesh::Client::GetIndexBuffer);
	subModelMeshClassDef.def("GetSceneMesh", &Lua::ModelSubMesh::Client::GetVkMesh);
	subModelMeshClassDef.def("GetExtensionData", &::ModelSubMesh::GetExtensionData);
	subModelMeshClassDef.scope[luabind::def("create", &Lua::ModelSubMesh::Client::Create)];

	auto modelClassDef = luabind::class_<Model>("Model");

	auto defMdlExportInfo = luabind::class_<pragma::asset::ModelExportInfo>("ExportInfo");
	defMdlExportInfo.add_static_constant("IMAGE_FORMAT_PNG", umath::to_integral(pragma::asset::ModelExportInfo::ImageFormat::PNG));
	defMdlExportInfo.add_static_constant("IMAGE_FORMAT_BMP", umath::to_integral(pragma::asset::ModelExportInfo::ImageFormat::BMP));
	defMdlExportInfo.add_static_constant("IMAGE_FORMAT_TGA", umath::to_integral(pragma::asset::ModelExportInfo::ImageFormat::TGA));
	defMdlExportInfo.add_static_constant("IMAGE_FORMAT_JPG", umath::to_integral(pragma::asset::ModelExportInfo::ImageFormat::JPG));
	defMdlExportInfo.add_static_constant("IMAGE_FORMAT_HDR", umath::to_integral(pragma::asset::ModelExportInfo::ImageFormat::HDR));
	defMdlExportInfo.add_static_constant("IMAGE_FORMAT_DDS", umath::to_integral(pragma::asset::ModelExportInfo::ImageFormat::DDS));
	defMdlExportInfo.add_static_constant("IMAGE_FORMAT_KTX", umath::to_integral(pragma::asset::ModelExportInfo::ImageFormat::KTX));

	defMdlExportInfo.add_static_constant("DEVICE_CPU", umath::to_integral(pragma::rendering::cycles::SceneInfo::DeviceType::CPU));
	defMdlExportInfo.add_static_constant("DEVICE_GPU", umath::to_integral(pragma::rendering::cycles::SceneInfo::DeviceType::GPU));
	defMdlExportInfo.def(luabind::constructor<>());
	defMdlExportInfo.def_readwrite("exportAnimations", &pragma::asset::ModelExportInfo::exportAnimations);
	defMdlExportInfo.def_readwrite("exportSkinnedMeshData", &pragma::asset::ModelExportInfo::exportSkinnedMeshData);
	defMdlExportInfo.def_readwrite("exportMorphTargets", &pragma::asset::ModelExportInfo::exportMorphTargets);
	defMdlExportInfo.def_readwrite("exportImages", &pragma::asset::ModelExportInfo::exportImages);
	defMdlExportInfo.def_readwrite("embedAnimations", &pragma::asset::ModelExportInfo::embedAnimations);
	defMdlExportInfo.def_readwrite("fullExport", &pragma::asset::ModelExportInfo::fullExport);
	defMdlExportInfo.def_readwrite("normalizeTextureNames", &pragma::asset::ModelExportInfo::normalizeTextureNames);
	defMdlExportInfo.def_readwrite("enableExtendedDDS", &pragma::asset::ModelExportInfo::enableExtendedDDS);
	defMdlExportInfo.def_readwrite("saveAsBinary", &pragma::asset::ModelExportInfo::saveAsBinary);
	defMdlExportInfo.def_readwrite("verbose", &pragma::asset::ModelExportInfo::verbose);
	defMdlExportInfo.def_readwrite("generateAo", &pragma::asset::ModelExportInfo::generateAo);
	defMdlExportInfo.def_readwrite("aoSamples", &pragma::asset::ModelExportInfo::aoSamples);
	defMdlExportInfo.def_readwrite("aoResolution", &pragma::asset::ModelExportInfo::aoResolution);
	defMdlExportInfo.def_readwrite("scale", &pragma::asset::ModelExportInfo::scale);
	defMdlExportInfo.def_readwrite("mergeMeshesByMaterial", &pragma::asset::ModelExportInfo::mergeMeshesByMaterial);
	defMdlExportInfo.def_readwrite("imageFormat", reinterpret_cast<std::underlying_type_t<decltype(pragma::asset::ModelExportInfo::imageFormat)> pragma::asset::ModelExportInfo::*>(&pragma::asset::ModelExportInfo::imageFormat));
	defMdlExportInfo.def_readwrite("aoDevice", reinterpret_cast<std::underlying_type_t<decltype(pragma::asset::ModelExportInfo::aoDevice)> pragma::asset::ModelExportInfo::*>(&pragma::asset::ModelExportInfo::aoDevice));
	defMdlExportInfo.def("SetAnimationList", static_cast<void (*)(lua_State *, pragma::asset::ModelExportInfo &, luabind::object)>([](lua_State *l, pragma::asset::ModelExportInfo &exportInfo, luabind::object oTable) {
		int32_t t = 2;
		auto n = Lua::GetObjectLength(l, t);
		std::vector<std::string> anims {};
		anims.reserve(n);
		for(auto i = decltype(n) {0u}; i < n; ++i) {
			Lua::PushInt(l, i + 1);
			Lua::GetTableValue(l, t);
			anims.push_back(Lua::CheckString(l, -1));

			Lua::Pop(l, 1);
		}
		exportInfo.SetAnimationList(anims);
	}));
	modelClassDef.scope[defMdlExportInfo];

	Lua::Model::register_class(GetLuaState(), modelClassDef, modelMeshClassDef, subModelMeshClassDef);
	modelClassDef.scope[luabind::def(
	  "create_quad", +[](Game &game, const pragma::model::QuadCreateInfo &createInfo) -> std::shared_ptr<::Model> {
		  auto mesh = pragma::model::create_quad(game, createInfo);
		  return Lua::Model::Client::create_generic_model(game, *mesh);
	  })];
	modelClassDef.scope[luabind::def(
	  "create_sphere", +[](Game &game, const pragma::model::SphereCreateInfo &createInfo) -> std::shared_ptr<::Model> {
		  auto mesh = pragma::model::create_sphere(game, createInfo);
		  return Lua::Model::Client::create_generic_model(game, *mesh);
	  })];
	modelClassDef.scope[luabind::def(
	  "create_cylinder", +[](Game &game, const pragma::model::CylinderCreateInfo &createInfo) -> std::shared_ptr<::Model> {
		  auto mesh = pragma::model::create_cylinder(game, createInfo);
		  return Lua::Model::Client::create_generic_model(game, *mesh);
	  })];
	modelClassDef.scope[luabind::def(
	  "create_cone", +[](Game &game, const pragma::model::ConeCreateInfo &createInfo) -> std::shared_ptr<::Model> {
		  auto mesh = pragma::model::create_cone(game, createInfo);
		  return Lua::Model::Client::create_generic_model(game, *mesh);
	  })];
	modelClassDef.scope[luabind::def(
	  "create_circle", +[](Game &game, const pragma::model::CircleCreateInfo &createInfo) -> std::shared_ptr<::Model> {
		  auto mesh = pragma::model::create_circle(game, createInfo);
		  return Lua::Model::Client::create_generic_model(game, *mesh);
	  })];
	modelClassDef.scope[luabind::def(
	  "create_ring", +[](Game &game, const pragma::model::RingCreateInfo &createInfo) -> std::shared_ptr<::Model> {
		  auto mesh = pragma::model::create_ring(game, createInfo);
		  return Lua::Model::Client::create_generic_model(game, *mesh);
	  })];
	modelClassDef.def("AddMaterial", &Lua::Model::Client::AddMaterial);
	modelClassDef.def("SetMaterial", &Lua::Model::Client::SetMaterial);
	modelClassDef.def("GetVertexAnimationBuffer", &Lua::Model::Client::GetVertexAnimationBuffer);
	modelClassDef.def("Export", &Lua::Model::Client::Export);
	modelClassDef.def("ExportAnimation", &Lua::Model::Client::ExportAnimation);
	modGame[modelClassDef];
	auto _G = luabind::globals(GetLuaState());
	_G["Model"] = _G["game"]["Model"];
	_G["Animation"] = _G["game"]["Model"]["Animation"];

	// COMPONENT TODO
	/*auto vhcWheelClassDef = luabind::class_<VHCWheelHandle COMMA EntityHandle>("Wheel")
	LUA_CLASS_VHCWHEEL_SHARED;
	vehicleClassDef.scope[vhcWheelClassDef];
	modGame[vehicleClassDef];*/

	// Custom Classes
	// COMPONENT TODO
	/*auto classDefBase = luabind::class_<CLuaEntityHandle COMMA CLuaEntityWrapper COMMA luabind::bases<EntityHandle>>("BaseEntity");
	LUA_CUSTOM_CLASS_ENTITY_SHARED(classDefBase,CLuaEntityWrapper);
	classDefBase.def("ReceiveData",&CLuaEntityWrapper::ReceiveData,static_cast<void(*)(lua_State*,EntityHandle&,NetPacket&)>(&CLuaBaseEntityWrapper::default_ReceiveData));
	classDefBase.def("OnRender",&CLuaEntityWrapper::OnRender,&CLuaBaseEntityWrapper::default_OnRender);
	classDefBase.def("OnPostRender",&CLuaEntityWrapper::OnPostRender,&CLuaBaseEntityWrapper::default_OnPostRender);
	classDefBase.def("ReceiveNetEvent",&CLuaEntityWrapper::ReceiveNetEvent,&CLuaBaseEntityWrapper::default_ReceiveNetEvent);
	modGame[classDefBase];*/

	/*auto classDefWeapon = luabind::class_<CLuaWeaponHandle COMMA CLuaWeaponWrapper COMMA luabind::bases<CLuaEntityHandle COMMA WeaponHandle>>("BaseWeapon");
	LUA_CUSTOM_CLASS_ENTITY_SHARED(classDefWeapon,CLuaWeaponWrapper);
	LUA_CUSTOM_CLASS_WEAPON_SHARED(classDefWeapon,CLuaWeaponWrapper);
	classDefWeapon.def("HandleViewModelAnimationEvent",&CLuaWeaponWrapper::HandleViewModelAnimationEvent,&CLuaWeaponWrapper::default_HandleViewModelAnimationEvent);
	classDefWeapon.def("ReceiveData",&CLuaWeaponWrapper::ReceiveData,&CLuaBaseEntityWrapper::default_ReceiveData);
	classDefWeapon.def("OnRender",&CLuaWeaponWrapper::OnRender,&CLuaBaseEntityWrapper::default_OnRender);
	classDefWeapon.def("OnPostRender",&CLuaWeaponWrapper::OnPostRender,&CLuaBaseEntityWrapper::default_OnPostRender);
	classDefWeapon.def("ReceiveNetEvent",&CLuaWeaponWrapper::ReceiveNetEvent,&CLuaBaseEntityWrapper::default_ReceiveNetEvent);
	modGame[classDefWeapon];*/

	// COMPONENT TODO
	/*auto classDefBaseVehicle = luabind::class_<CLuaVehicleHandle COMMA CLuaVehicleWrapper COMMA luabind::bases<CLuaEntityHandle COMMA VehicleHandle>>("BaseVehicle");
	LUA_CUSTOM_CLASS_ENTITY_SHARED(classDefBaseVehicle,CLuaVehicleWrapper);
	LUA_CUSTOM_CLASS_VEHICLE_SHARED(classDefBaseVehicle,CLuaVehicleWrapper);
	classDefBaseVehicle.def("ReceiveData",&CLuaVehicleWrapper::ReceiveData,&CLuaBaseEntityWrapper::default_ReceiveData);
	classDefBaseVehicle.def("OnRender",&CLuaVehicleWrapper::OnRender,&CLuaBaseEntityWrapper::default_OnRender);
	classDefBaseVehicle.def("OnPostRender",&CLuaVehicleWrapper::OnPostRender,&CLuaBaseEntityWrapper::default_OnPostRender);
	classDefBaseVehicle.def("ReceiveNetEvent",&CLuaVehicleWrapper::ReceiveNetEvent,&CLuaBaseEntityWrapper::default_ReceiveNetEvent);
	modGame[classDefBaseVehicle];

	auto classDefBaseNPC = luabind::class_<CLuaNPCHandle COMMA CLuaNPCWrapper COMMA luabind::bases<CLuaEntityHandle COMMA NPCHandle>>("BaseNPC");
	LUA_CUSTOM_CLASS_ENTITY_SHARED(classDefBaseNPC,CLuaNPCWrapper);
	LUA_CUSTOM_CLASS_NPC_SHARED(classDefBaseNPC,CLuaNPCWrapper);
	classDefBaseNPC.def("ReceiveData",&CLuaNPCWrapper::ReceiveData,&CLuaBaseEntityWrapper::default_ReceiveData);
	classDefBaseNPC.def("OnRender",&CLuaNPCWrapper::OnRender,&CLuaBaseEntityWrapper::default_OnRender);
	classDefBaseNPC.def("OnPostRender",&CLuaNPCWrapper::OnPostRender,&CLuaBaseEntityWrapper::default_OnPostRender);
	classDefBaseNPC.def("ReceiveNetEvent",&CLuaNPCWrapper::ReceiveNetEvent,&CLuaBaseEntityWrapper::default_ReceiveNetEvent);
	classDefBaseNPC.def("PlayFootStepSound",&CLuaNPCWrapper::PlayFootStepSound,&CLuaNPCWrapper::default_PlayFootStepSound);
	classDefBaseNPC.def("CalcMovementSpeed",&CLuaNPCWrapper::CalcMovementSpeed,&CLuaNPCWrapper::default_CalcMovementSpeed);
	modGame[classDefBaseNPC];*/
	//

	// COMPONENT TODO
	/*auto defListener = luabind::class_<ListenerHandle COMMA EntityHandle>("Listener");
	defListener.def("GetGain",&Lua_Listener_GetGain);
	defListener.def("SetGain",&Lua_Listener_SetGain);
	modGame[defListener];
	
	auto defEnvParticleSystem = luabind::class_<EnvParticleSystemHandle COMMA EntityHandle>("EnvParticleSystem");
	modGame[defEnvParticleSystem];
	
	auto envLightClassDef = luabind::class_<EnvLightHandle COMMA EntityHandle>("EnvLight");
	Lua::EnvLight::register_class(envLightClassDef);
	modGame[envLightClassDef];

	auto envLightSpotClassDef = luabind::class_<EnvLightSpotHandle COMMA EnvLightHandle COMMA EntityHandle>("EnvLightSpot");
	Lua::EnvLightSpot::register_class(envLightSpotClassDef);
	modGame[envLightSpotClassDef];

	auto envLightPointClassDef = luabind::class_<EnvLightPointHandle COMMA EnvLightHandle COMMA EntityHandle>("EnvLightPoint");
	Lua::EnvLightPoint::register_class(envLightPointClassDef);
	modGame[envLightPointClassDef];

	auto envLightDirectionalClassDef = luabind::class_<EnvLightDirectionalHandle COMMA EnvLightHandle COMMA EntityHandle>("EnvLightDirectional");
	Lua::EnvLightDirectional::register_class(envLightDirectionalClassDef);
	modGame[envLightDirectionalClassDef];

	auto funcWaterClassDef = luabind::class_<FuncWaterHandle COMMA EntityHandle>("FuncWater");
	Lua::FuncWater::register_class(funcWaterClassDef);
	modGame[funcWaterClassDef];

	auto defRenderTarget = luabind::class_<PointRenderTargetHandle COMMA EntityHandle>("RenderTarget");
	defRenderTarget.def("GetTextureBuffer",&Lua_PointRenderTarget_GetTextureBuffer);
	defRenderTarget.def("SetRenderSize",&Lua_PointRenderTarget_SetRenderSize);
	defRenderTarget.def("GetRenderSize",&Lua_PointRenderTarget_GetRenderSize);
	defRenderTarget.def("SetRenderMaterial",static_cast<void(*)(lua_State*,PointRenderTargetHandle&,Material*)) &Lua_PointRenderTarget_SetRenderMaterial);
	defRenderTarget.def("SetRenderMaterial",static_cast<void(*)(lua_State*,PointRenderTargetHandle&,std::string)) &Lua_PointRenderTarget_SetRenderMaterial);
	defRenderTarget.def("SetRenderMaterial",static_cast<void(*)(lua_State*,PointRenderTargetHandle&)) &Lua_PointRenderTarget_SetRenderMaterial);
	defRenderTarget.def("GetRenderMaterial",&Lua_PointRenderTarget_GetRenderMaterial);
	defRenderTarget.def("SetRefreshRate",&Lua_PointRenderTarget_SetRefreshRate);
	defRenderTarget.def("GetRefreshRate",&Lua_PointRenderTarget_GetRefreshRate);
	defRenderTarget.def("SetRenderDepth",&Lua_PointRenderTarget_SetRenderDepth);
	defRenderTarget.def("GetRenderDepth",&Lua_PointRenderTarget_GetRenderDepth);
	defRenderTarget.def("SetRenderFOV",&Lua_PointRenderTarget_SetRenderFOV);
	defRenderTarget.def("GetRenderFOV",&Lua_PointRenderTarget_GetRenderFOV);
	modGame[defRenderTarget];*/

	_G["Entity"] = _G["ents"]["Entity"];
	_G["BaseEntity"] = _G["ents"]["BaseEntity"];

	auto worldEnvClassDef = luabind::class_<::WorldEnvironment>("WorldEnvironment");
	Lua::WorldEnvironment::register_class(worldEnvClassDef);
	modGame[worldEnvClassDef];
}
