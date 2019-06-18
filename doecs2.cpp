// Fastbird Engine
// Written by Jungwan Byun
// https://fastbirddev.blogspot.com

#include "doecs2.h"

// Platform dependent code
#ifdef _WINDLL
#		define DLL_EXPORT __declspec(dllexport)
#	else
#		define DLL_EXPORT __declspec(dllimport)
#	endif //_WINDLL
namespace de2
{
	namespace impl {
		DLL_EXPORT FEntityIdGen EntityIdGen;
	}

	DOECS::~DOECS()
	{
		for (auto p : Pools)
		{
			delete p.second;
		}
	}
}