#pragma once
#include <iostream>
#include "../doecs.h"
#include "Components.h"

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
