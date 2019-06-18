#pragma once
// Fastbird Engine
// Written by Jungwan Byun
// https://fastbirddev.blogspot.com

#include <cstddef>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <assert.h>
#include <algorithm>

#include "doecs_type.h"

// Platform dependent code
#ifndef DLL_EXPORT
#	ifdef _WINDLL
#		define DLL_EXPORT __declspec(dllexport)
#	else
#		define DLL_EXPORT __declspec(dllimport)
#	endif //_WINDLL
#endif

namespace de2
{
	class DOECS;
	class ISystem
	{
	public:
		volatile bool Done = false;

		virtual std::size_t GetComponentHashes(const uint64_t*& pHashes) = 0;
		virtual void Execute(uint32_t elementCount, const de2::ComponentsArg& components) = 0;
	};

	class IEvent
	{
	public:
		virtual std::size_t GetComponentHashes(const uint64_t*& pHashes) = 0;
		virtual void Execute(const de2::ComponentsArg& components) = 0;
	};

	template<std::size_t N, typename T>
	constexpr size_t ArrayCount(const T(&arr)[N])
	{
		return N;
	}

	namespace impl
	{
		constexpr int ChunkSize = 16 * 1024; // Usually CPU has 32 kb L1 cache including instruction and data cache.
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
		DLL_EXPORT extern FEntityIdGen EntityIdGen;

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

		template <class _Ty, class _Alloc = std::allocator<_Ty>>
		class SortedVector : public std::vector<_Ty, _Alloc> {
			using super = std::vector<_Ty, _Alloc>;
			using iterator = typename std::vector<_Ty, _Alloc>::iterator;

		public:
			iterator insert(const _Ty& v) {
				iterator it = std::lower_bound(super::begin(), super::end(), v);
				if (it != super::end() && *it == v) {
					*it = v;
				}
				else {
					it = super::insert(it, v);
				}
				return it;
			}

			bool contains(const _Ty& v) const {
				return std::binary_search(super::begin(), super::end(), v);
			}

			bool erase(const _Ty& v) {
				auto it = std::lower_bound(super::begin(), super::end(), v);
				if (it != super::end() && *it == v) {
					super::erase(it);
					return true;
				}
				return false;
			}

		private:
			void push_back(const _Ty& v) = delete;
		};

		//
		// IArchetypePool
		//
		class IArchetypePool
		{
		public:
			virtual bool IsPoolFor(uint64_t componentHash) = 0;
			virtual bool IsPoolFor(ISystem* system) = 0;
			virtual EntityId CreateEntity() = 0;
			virtual bool RemoveEntity(EntityId entity) = 0;
			virtual uint32_t GetComponents(uint32_t chunkIndex, uint64_t hash, void*& components) = 0;
			virtual void* GetComponent(EntityId entity, uint64_t componentHash) = 0;
			virtual void* SetComponent(EntityId entity, uint64_t componentHash, void* comp) = 0;
			virtual void PushEvent(EntityId entId, IEvent* evt) = 0;
			virtual void RunEvents() = 0;
			virtual void Flush() = 0;
		};

		template<typename ... ComponentTypes>
		class ArchetypePool : public IArchetypePool
		{
			uint64_t Hash;
			std::vector<uint64_t> ComponentHashes;

		public:
			using Tuple = std::tuple<ComponentTypes...>;
			using MovedFromTo = std::pair<uint32_t, uint32_t>;
			static constexpr uint32_t EntitySize = SizeOf<ComponentTypes...>::Value;
			static constexpr uint32_t ElementCountPerChunk = (ChunkSize - sizeof(void*) - sizeof(uint32_t)) / EntitySize;
			static constexpr uint32_t ComponentCount = sizeof...(ComponentTypes);

			struct Chunk
			{
			public:
				std::tuple<std::array<ComponentTypes, ElementCountPerChunk>...> Components;
				constexpr static uint32_t InvalidIndex = -1;
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

				template<std::size_t I>
				std::enable_if_t<I == sizeof...(ComponentTypes), uint32_t> GetComponents(uint32_t componentTupleIndex, void*& components)
				{
					return 0;
				}

				template<std::size_t I = 0>
				std::enable_if_t < I < sizeof...(ComponentTypes), uint32_t> GetComponents(uint32_t componentTupleIndex, void*& components)
				{
					if (I == componentTupleIndex) {
						components = &std::get<I>(Components)[0];
						return Count;
					}
					else {
						return GetComponents<I + 1>(componentTupleIndex, components);
					}
				}

