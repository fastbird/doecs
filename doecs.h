#pragma once
#ifndef __doecs_header__
#define __doecs_header__
// Fastbird Engine
// Written by Jungwan Byun
// https://fastbirddev.blogspot.com

#include "doecs_type.h"
#include <unordered_map>
#include <vector>
#include <array>
#include <cstdlib>
#include <mutex>
#include <assert.h>

// Create a cpp file and define IMPLEMENT_DOECS then include this header file.
//#define IMPLEMENT_DOECS 1

#define DeclareEntityArchetypePool(PoolName, ...) using PoolName = de::impl::ArchetypePool<__VA_ARGS__>

// de stands for Do Ecs
namespace de
{
	namespace impl {
		constexpr int ChunkSize = 16 * 1024; // Usually CPU has 32 kb L1 cache and I decide to use half of it.
		constexpr int CacheLineSize = 64;
		class FEntityIdGen {
			std::mutex Mutex;
			EntityId NextId = 1;

		public:
			FEntityIdGen() = default;
			FEntityIdGen(EntityId startId)
				: NextId(startId) {}

			EntityId Gen() {
				std::lock_guard l(Mutex);
				return NextId++;
			}
		};
		extern FEntityIdGen EntityIdGen;
#ifdef IMPLEMENT_DOECS
		FEntityIdGen EntityIdGen;
#endif

		//
		// SizeOf
		//
		template < typename ... Types >
		struct SizeOf;

		template < typename TFirst >
		struct SizeOf < TFirst >
		{
			static const auto Value = sizeof(TFirst);
		};

		template < typename TFirst, typename ... TRemaining >
		struct SizeOf < TFirst, TRemaining ... >
		{
			static const auto Value = (sizeof(TFirst) + SizeOf<TRemaining...>::Value);
		};

		//
		// has_type
		//
		template <typename T, typename Tuple>
		struct has_type;

		template <typename T, typename... Us>
		struct has_type<T, std::tuple<Us...>> : std::disjunction<std::is_same<T, Us>...> {};

		//
		// intersection
		//
		template <typename S1, typename S2>
		struct intersect
		{
			template <std::size_t... Indices>
			static constexpr auto make_intersection(std::index_sequence<Indices...>) {

				return std::tuple_cat(
					std::conditional_t<
					has_type<std::tuple_element_t<Indices, S1>, S2>::value,
					std::tuple<std::tuple_element_t<Indices, S1>>,
					std::tuple<>
					>{}...);
			}
			using type = decltype(make_intersection(std::make_index_sequence<std::tuple_size<S1>::value>{}));
		};

		template<typename SubType, typename CheckingType, typename = void>
		struct has_all_type;

		template<typename SubType, typename CheckingType>
		struct has_all_type<SubType, CheckingType, std::enable_if_t< std::is_same_v<typename intersect<SubType, CheckingType>::type, SubType>>>
			: std::true_type
		{
		};

		template<typename SubType, typename CheckingType>
		struct has_all_type<SubType, CheckingType, std::enable_if_t< !std::is_same_v<typename intersect<SubType, CheckingType>::type, SubType>>>
			: std::false_type
		{
		};

		template<std::size_t N, typename T, typename... types>
		struct GetNthType
		{
			using type = typename GetNthType<N - 1, types...>::type;
		};

		template<typename T, typename... types>
		struct GetNthType<0, T, types...>
		{
			using type = T;
		};

		//
	// ArchetypePool
	//
		template<typename ... ComponentTypes>
		class ArchetypePool
		{
		public:
			using Tuple = std::tuple<ComponentTypes...>;
			static constexpr uint32_t EntitySize = SizeOf<ComponentTypes...>::Value;
			static constexpr uint32_t ElementCountPerChunk = (ChunkSize - sizeof(void*) - sizeof(uint32_t)) / EntitySize;
			static constexpr uint32_t ComponentCount = sizeof...(ComponentTypes);

			struct Chunk
			{
			public:
				std::tuple<std::array<ComponentTypes, ElementCountPerChunk>...> Components;
				uint32_t Count = 0;
				Chunk* Next = nullptr;

