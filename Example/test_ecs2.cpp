#include "../doecs2.h"
#include "EntitySystem.h"
#include "Events.h"
void TestECS2()
{
	de2::DOECS ecs;
	ecs.AddPool<PlayerComponents>();
	auto entity = ecs.CreateEntity< PlayerComponents>();
	ecs.RunSystems();
	KnockBackEvent* evt = new KnockBackEvent(10, 10, 10);
	ecs.PushEvent(entity, evt);
	ecs.RunEvents();
}