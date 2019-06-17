#include "../doecs2.h"
#include "EntitySystem.h"
#include "Events.h"
void TestECS2()
{
	de2::DOECS ecs;
	ecs.AddPool<PlayerComponents>();
	auto entity = ecs.CreateEntity<PlayerComponents>();
	auto entity2 = ecs.CreateEntity<PlayerComponents>();
	auto entity3 = ecs.CreateEntity<PlayerComponents>();
	ecs.RunSystems();
	KnockBackEvent* evt = new KnockBackEvent(10, 10, 10);
	ecs.PushEvent(entity, evt);
	ecs.RunEvents();
	ecs.RemoveEntity(entity2);
	ecs.Flush();
	de2::DOECS ecs2;
	ecs2.AddPool<PlayerComponents>();
	ecs2.AddEntity(entity, FPositionComponent{ 10.f, 10.f, 10.f }, FRotationComponent{ 10.f, 10.f, 10.f, 1.f }, FLifeformComponent{ 100, 200 });
}