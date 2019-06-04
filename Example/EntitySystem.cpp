#define IMPLEMENT_DOECS

#include "EntitySystem.h"

std::tuple<FPlayerPool&, FWeaponPool&> EntityPools = { FPlayerPool::Get(), FWeaponPool::Get() };