				void* operator new(std::size_t size)
				{
#ifdef _MSC_VER
					return _aligned_malloc(size, CacheLineSize);
#else
					// not tested.
					return std::aligned_alloc(size, CacheLineSize);
#endif
				}

				void operator delete(void* p)
				{

#ifdef _MSC_VER
					_aligned_free(p);
#else
					std::free(p);
#endif
				}

				template<typename ComponentType>
				ComponentType* GetComponent(uint32_t index)
				{
					return &std::get<std::array<ComponentType, ElementCountPerChunk>>(Components)[index];
				}

				void DeleteChunkHierarchy()
				{
					if (!Next)
						return;
					Next->DeleteChunkHierarchy();
					delete Next;
					Next = nullptr;
				}
			};

			static_assert(sizeof(Chunk) <= ChunkSize, "Invalid chunk size. Array alignment problem?");
			Chunk* RootChunk;
			std::unordered_map<EntityId, std::pair<Chunk*, uint32_t> > EntityToComponent;
			std::vector<EntityId> PendingRemove;
			std::mutex Mutex;

			static auto& Get()
			{
				static ArchetypePool<ComponentTypes...> Instance;
				return Instance;
			}

			void Initialize()
			{
				assert(!RootChunk);
				RootChunk = new Chunk;
			}

			void Destroy()
			{
				if (!RootChunk)
					return;
				auto next = RootChunk->Next;
				delete RootChunk;
				RootChunk = nullptr;
				while (next)
				{
					auto p = next;
					delete p;
					next = next->Next;
				}
			}

			void Flush()
			{
				std::sort(PendingRemove.begin(), PendingRemove.end());
				PendingRemove.erase(std::unique(PendingRemove.begin(), PendingRemove.end()), PendingRemove.end());
				while (!PendingRemove.empty()) {
					auto rit = PendingRemove.rbegin();
					int consecutiveCount = 1;
					void* chunk;
					uint32_t index;
					auto has = HasEntity(*rit, chunk, index);
					assert(has);
					++rit;
					for ( ; rit != PendingRemove.rend(); ++rit)
					{
						void* chunk2;
						uint32_t index2;
						has = HasEntity(*rit, chunk2, index2);
						assert(has);
						if (chunk == chunk2 && index2 + 1 == index) {
							++consecutiveCount;
							index = index2;
						}
						else {
							break;
						}
					}
					PendingRemove.erase(PendingRemove.end() - consecutiveCount, PendingRemove.end());
					Memmove((Chunk*)chunk, index, consecutiveCount);
				}
			}

			EntityId CreateEntity()
			{
				static_assert(ElementCountPerChunk > 50, "Entity is too big");
				auto entityId = EntityIdGen.Gen();
				auto chunk = RootChunk;
				while (chunk)
				{
					if (chunk->Count < ElementCountPerChunk)
					{
						auto componentIndex = chunk->Count++;
						EntityToComponent[entityId] = { chunk, componentIndex };
						return entityId;
					}
					if (!chunk->Next)
					{
						// create new chunk
						chunk->Next = new Chunk;
					}
					chunk = chunk->Next;
				}
				return INVALID_ENTITY_ID;
			}

			template<typename SystemType, typename ... ComponentTypes>
			void RunSystem(SystemType* system, std::tuple<ComponentTypes...> dummy)
			{
				auto chunk = RootChunk;
				while (chunk)
				{
					system->Execute(chunk->Count, &std::get< std::array<ComponentTypes, ElementCountPerChunk>>(chunk->Components)[0]...);
					chunk = chunk->Next;
				}
			}

			bool HasEntity(EntityId id, void*& chunk, uint32_t& index)
			{
				auto it = EntityToComponent.find(id);
				if (it != EntityToComponent.end())
				{
					chunk = it->second.first;
					index = it->second.second;
					return true;
				}
				return false;
			}

