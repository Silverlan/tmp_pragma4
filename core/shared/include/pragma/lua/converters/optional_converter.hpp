// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __LUA_OPTIONAL_CONVERTER_HPP__
#define __LUA_OPTIONAL_CONVERTER_HPP__

#include <luabind/detail/conversion_policies/native_converter.hpp>
#include <optional>

namespace luabind {
	template<typename T>
	struct default_converter<std::optional<T>> : native_converter_base<std::optional<T>> {
		enum { consumed_args = 1 };

		template<typename U>
		std::optional<T> to_cpp(lua_State *L, U u, int index);

		template<class U>
		int match(lua_State *l, U u, int index);

		template<class U>
		void converter_postcall(lua_State *, U u, int)
		{
		}

		void to_lua(lua_State *L, std::optional<T> const &x);
		void to_lua(lua_State *L, std::optional<T> *x);
	  private:
		default_converter<T> m_converter;
	};

	template<typename T>
	struct default_converter<const std::optional<T>> : default_converter<std::optional<T>> {};

	template<typename T>
	struct default_converter<std::optional<T> const &> : default_converter<std::optional<T>> {};

	template<typename T>
	struct default_converter<std::optional<T> &&> : default_converter<std::optional<T>> {};
}

#endif
