#pragma once
#ifndef __doecs_header__
#define __doecs_header__

#include <unordered_map>
#include <array>
#include <cstdlib>
#include <mutex>

// Create a cpp file and define IMPLEMENT_DOECS then include this header file.
//#define IMPLEMENT_DOECS 1

#define DeclareEntityArchetypePool(PoolName, ...) using PoolName = de::ArchetypePool<__VA_ARGS__>
#define ImplementEntityArchetypePool(PoolName) PoolName::Chunk* PoolName::RootChunk = nullptr;\
	std::unordered_map<de::EntityId, std::pair<PoolName::Chunk*, uint32_t> > PoolName::EntityToComponent
#define InitializeEntityArchetypePool(PoolName) PoolName::Initialize()
#define DestroyEntityArchetypePool(PoolName) PoolName::Destroy();

// de stands for Do Ecs
namespace de
{
	using EntityId = uint64_t;

	constexpr int ChunkSize = 16 * 1024; // Usually CPU has 32 kb L1 cache and I decide to use half of it.
	constexpr int CacheLineSize = 64;
	constexpr EntityId INVALID_ENTITY_ID = -1;

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

	//
	// ArchetypePool
	//
	template<typename ... ComponentTypes>
	class ArchetypePool
	{
	public:
		using Tuple = std::tuple<ComponentTypes...>;
		static constexpr uint32_t EntitySize = SizeOf<ComponentTypes...>::Value;
		static constexpr uint32_t ComponentCount = sizeof...(ComponentTypes);
		static constexpr uint32_t ElementCountPerChunk = (ChunkSize - sizeof(void*) - sizeof(uint32_t)) / EntitySize;
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
		};
		static_assert(sizeof(Chunk) <= ChunkSize, "Invalid chunk size. Array alignment problem?");
		static Chunk* RootChunk;
		static std::unordered_map<EntityId, std::pair<Chunk*, uint32_t> > EntityToComponent;

		static void Initialize()
		{
			RootChunk = new Chunk;
		}

		static void Destroy()
		{
			auto next = RootChunk->Next;
			delete RootChunk;
			while (next)
			{
				auto p = next;
				delete p;
				next = next->Next;
			}
		}
		static EntityId CreateEntity()
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
		static void RunSystem(SystemType* system, std::tuple<ComponentTypes...> dummy)
		{
			auto chunk = RootChunk;
			while (chunk)
			{
				for (uint32_t i = 0; i < chunk->Count; ++i)
				{
					system->Execute(std::get< std::array<ComponentTypes, ElementCountPerChunk>>(chunk->Components)[i]...);
				}
				chunk = chunk->Next;
			}			
		}
	};


	template<typename ... ComponentTypes>
	struct System
	{
		using Tuple = std::tuple<ComponentTypes...>;
	};

	template<std::size_t I, typename SystemType, typename... Tp>
	typename std::enable_if_t<I == sizeof...(Tp)> RunSystemImpl(std::tuple<Tp...>& pools, SystemType* system)
	{ }


	template<std::size_t I = 0, typename SystemType, typename... Tp>
	typename std::enable_if_t < I < sizeof...(Tp)> RunSystemImpl(std::tuple<Tp...>& pools, SystemType* system)
	{
		auto& pool = std::get<I>(pools);
		if constexpr (has_all_type<SystemType::Tuple, std::remove_reference<decltype(pool)>::type::Tuple>::value)
		{
			pool.RunSystem(system, SystemType::Tuple{});
		}

		RunSystemImpl<I + 1>(pools, system);
	}

	// struct MovementSystem : public System<PositionComponent>
	template<typename EntityPoolsType, typename SystemType>
	void RunSystem(EntityPoolsType& entityPools, SystemType* system)
	{
		RunSystemImpl(entityPools, system);
	}


	template<typename ... ComponentTypes>
	EntityId CreateEntity()
	{
		return ArchetypePool<ComponentTypes...>::CreateEntity();
	}
}
#endif //__doecs_header__