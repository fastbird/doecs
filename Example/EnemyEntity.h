#pragma once
#include "../doecs.h"
#include "Components.h"

#define EnemyComponents FPositionComponent
DeclareEntityArchetypePool(FEnemyPool, EnemyComponents);