				template<std::size_t I>
				std::enable_if_t<I == sizeof...(ComponentTypes), void*> GetComponent(uint32_t componentTupleIndex, uint32_t entityIndex)
				{
					return nullptr;
				}

				template<std::size_t I = 0>
				std::enable_if_t < I < sizeof...(ComponentTypes), void*> GetComponent(uint32_t componentTupleIndex, uint32_t entityIndex)
				{
					if (I == componentTupleIndex) {
						return &std::get<I>(Components)[entityIndex];
					}
					else {
						return GetComponent<I + 1>(componentTupleIndex, entityIndex);
					}
				}

				template<std::size_t I>
				std::enable_if_t<I == sizeof...(ComponentTypes), void*> SetComponent(uint32_t componentTupleIndex, uint32_t entityIndex, void* compData)
				{
					return nullptr;
				}

				template<std::size_t I = 0>
				std::enable_if_t < I < sizeof...(ComponentTypes), void*> SetComponent(uint32_t componentTupleIndex, uint32_t entityIndex, void* compData)
				{
					if (I == componentTupleIndex) {
						auto& componentArray = std::get<I>(Components);
						auto* comp = &componentArray[entityIndex];
						memcpy(comp, compData, sizeof(std::remove_reference_t<decltype(componentArray)>::value_type));
						return comp;
					}
					else {
						return SetComponent<I + 1>(componentTupleIndex, entityIndex);
					}
				}

				void DeleteChunkHierarchy()
				{
					if (!Next)
						return;
					Next->DeleteChunkHierarchy();
					delete Next;
					Next = nullptr;
				}

				template<std::size_t I>
				std::enable_if_t<I == sizeof...(ComponentTypes)> SetComponents(uint32_t entityIndex, std::tuple<ComponentTypes&&...>&& source)
				{
				}

				template<std::size_t I = 0>
				std::enable_if_t < I < sizeof...(ComponentTypes)> SetComponents(uint32_t entityIndex, std::tuple<ComponentTypes&&...>&& source)
				{
					std::get<I>(Components)[entityIndex] = std::get<I>(source);
					SetComponents<I + 1>(entityIndex, std::forward<std::tuple<ComponentTypes&& ...>>(source));
				}

				uint32_t FindLastValidIndexWhileRemoving(const std::vector<uint32_t>& entities,
					const std::vector<MovedFromTo>& invalidatedIndex) const
				{
					assert(!entities.empty());
					if (Count == 0)
						return InvalidIndex;

					for (uint32_t i = Count - 1; i >= 0; --i) {
						if (std::find_if(invalidatedIndex.begin(), invalidatedIndex.end(), [i](const MovedFromTo& v) {return v.first == i; })
							!= invalidatedIndex.end())
							continue;

						if (i > entities.back())
							return i;

						if (std::find(entities.rbegin(), entities.rend(), i) == entities.rend())
							return i;
					}
					return InvalidIndex;
				}

				std::vector<MovedFromTo> /*Chunk::*/RemoveEntities(const std::vector<uint32_t>& entities)
				{
					assert(!entities.empty());
					assert(entities.size() <= Count);
					std::vector<MovedFromTo> invalidatedIndex;
					for (uint32_t index : entities) {
						uint32_t lastValidIndex = FindLastValidIndexWhileRemoving(entities, invalidatedIndex);
						if (lastValidIndex != InvalidIndex) {
							Memcpy<0>(index, lastValidIndex);
							invalidatedIndex.push_back({ lastValidIndex, index });
						}
						--Count;
					}
					return invalidatedIndex;
				}

				template<std::size_t I>
				std::enable_if_t<I == sizeof...(ComponentTypes)> Memcpy(uint32_t dest, uint32_t src)
				{
				}

				template<std::size_t I = 0>
				std::enable_if_t < I < sizeof...(ComponentTypes)> Memcpy(uint32_t dest, uint32_t src)
				{
					auto& componentArray = std::get<I>(Components);
					memcpy(&componentArray[dest], &componentArray[src],
						sizeof(std::remove_reference_t<decltype(componentArray)>::value_type));
					Memcpy<I + 1>(dest, src);
				}
			};

			static_assert(sizeof(Chunk) <= ChunkSize, "Invalid chunk size. Array alignment problem?");
			Chunk* RootChunk;
			std::unordered_map<EntityId, std::pair<Chunk*, uint32_t> > EntityToComponent;
			std::unordered_map<Chunk*, std::array<EntityId, ElementCountPerChunk>> EntityIdsPerChunk;
			std::unordered_map<EntityId, std::vector<IEvent*>> Events;

