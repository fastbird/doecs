#define IMPLEMENT_DOECS

#include "EntitySystem.h"

std::tuple<FPlayerPool, FEnemyPool> EntityPools;
ImplementEntityArchetypePool(FPlayerPool);
ImplementEntityArchetypePool(FEnemyPool);