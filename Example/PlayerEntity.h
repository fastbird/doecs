#pragma once
#include "../doecs.h"
#include "Components.h"
#define PlayerComponents FPositionComponent, FRotationComponent
DeclareEntityArchetypePool(FPlayerPool, PlayerComponents);