#include "EntitySystem.h"
#include "MovementSystem.h"
#include <assert.h>
int main()
{
	// need to call once per new pool
	InitializeEntityArchetypePool(FPlayerPool);
	InitializeEntityArchetypePool(FEnemyPool);

	auto entityId = de::CreateEntity<PlayerComponents>();
	auto entityId2 = de::CreateEntity<EnemyComponents>();

	MovementSystem* pMoveSystem = new MovementSystem;
	RunSystem(EntityPools, pMoveSystem);
	delete pMoveSystem;

	DestroyEntityArchetypePool(FPlayerPool);
	DestroyEntityArchetypePool(FEnemyPool);

	// component access
	FPositionComponent* comp = de::GetComponent<FPositionComponent>(entityId, EntityPools);
	if (comp)
	{
		std::cout << comp->x;
	}

	FRotationComponent* rotComp = de::GetComponent<FRotationComponent>(entityId2, EntityPools);
	assert(rotComp == nullptr);

	
}