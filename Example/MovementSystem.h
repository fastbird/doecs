#pragma once
#include <iostream>
#include "../doecs.h"
#include "Components.h"

struct MovementSystem : public de::System<FPositionComponent>
{
	void Execute(FPositionComponent& pos)
	{
		// move it
		std::cout << "move " << &pos << std::endl;
	}
};
