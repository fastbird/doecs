#pragma once
#include "../doecs.h"
#include "Components.h"

#define WeaponComponents FPositionComponent, FRotationComponent, FWeaponComponent, FDurabilityComponent
DeclareEntityArchetypePool(FWeaponPool, WeaponComponents);