			struct RemovingEntity {
				EntityId Entity;
				Chunk* Chunk;
				uint32_t Index;
				bool operator ==(const RemovingEntity& other) const { return Entity == other.Entity; }
				bool operator <(const RemovingEntity& other) const {
					return Chunk < other.Chunk ||
						Chunk == other.Chunk && Index < other.Index;
				}
			};

			SortedVector<RemovingEntity> PendingRemove;
			std::mutex Mutex;

		public:
			ArchetypePool(uint64_t hash, std::vector<uint64_t>&& componentHashes)
				: Hash(hash)
				, ComponentHashes(componentHashes)
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

			bool IsPoolFor(ISystem* system) override
			{
				const uint64_t* systemComponents;
				auto count = system->GetComponentHashes(systemComponents);
				bool contains = true;
				for (std::size_t i = 0; i < count; ++i) {
					if (std::find(ComponentHashes.begin(), ComponentHashes.end(), systemComponents[i]) == ComponentHashes.end())
						return false;
				}
				return true;
			}

			void RecacheMovedEntities(Chunk* chunk, std::vector<MovedFromTo>&& movedEntities) {
				auto& entityIds = EntityIdsPerChunk[chunk];
				for (auto& it : movedEntities) {
					entityIds[it.second] = entityIds[it.first];
					EntityToComponent[entityIds[it.second]] = { chunk, it.second };
					EntityToComponent.erase(entityIds[it.first]);
					entityIds[it.first] = 0;
				}
			}

			void /*ArchetypePool::*/Flush() override
			{
				std::vector<uint32_t> indexToRemove;
				Chunk* lastChunk = nullptr;
				for (size_t i = 0; i < PendingRemove.size(); ++i)
				{
					if (lastChunk == nullptr || lastChunk == PendingRemove[i].Chunk) {
						lastChunk = PendingRemove[i].Chunk;
						indexToRemove.push_back(PendingRemove[i].Index);
					}
					else {
						if (!indexToRemove.empty()) {
							RecacheMovedEntities(lastChunk, lastChunk->RemoveEntities(indexToRemove));
							indexToRemove.clear();
						}
						lastChunk = PendingRemove[i].Chunk;
						indexToRemove.push_back(PendingRemove[i].Index);
					}
				}
				if (!indexToRemove.empty()) {
					RecacheMovedEntities(lastChunk, lastChunk->RemoveEntities(indexToRemove));
				}
				PendingRemove.clear();

				
				/*while (!PendingRemove.empty()) {
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
				}*/
			}

