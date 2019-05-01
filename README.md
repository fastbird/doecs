# doecs
Data Oriented Entity Component System framework.

# How to use
```		
// Step 1. Define components, maybe in <EntityName>.h file
struct PositionComponent
{
	float x, y, z;
};

struct RotationComponent
{
	float x, y, z, w;
};

// Step 2. Declare Archetype Pool for the new entity type in <EntityName>.h file (e.g. PlayerEntity.h)
#define PlayerComponents PositionComponent, RotationComponent
DeclareEntityArchetypePool(APlayerPool, PlayerComponents);

// This lines could be located in another header file EnemyEntity.h for example.
#define EnemyComponents PositionComponent
DeclareEntityArchetypePool(AEnemyPool, EnemyComponents);

// Step 3. Add new EntityArchetypePools to the global tuple for EntityPools in <MyEntitySystem>.h file (e.g. EntitySystem.h)
extern std::tuple<APlayerPool, AEnemyPool> EntityPools;

// Step 4. Also need to add the new pool to the actual tuple implementation in <MyEntitySystem>.cpp file
std::tuple<APlayerPool, AEnemyPool> EntityPools;

// Step 5. Implementation for Pools goes to each of header files. (e.g. PlayerEntity.cpp and EnemyEntity.cpp)
ImplementEntityArchetypePool(APlayerPool);
ImplementEntityArchetypePool(AEnemyPool);

// Step 6. Define Systems in a new header file (e.g. MovementSystem.h)
struct MovementSystem : public de::System<PositionComponent>
{
	void Execute(PositionComponent& pos)
	{
		// move it
	}
};

// Step 7. Use defined entity and system like this
void Test()
{
	// need to call once per new pool
	InitializeEntityArchetypePool(APlayerPool);
	InitializeEntityArchetypePool(AEnemyPool);

	auto entityId = CreateEntity<PlayerComponents>();
	auto entityId2 = CreateEntity<EnemyComponents>();

	MovementSystem* pMoveSystem = new MovementSystem;
	RunSystem(pMoveSystem);
	delete pMoveSystem;

	DestroyEntityArchetypePool(APlayerPool);
	DestroyEntityArchetypePool(AEnemyPool);
}
```