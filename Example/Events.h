#pragma once
#include "../doecs2.h"
#include "Components.h"
class KnockBackEvent : public de2::IEvent
{
	float X, Y, Z;
public:
	KnockBackEvent(float x, float y, float z)
		: X(x), Y(y), Z(z)
	{}

	virtual std::size_t GetComponentHashes(const uint64_t*& pHashes) override
	{
		static const uint64_t ComponentHashes[] = { typeid(FPositionComponent).hash_code() };
		pHashes = ComponentHashes;
		return  (uint32_t)de2::ArrayCount(ComponentHashes);
	}
	virtual void Execute(const de2::ComponentsArg& components) override
	{
		FPositionComponent* posComp = (FPositionComponent*)components[0];
		posComp->x += X;
		posComp->y += Y;
		posComp->z += Z;
	}
};