			template<std::size_t I>
			void MemmoveRecursive(Chunk* chunk, uint32_t removingIndex, uint32_t consecutiveCount)
			{
				assert(consecutiveCount >= 1);
				auto numNeedToMove = (chunk->Count - (removingIndex + consecutiveCount));
				if (chunk->Count == ElementCountPerChunk) {
					if (numNeedToMove) {
						auto& componentArray = std::get<I>(chunk->Components);
						memmove(&componentArray[removingIndex],
							&componentArray[removingIndex + consecutiveCount],
							sizeof(std::remove_reference_t<decltype(componentArray)>::value_type) * numNeedToMove);
					}
					if (chunk->Next && chunk->Next->Count > 0) {
						auto& componentArray = std::get<I>(chunk->Components);
						auto nextChunk = chunk->Next;
						auto& nextComponentArray = std::get<I>(nextChunk->Components);
						auto numCopyFromNextChunk = std::min(consecutiveCount, nextChunk->Count);
						memcpy(&componentArray[ElementCountPerChunk - consecutiveCount],
							&nextComponentArray[0],
							sizeof(std::remove_reference_t<decltype(componentArray)>::value_type) * numCopyFromNextChunk);
						MemmoveRecursive<I>(nextChunk, 0, numCopyFromNextChunk);
					}
					else
					{
						if constexpr (I == sizeof...(ComponentTypes) - 1) {
							assert(chunk->Count >= consecutiveCount);
							chunk->Count -= consecutiveCount;
						}
					}
				}
				else {
					if (numNeedToMove) {
						auto& componentArray = std::get<I>(chunk->Components);
						memmove(&componentArray[removingIndex],
							&componentArray[removingIndex + consecutiveCount],
							sizeof(std::remove_reference_t<decltype(componentArray)>::value_type) * numNeedToMove);
					}
					if constexpr (I == sizeof...(ComponentTypes) - 1)
					{
						assert(chunk->Count >= consecutiveCount);
						chunk->Count -= consecutiveCount;
						if (chunk->Next) {
							chunk->DeleteChunkHierarchy();
						}
					}
				}
			}

			template<std::size_t I>
			std::enable_if_t<I == sizeof...(ComponentTypes)> Memmove(Chunk* chunk, uint32_t removingIndex, uint32_t consecutiveCount)
			{				
			}

			template<std::size_t I = 0>
			std::enable_if_t<I < sizeof...(ComponentTypes)> Memmove(Chunk * chunk, uint32_t removingIndex, uint32_t consecutiveCount)
			{
				MemmoveRecursive<I>(chunk, removingIndex, consecutiveCount);
				Memmove<I + 1>(chunk, removingIndex, consecutiveCount);
			}

			bool RemoveEntity(EntityId id)
			{
				void* chunk;
				uint32_t index;
				if (HasEntity(id, chunk, index))
				{
					std::lock_guard l(Mutex);
					PendingRemove.push_back(id);
					return true;
				}
				return false;
			}

			template<typename ComponentType>
			ComponentType* GetComponent(void* chunk, uint32_t index)
			{
				return ((Chunk*)chunk)->GetComponent<ComponentType>(index);
			}
		};

		template<std::size_t I, typename SystemType, typename... Tp>
		typename std::enable_if_t<I == sizeof...(Tp)> RunSystemImpl(SystemType* system, std::tuple<Tp...>& pools)
		{ }


		template<std::size_t I = 0, typename SystemType, typename... Tp>
		typename std::enable_if_t < I < sizeof...(Tp)> RunSystemImpl(SystemType* system, std::tuple<Tp...>& pools)
		{
			auto& pool = std::get<I>(pools);
			if constexpr (has_all_type<SystemType::Tuple, std::remove_reference<decltype(pool)>::type::Tuple>::value)
			{
				pool.RunSystem(system, SystemType::Tuple{});
			}

			RunSystemImpl<I + 1>(system, pools);
		}

		template<std::size_t I = 0, typename ... PoolTypes>
		inline typename std::enable_if<I == sizeof...(PoolTypes)>::type
			RemoveEntityImpl(EntityId entityId, std::tuple<PoolTypes...>& pools)
		{
			return;
		}

