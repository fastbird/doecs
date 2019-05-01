# doecs
Data Oriented Entity Component System framework.


## Features
* CPU cache friendly
  * Customizable cache line & chunk size. Default value is 64 bytes for cache line and 16 KB for chunks.
* Single header file
  * Just drop it into your project
* Needs C++17 compiler


# How to use
For the concrete usage example, see ./Example/ project.

For the Simpler explanation, check the following codes.

```		
// Step 1. Define components in a new header file. (e.g. Components.h)
struct FPositionComponent
{
	float x, y, z;
};

struct FRotationComponent
{
	float x, y, z, w;
};

// Step 2. Declare ArchetypePools for the new entities in new header files. 
// (e.g. PlayerEntity.h and EnemyEntity.h)
#define PlayerComponents FPositionComponent, FRotationComponent
DeclareEntityArchetypePool(FPlayerPool, PlayerComponents);

#define EnemyComponents FPositionComponent
DeclareEntityArchetypePool(FEnemyPool, EnemyComponents);

// Step 3. Add new EntityArchetypePools to the global tuple for EntityPools 
// in <MyEntitySystem>.h file (e.g. EntitySystem.h)
extern std::tuple<FPlayerPool, FEnemyPool> EntityPools;

// Step 4. Also need to add the new pool to the actual tuple implementation 
// in <MyEntitySystem>.cpp file In this cpp file, you must define IMPLEMENT_DOECS macro 
// before including the doecs.h header file so that the required implementation 
// for doecs framework is compiled in this translation unit.
std::tuple<FPlayerPool, FEnemyPool> EntityPools;

// Step 5. Implement new pools with following codes in each of entity cpp files
// (e.g. PlayerEntity.cpp and EnemyEntity.cpp) or if you don't want to create 
// a new cpp file <MyEntitySystem.cpp> would be a good as well.
ImplementEntityArchetypePool(FPlayerPool);
ImplementEntityArchetypePool(FEnemyPool);

// Step 6. Define Systems in a new header file (e.g. MovementSystem.h)
struct MovementSystem : public de::System<FPositionComponent>
{
	void Execute(FPositionComponent& pos)
	{
		// move it
	}
};

// Step 7. Use defined entity and system like this
void Test()
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
}
```