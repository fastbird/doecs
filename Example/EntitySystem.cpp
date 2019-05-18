#define IMPLEMENT_DOECS

#include "EntitySystem.h"

std::tuple<FPlayerPool&, FEnemyPool&> EntityPools = { FPlayerPool::Get(), FEnemyPool::Get() };