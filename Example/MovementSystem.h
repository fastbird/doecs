#pragma once
#include <iostream>
#include "../doecs.h"
#include "Components.h"
#include <functional>

struct MovementSystem : public de::System<FPositionComponent>
{
	void Execute(uint32_t count, FPositionComponent* positions)
	{
		for (uint32_t i = 0; i < count; ++i) {
			// move it
			std::cout << "move " << &positions[i] << std::endl;
		}
	}
};

class MovementSystem2 : public de2::ISystem
{	
public:
	uint32_t GetComponentHashes(const uint64_t*& pHashes) override
	{
		static const uint64_t ComponentHashes[] = { typeid(FPositionComponent).hash_code() };
		pHashes = ComponentHashes;
		return  (uint32_t)de2::ArrayCount(ComponentHashes);
	}

	void Execute(uint32_t elementCount, const de2::ComponentsArg& components) override
	{
		FPositionComponent* posComps = (FPositionComponent*)components[0];

		for (uint32_t i = 0; i < elementCount; ++i)
		{
			posComps[i].y += 1;
		}
	}
};
