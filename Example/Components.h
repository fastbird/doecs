#pragma once
struct FPositionComponent
{
	float x, y, z;
};

struct FRotationComponent
{
	float x, y, z, w;
};

struct FLifeformComponent
{
	uint32_t HitPoint;
	uint32_t MaxHitPoint;
};

struct FWeaponComponent
{
	float Delay;
	float Charging;
	float Range;
};

struct FDurabilityComponent
{
	uint32_t Durability;
	uint32_t MaxDurability;
};
