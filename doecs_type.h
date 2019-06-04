#pragma once
#ifndef __doecs_type_header__
#define __doecs_type_header__
#include <stdint.h>

namespace de
{
	using EntityId = uint64_t;
	constexpr EntityId INVALID_ENTITY_ID = -1;
}

namespace de2
{
	using EntityId = uint64_t;
	constexpr EntityId INVALID_ENTITY_ID = -1;
}

#endif // __doecs_type_header__
