#include "EntitySystem.h"
#include "../doecs2.h"
#include "MovementSystem.h"
#include "Events.h"
#include <assert.h>

int main()
{
	de2::DOECS ecs;
	ecs.AddPool<PlayerComponents>();
	auto entity = ecs.CreateEntity< PlayerComponents>();
	ecs.RunSystems();
	KnockBackEvent* evt = new KnockBackEvent(10, 10, 10);
	ecs.PushEvent(entity, evt);
	ecs.RunEvents();

	
}

// old  system
void TestECS1()
{
	de::InitializePools(EntityPools);

	auto entityId = de::CreateEntity<PlayerComponents>();
	auto entityId2 = de::CreateEntity<WeaponComponents>();

	MovementSystem* pMoveSystem = new MovementSystem;
	RunSystem(pMoveSystem, EntityPools);

	// component access
	FPositionComponent* comp = de::GetComponent<FPositionComponent>(entityId, EntityPools);
	if (comp)
	{
		std::cout << comp->x;
	}

	FRotationComponent* rotComp = de::GetComponent<FRotationComponent>(entityId2, EntityPools);
	assert(rotComp == nullptr);

	de::RemoveEntity(entityId2, EntityPools);
	de::FlushPools(EntityPools);

	delete pMoveSystem;

	de::DestroyPools(EntityPools);
}