#include "doecs2.h"

namespace de2
{
	namespace impl {
		FEntityIdGen EntityIdGen;
	}

	DOECS::~DOECS()
	{
		for (auto p : Pools)
		{
			delete p.second;
		}
	}
}