// SPDX-FileCopyrightText: (c) 2024 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PRAGMA_SHADER_GRAPH_NODES_FOG_HPP__
#define __PRAGMA_SHADER_GRAPH_NODES_FOG_HPP__

#include "pragma/clientdefinitions.h"

import pragma.shadergraph;

namespace pragma::rendering::shader_graph {
	class DLLCLIENT FogNode : public pragma::shadergraph::Node {
	  public:
		static constexpr const char *OUT_COLOR = "color";
		static constexpr const char *OUT_START_DISTANCE = "startDistance";
		static constexpr const char *OUT_END_DISTANCE = "endDistance";
		static constexpr const char *OUT_DENSITY = "density";

		FogNode(const std::string_view &type);

		virtual std::string DoEvaluate(const pragma::shadergraph::Graph &graph, const pragma::shadergraph::GraphNode &instance) const override;
	};
};

#endif
