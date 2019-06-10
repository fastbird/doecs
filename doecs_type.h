#pragma once
#ifndef __doecs_type_header__
#define __doecs_type_header__
#include <stdint.h>
#include <unordered_map>
namespace de
{
	using EntityId = uint64_t;
	constexpr EntityId INVALID_ENTITY_ID = -1;
}

namespace de2
{
	using EntityId = uint64_t;
	constexpr EntityId INVALID_ENTITY_ID = -1;

	using ComponentIndex = uint32_t;
	using ElementCount = uint32_t;
	// component index, <count, comopnents> 
	using ComponentsArg = std::vector<void*>;
}

#endif // __doecs_type_header__
