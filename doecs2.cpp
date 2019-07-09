// Fastbird Engine
// Written by Jungwan Byun
// https://fastbirddev.blogspot.com

// change this if you are not compiling DOECS as a part of a dll.
#define DOECS_IN_DLL 1

#include "doecs2.h"

namespace de2
{
	namespace impl {
		DLL_EXPORT FEntityIdGen EntityIdGen;
		DLL_EXPORT EntityId GenerateEntityId() //  If you get an error in this line, check the define 'DOECS_IN_DLL' at the top of this file.
		{
			static std::mutex Mutex;
			std::lock_guard lock(Mutex);
			return EntityIdGen.Gen();
		}
	}

	DOECS::~DOECS()
	{
		for (auto p : Pools)
		{
			delete p.second;
		}
	}
}