			EntityId CreateEntity() override
			{
				static_assert(ElementCountPerChunk > 50, "Entity is too big");
				auto entity = EntityIdGen.Gen();
				auto chunk = RootChunk;
				while (chunk)
				{
					if (chunk->Count < ElementCountPerChunk)
					{
						auto componentIndex = chunk->Count++;
						EntityToComponent[entity] = { chunk, componentIndex };
						EntityIdsPerChunk[chunk][componentIndex] = entity;
						return entity;
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
			
			EntityId /*ArchetypePool::*/AddEntity(std::tuple<ComponentTypes&&...>&& components) {
				static_assert(ElementCountPerChunk > 50, "Entity is too big");
				auto entity = EntityIdGen.Gen();
				return AddEntity(entity, std::forward<std::tuple<ComponentTypes && ...>>(components));
			}

			EntityId /*ArchetypePool::*/AddEntity(EntityId entity, std::tuple<ComponentTypes&& ...>&& components) {
				static_assert(ElementCountPerChunk > 50, "Entity is too big");
				auto chunk = RootChunk;
				while (chunk)
				{
					if (chunk->Count < ElementCountPerChunk)
					{
						auto componentIndex = chunk->Count++;
						EntityToComponent[entity] = { chunk, componentIndex };
						EntityIdsPerChunk[chunk][componentIndex] = entity;
						chunk->SetComponents(componentIndex, std::forward<std::tuple<ComponentTypes && ...>>(components));
						return entity;
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


			uint32_t GetComponents(uint32_t chunkIndex, uint64_t hash, void*& components) override
			{
				auto chunk = RootChunk;
				while (chunkIndex > 0) {
					--chunkIndex;
					if (!chunk->Next)
						return 0;
					chunk = chunk->Next;
				}
				if (!chunk)
					return 0;

				auto it = std::find(ComponentHashes.begin(), ComponentHashes.end(), hash);
				if (it == ComponentHashes.end())
					return 0;

				return chunk->GetComponents((uint32_t)std::distance(ComponentHashes.begin(), it), components);
			}

			void* GetComponent(EntityId entity, uint64_t componentHash) override
			{
				void* chunk;
				uint32_t index;
				if (HasEntity(entity, chunk, index))
				{
					return GetComponent(chunk, componentHash, index);
				}
				return nullptr;
			}

			void* GetComponent(void* chunk, uint64_t componentHash, uint32_t index)
			{
				auto it = std::find(ComponentHashes.begin(), ComponentHashes.end(), componentHash);
				if (it == ComponentHashes.end())
					return nullptr;

				return ((Chunk*)chunk)->GetComponent((uint32_t)std::distance(ComponentHashes.begin(), it), index);
			}

			template<typename ComponentType>
			ComponentType* GetComponent(void* chunk, uint32_t index)
			{
				return ((Chunk*)chunk)->GetComponent<ComponentType>(index);
			}

			void* SetComponent(EntityId entity, uint64_t componentHash, void* comp) override
			{
				void* chunk;
				uint32_t index;
				if (HasEntity(entity, chunk, index))
				{
					return GetComponent(chunk, componentHash, index);
				}
				return nullptr;
			}

			void* SetComponent(void* chunk, uint64_t componentHash, uint32_t index, void* comp)
			{
				auto it = std::find(ComponentHashes.begin(), ComponentHashes.end(), componentHash);
				if (it == ComponentHashes.end())
					return nullptr;

				return ((Chunk*)chunk)->SetComponent(std::distance(ComponentHashes.begin(), it), index, comp);
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
			std::enable_if_t<I == sizeof...(ComponentTypes)> Memmove(Chunk * chunk, uint32_t removingIndex, uint32_t consecutiveCount)
			{
			}

			template<std::size_t I = 0>
			std::enable_if_t < I < sizeof...(ComponentTypes)> Memmove(Chunk * chunk, uint32_t removingIndex, uint32_t consecutiveCount)
			{
				MemmoveRecursive<I>(chunk, removingIndex, consecutiveCount);
				Memmove<I + 1>(chunk, removingIndex, consecutiveCount);
			}

			bool RemoveEntity(EntityId entity) override
			{
				void* chunk;
				uint32_t index;
				if (HasEntity(entity, chunk, index))
				{
					std::lock_guard l(Mutex);
					PendingRemove.insert(RemovingEntity{ entity, (Chunk*)chunk, index });
					return true;
				}
				return false;
			}

			void PushEvent(EntityId entId, IEvent* evt) override
			{
				Events[entId].push_back(evt);
			}

			void RunEvents() override
			{
				for (auto& it : Events) {
					void* chunk;
					uint32_t index;
					if (HasEntity(it.first, chunk, index)){
						for (auto& evt : it.second) {
							const uint64_t* componentHashes = nullptr;
							auto count = evt->GetComponentHashes(componentHashes);
							ComponentsArg components;
							for (size_t i = 0; i < count; ++i)
							{
								auto pComponent = GetComponent(chunk, componentHashes[i], index);
								assert(pComponent);
								components.push_back(pComponent);
							}
							evt->Execute(components);
							delete evt;
						}
					}
					else {
						for (auto& evt : it.second) {
							delete evt;
						}
					}
				}
				Events.clear();
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
		using PoolContainer = std::unordered_map<uint64_t, impl::IArchetypePool*>;
		PoolContainer Pools;
		std::unordered_map<EntityId, uint64_t> EntityPoolMap;
		std::vector<ISystem*> Systems;
		std::unordered_map<ISystem*, std::vector<ISystem*>> SystemDependencies;
	public:

		~DOECS();

		template<typename ... ComponentTypes>
		PoolContainer::iterator AddPool()
		{
			uint64_t hash = 0;
			impl::ComponentsHash<0, ComponentTypes...>(hash);
			return Pools.insert({ hash, new impl::ArchetypePool<ComponentTypes...>(hash, { typeid(ComponentTypes).hash_code()... }) }).first;
		}

		void AddSystem(ISystem* system)
		{
			Systems.push_back(system);
		}

		void RemoveSystem(ISystem* system)
		{
			Systems.erase(std::remove(Systems.begin(), Systems.end(), system), Systems.end());
		}

		template<typename ... ComponentTypes>
		EntityId CreateEntity(bool autoCreatePool = true)
		{
			uint64_t poolHash = 0;
			impl::IArchetypePool* pool = nullptr;
			impl::ComponentsHash<0, ComponentTypes...>(poolHash);
			
			auto it = Pools.find(poolHash);
			if (it == Pools.end()) 
			{
				if (autoCreatePool) 
				{
					it = AddPool<ComponentTypes...>();
					pool = it->second;
				}
			}
			else {
				pool = it->second;
			}
			if (!pool)
				return INVALID_ENTITY_ID;

			auto entity = pool->CreateEntity();
			if (entity != INVALID_ENTITY_ID) {
				EntityPoolMap[entity] = poolHash;
			}
			return entity;
		}

		template<typename ... ComponentTypes>
		EntityId /*DOECS::*/AddEntity(ComponentTypes&& ... components) {
			uint64_t poolHash = 0;
			impl::ComponentsHash<0, ComponentTypes...>(poolHash);
			auto it = Pools.find(poolHash);
			if (it == Pools.end()) {
				return INVALID_ENTITY_ID;
			}
			auto pool = (impl::ArchetypePool<ComponentTypes...>*)(it->second);
			auto entity = pool->AddEntity(std::forward_as_tuple<ComponentTypes...>(std::forward<ComponentTypes>(components)...));
			EntityPoolMap[entity] = poolHash;
			return entity;
		}

		template<typename ... ComponentTypes>
		EntityId /*DOECS::*/AddEntity(EntityId entity, ComponentTypes&& ... components) {
			uint64_t poolHash = 0;
			impl::ComponentsHash<0, ComponentTypes...>(poolHash);
			auto it = Pools.find(poolHash);
			if (it == Pools.end()) {
				return INVALID_ENTITY_ID;
			}
			auto pool = (impl::ArchetypePool<ComponentTypes...>*)(it->second);
			entity = pool->AddEntity(entity, std::forward_as_tuple<ComponentTypes...>(std::forward<ComponentTypes>(components)...));
			EntityPoolMap[entity] = poolHash;
			return entity;
		}

		bool /*DOECS::*/RemoveEntity(EntityId entity) {
			auto pool = GetPoolForEntity(entity);
			if (!pool)
				return false;
			return pool->RemoveEntity(entity);
		}

		template<typename ComponentType>
		ComponentType* GetComponent(EntityId entity)
		{
			auto pool = GetPoolForEntity(entity);
			if (!pool)
				return nullptr;
			return (ComponentType*)pool->GetComponent(entity, typeid(ComponentType).hash_code());
		}

		template<typename ComponentType>
		ComponentType* SetComponent(EntityId entity, ComponentType&& comp)
		{
			auto pool = GetPoolForEntity(entity);
			if (!pool)
				return nullptr;
			return (ComponentType*)pool->SetComponent(entity, typeid(ComponentType).hash_code(), &comp);
		}

		void RunSystem(ISystem* system)
		{
			const uint64_t* componentHashes = nullptr;
			std::size_t componentCount = system->GetComponentHashes(componentHashes);
			for (auto& pool : Pools)
			{
				if (pool.second->IsPoolFor(system))
				{
					int chunkIndex = 0;
					bool checkNextChunk = true;
					// loop over chunks;
					while (checkNextChunk) {
						ComponentsArg requiredComponents;
						// loop over required components;
						uint32_t count = 0;
						for (uint32_t c = 0; c < componentCount; ++c) {
							void* components = nullptr;
							uint32_t count_ = pool.second->GetComponents(chunkIndex, componentHashes[c], components);

							if (count == 0 || components == nullptr) {
								checkNextChunk = false;
								break;
							}
							assert(count == 0 || count == count_);
							count = count_;
							requiredComponents.push_back(components);
						}
						system->Execute(count, requiredComponents);
						++chunkIndex;
					}
				}
			}
		}

		void RunSystems()
		{
			for (auto system : Systems) {
				system->Done = false;
			}

			for (auto system : Systems) {
				RunSystem(system);
			}
		}

		bool PushEvent(EntityId entId, IEvent* evt)
		{
			impl::IArchetypePool* pool = GetPoolForEntity(entId);
			if (!pool)
				return false;
			pool->PushEvent(entId, evt);
			return true;
		}

		void RunEvents()
		{
			for (auto& pool : Pools) {
				pool.second->RunEvents();
			}
		}

		void Flush()
		{
			for (auto& pool : Pools) {
				pool.second->Flush();
			}
		}

	private:
		impl::IArchetypePool* GetPoolForEntity(EntityId entId) {
			auto it = EntityPoolMap.find(entId);
			if (it != EntityPoolMap.end()) {
				return Pools.find(it->second)->second;
			}
			return nullptr;
		}
	};
}