#pragma once
#include "../doecs.h"
#include "Components.h"

#define PlayerComponents FPositionComponent, FRotationComponent, FLifeformComponent
DeclareEntityArchetypePool(FPlayerPool, PlayerComponents);