#pragma once
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <assert.h>

#include "doecs_type.h"
namespace de2
{
	namespace impl
	{
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

		template <class T>
		uint64_t hash_combine(uint64_t& seed, const T& v)
		{
			std::hash<T> hasher;
			const uint64_t kMul = 0x9ddfea08eb382d69ULL;
			uint64_t a = (hasher(v) ^ seed) * kMul;
			a ^= (a >> 47);
			uint64_t b = (seed ^ a) * kMul;
			b ^= (b >> 47);
			seed = b * kMul;
			return seed;
		}

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

		class IArchetypePool
		{
		public:
			virtual bool IsPoolFor(uint64_t componentHash) = 0;
			virtual EntityId CreateEntity() = 0;
		};

		template<typename ... ComponentTypes>
		class ArchetypePool : public IArchetypePool
		{
			uint64_t Hash;

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

		public:
			ArchetypePool(uint64_t hash)
				: Hash(hash)
			{
				RootChunk = new Chunk;
			}

			~ArchetypePool()
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

			bool IsPoolFor(uint64_t componentHash) override
			{
				return Hash == componentHash;
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
					for (; rit != PendingRemove.rend(); ++rit)
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

			EntityId CreateEntity() override
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
			std::enable_if_t < I < sizeof...(ComponentTypes)> Memmove(Chunk * chunk, uint32_t removingIndex, uint32_t consecutiveCount)
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

		template<std::size_t I = 0, typename ... ComponentTypes>
		void ComponentsHash(uint64_t & hash)
		{
			hash_combine(hash, typeid(std::tuple_element_t<I, std::tuple<ComponentTypes...>>).hash_code());
			if constexpr (I + 1 < sizeof...(ComponentTypes)) {
				ComponentsHash<I + 1, ComponentTypes...>(hash);
			}
		}
	}

	class DOECS
	{
		std::unordered_map<uint64_t, impl::IArchetypePool*> Pools;
	public:

		~DOECS();

		template<typename ... ComponentTypes>
		void AddPool()
		{
			uint64_t hash = 0;
			impl::ComponentsHash<0, ComponentTypes...>(hash);
			Pools.insert({hash, new impl::ArchetypePool<ComponentTypes...>(hash)});
		}

		template<typename ... ComponentTypes>
		EntityId CreateEntity()
		{
			uint64_t poolHash = 0;
			impl::ComponentsHash<0, ComponentTypes...>(poolHash);
			return CreateEntity(poolHash);
		}

		EntityId CreateEntity(uint64_t poolHash)
		{
			auto it = Pools.find(poolHash);
			if (it == Pools.end()) {
				return INVALID_ENTITY_ID;
			}
			return it->second->CreateEntity();
		}
	};
}