		template<std::size_t I = 0, typename ... PoolTypes>
		inline typename std::enable_if < I < sizeof...(PoolTypes)>::type
			RemoveEntityImpl(EntityId entityId, std::tuple<PoolTypes...>& pools)
		{
			auto& pool = std::get<I>(pools);
			if (pool.RemoveEntity(entityId))
				return;

			RemoveEntityImpl<I + 1>(entityId, pools);
		}

		template<std::size_t I = 0, typename ComponentType, typename ... PoolTypes>
		inline typename std::enable_if<I == sizeof...(PoolTypes), ComponentType*>::type
			GetComponentImpl(EntityId entityId, std::tuple<PoolTypes...>& pools)
		{
			return nullptr;
		}

		template<std::size_t I = 0, typename ComponentType, typename ... PoolTypes>
		inline typename std::enable_if < I < sizeof...(PoolTypes), ComponentType*>::type
			GetComponentImpl(EntityId entityId, std::tuple<PoolTypes...>& pools)
		{
			auto& pool = std::get<I>(pools);
			[[maybe_unused]] void* chunk;
			[[maybe_unused]] uint32_t index;
			if constexpr (has_type<ComponentType, std::remove_reference_t<decltype(pool)>::Tuple>::value)
			{
				if (pool.HasEntity(entityId, chunk, index))
				{
					return pool.GetComponent<ComponentType>(chunk, index);
				}
			}

			return GetComponentImpl<I + 1, ComponentType>(entityId, pools);
		}

		struct InitializeFunctor
		{
			template<typename PoolType>
			void operator()(PoolType& pool)
			{
				pool.Initialize();
			}
		};

		struct DestroyFunctor
		{
			template<typename PoolType>
			void operator()(PoolType& pool)
			{
				pool.Destroy();
			}
		};

		struct FlushFunctor
		{
			template<typename PoolType>
			void operator()(PoolType& pool)
			{
				pool.Flush();
			}
		};


		template<std::size_t I, typename ... PoolTypes, typename F>
		std::enable_if_t < I == sizeof...(PoolTypes) > CallFunction(std::tuple<PoolTypes...>& pools, F f)
		{
		}

		template<std::size_t I = 0, typename ... PoolTypes, typename F>
		std::enable_if_t < I < sizeof...(PoolTypes) > CallFunction(std::tuple<PoolTypes...>& pools, F f)
		{
			auto& pool = std::get<I>(pools);
			f(pool);
			CallFunction<I + 1>(pools, f);
		}
	}
	
	template<typename ... ComponentTypes>
	struct System
	{
		using Tuple = std::tuple<ComponentTypes...>;
	};

	template<typename ... PoolTypes>
	void InitializePools(std::tuple<PoolTypes...>& pools)
	{
		impl::CallFunction(pools, impl::InitializeFunctor());
	}

	template<typename ... PoolTypes>
	void DestroyPools(std::tuple<PoolTypes...>& pools)
	{
		impl::CallFunction(pools, impl::DestroyFunctor());
	}

	template<typename ... PoolTypes>
	void FlushPools(std::tuple<PoolTypes...>& pools)
	{
		impl::CallFunction(pools, impl::FlushFunctor());
	}

	template<typename ... ComponentTypes>
	EntityId CreateEntity()
	{
		return impl::ArchetypePool<ComponentTypes...>::Get().CreateEntity();
	}

	template <typename EntityPoolsType>
	void RemoveEntity(EntityId entityId, EntityPoolsType& entityPools)
	{
		impl::RemoveEntityImpl<0>(entityId, entityPools);
	}

	template<typename SystemType, typename EntityPoolsType>
	void RunSystem(SystemType* system, EntityPoolsType& entityPools)
	{
		impl::RunSystemImpl(system, entityPools);
	}
	
	template <typename ComponentType, typename EntityPoolsType>
	ComponentType* GetComponent(EntityId entityId, EntityPoolsType& entityPools)
	{
		return impl::GetComponentImpl<0, ComponentType>(entityId, entityPools);
	}
}
#endif //__doecs_header__