#ifndef FORK_NUMBER_TREE_HEAD
#define FORK_NUMBER_TREE_HEAD

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <mutex>

namespace FNTree {
	struct BitCovers {
		size_t shifts[sizeof(size_t) * CHAR_BIT] = {0};
		constexpr BitCovers() {
			size_t i = (sizeof(size_t) * CHAR_BIT);
			size_t j = 0;
			do {
				i -= 1;
				shifts[j] = ~0UL >> i;
				j +=1;
			} while(i > 0);
		}
	};

	static constexpr BitCovers BIT_COVERS;

	inline constexpr size_t hashData(void* data, size_t size) {
		size_t hash = 5381;
		uint64_t* reader = (uint64_t*)data;
		size_t count = size / sizeof(uint64_t);
		for (size_t i = 0; i < count; ++i)
		{
			hash = ((hash << 5) + hash) ^ reader[i];
		}
		return hash;
	}

	struct KeyValuePair {
		unsigned char key[16] = {0};
		void* value = nullptr;

		void printKey() const {
			for (size_t i = 0; i < sizeof(key); ++i)
			{
				printf("%u ", key[i]);
			}
			printf("\n");
		}

		void printValue() const {
			int* reader = (int*)value;
			printf("%d\n", *reader);
		}
	};

	struct KeyValueList {
		KeyValuePair** lst = nullptr;
		size_t len = 0;
		size_t cap = 0;

		KeyValueList(KeyValuePair* kvp, size_t cap): cap(cap) {
			lst = (KeyValuePair**)calloc(cap, sizeof(KeyValuePair*));
			lst[len++] = kvp;
		}

		void push(KeyValuePair* kvp) {
			if (len == cap) {
				cap *= cap;
				lst = (KeyValuePair**)realloc(lst, cap);
			}
			lst[len++] = kvp;
		}
	};

	struct KeyValueSpot {
		KeyValuePair* kvp = nullptr;
		struct KeyValueSpot* next = nullptr;
	};

	struct ParitionLock {
		std::mutex mux;
	};

	struct ScopedPartLock {
		ScopedPartLock(ParitionLock* ptl):_ptl(ptl) {
			_ptl->mux.lock();
		}

		~ScopedPartLock() {
			_ptl->mux.unlock();
		}

		ParitionLock* _ptl;
	};

	constexpr size_t getPartitionCount(size_t level, size_t childCount) {
		size_t total = 0;
		size_t childCumulative = childCount;
		for (size_t i = 0; i < level; ++i)
		{
			total += childCumulative;
			childCumulative *= childCount;
		}

		return total;
	}

	constexpr size_t getBitCount(size_t childCount) {
		if (childCount == 2) {
			return 1;
		} else if (childCount == 4) {
			return 2;
		} else if (childCount == 8) {
			return 3;
		} else if (childCount == 16) {
			return 4;
		} else if (childCount == 32) {
			return 5;
		} else if (childCount == 64) {
			return 6;
		} else if (childCount == 128) {
			return 7;
		}
		return (size_t)-1;
	}

	static size_t totalMemUsed = 0;
	static size_t collCount = 0;

	template <size_t keySize, size_t childCount /*, size_t partLevel = (size_t)-1*/>
	struct BitNode {
		template<size_t amount, size_t bCount>
		struct OffSetArr {
				size_t offsets[amount] = {0};
				constexpr OffSetArr() {
					size_t i = amount;
					size_t j = 0;
					do {
						i -= 1;
						offsets[j] = i * bCount;
						j += 1;
					} while (i > 0);
			}
		};

		BitNode() { FNTree::totalMemUsed += sizeof(BitNode);}

		static_assert(getBitCount(childCount) != (size_t)-1);
		static constexpr size_t bitCount = getBitCount(childCount);
		static_assert(keySize % bitCount == 0);
		static constexpr size_t bitIndex = bitCount - 1;
		static constexpr size_t bitShift = BIT_COVERS.shifts[bitIndex];
		static constexpr OffSetArr<keySize / bitCount, bitCount> offsets;
		static constexpr size_t offsetsSize = sizeof(offsets.offsets) / sizeof(offsets.offsets[0]);
		static constexpr size_t bridgesSize = offsetsSize - 1;
		void* lock = nullptr;
		void* children[childCount] = {nullptr};
	};

	template <size_t keySize, size_t childCount>
	void makeParitions(BitNode<keySize, childCount>* tree, size_t level) {
		if (level == 0) {
			tree->lock = new ParitionLock();
			return;
		}
		for (size_t i = 0; i < childCount; ++i)
		{
			tree->children[i] = new BitNode<keySize, childCount>();
			if (level > 0) {
				makeParitions((BitNode<keySize, childCount>*)tree->children[i], level - 1);
			}
		}
	}

	template <size_t keySize, size_t childCount>
	void insertInto(BitNode<keySize, childCount>* tree, size_t key, void* data) {
		BitNode<keySize, childCount>* current = tree;
		size_t i = 0;
		for (; i < BitNode<keySize, childCount>::bridgesSize; ++i)
		{
			//printf("key slice is %zu\n", key >> BitNode<keySize, childCount>::offsets.offsets[i]);
			size_t shifted = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
			if (current->children[shifted] == nullptr) {
				current->children[shifted] = new BitNode<keySize, childCount>();
			} 
			current =  (BitNode<keySize, childCount>*)current->children[shifted];
		}
		size_t shiftedLast = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
		current->children[shiftedLast] = data;
	}

	template <size_t keySize, size_t childCount>
	void insertHash(BitNode<keySize, childCount>* tree, size_t key, KeyValuePair* kvp) {
		BitNode<keySize, childCount>* current = tree;
		size_t i = 0;
		for (; i < BitNode<keySize, childCount>::bridgesSize; ++i)
		{
			//printf("key slice is %zu\n", key >> BitNode<keySize, childCount>::offsets.offsets[i]);
			size_t shifted = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
			//printf("slice %zu ", shifted);
			if (current->children[shifted] == nullptr) {
				current->children[shifted] = new BitNode<keySize, childCount>();
			} 
			current =  (BitNode<keySize, childCount>*)current->children[shifted];
		}
		size_t shiftedLast = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
		//printf("slice %zu\n", shiftedLast);
		KeyValueSpot* gotkv = (KeyValueSpot*)current->children[shiftedLast];
		if (gotkv == nullptr) {
			KeyValueSpot* newkvs = new KeyValueSpot();
			newkvs->kvp = kvp;
			current->children[shiftedLast] = newkvs;
		} else {
			FNTree::collCount += 1;
			while (gotkv != nullptr) {
				if (std::memcmp(gotkv->kvp->key, kvp->key, sizeof(kvp->key)) == 0) {
					gotkv->kvp->value = kvp->value;
					return;
				}
				gotkv = gotkv->next;
			}
			KeyValueSpot* newkvs = new KeyValueSpot();
			newkvs->kvp = kvp;
			gotkv = (KeyValueSpot*)current->children[shiftedLast];
			newkvs->next = gotkv;
			current->children[shiftedLast] = newkvs;
		}
	}

	template <size_t keySize, size_t childCount>
	void insertHashPart(BitNode<keySize, childCount>* tree, size_t key, KeyValuePair* kvp, size_t levels) {
		BitNode<keySize, childCount>* current = tree;
		size_t i = 0;
		for (; i < levels; ++i)
		{
			current = (BitNode<keySize, childCount>*)current->children[(key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift];
		}
		// now the lock part
		ScopedPartLock scoped((ParitionLock*)current->lock);
		for (; i < BitNode<keySize, childCount>::bridgesSize; ++i)
		{
			//printf("key slice is %zu\n", key >> BitNode<keySize, childCount>::offsets.offsets[i]);
			size_t shifted = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
			//printf("slice %zu ", shifted);
			if (current->children[shifted] == nullptr) {
				current->children[shifted] = new BitNode<keySize, childCount>();
			} 
			current =  (BitNode<keySize, childCount>*)current->children[shifted];
		}
		size_t shiftedLast = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
		//printf("slice %zu\n", shiftedLast);
		KeyValueSpot* gotkv = (KeyValueSpot*)current->children[shiftedLast];
		if (gotkv == nullptr) {
			KeyValueSpot* newkvs = new KeyValueSpot();
			newkvs->kvp = kvp;
			current->children[shiftedLast] = newkvs;
		} else {
			FNTree::collCount += 1;
			while (gotkv != nullptr) {
				if (std::memcmp(gotkv->kvp->key, kvp->key, sizeof(kvp->key)) == 0) {
					gotkv->kvp->value = kvp->value;
					return;
				}
				gotkv = gotkv->next;
			}
			KeyValueSpot* newkvs = new KeyValueSpot();
			newkvs->kvp = kvp;
			gotkv = (KeyValueSpot*)current->children[shiftedLast];
			newkvs->next = gotkv;
			current->children[shiftedLast] = newkvs;
		}
	}

	template <size_t keySize, size_t childCount>
	void* findInto(BitNode<keySize, childCount>* tree, size_t key) {
		BitNode<keySize, childCount>* current = tree;
		//printf("FIND offsetsize is %zu\n", BitNode<keySize, childCount>::offsetsSize);
		size_t i = 0;
		for (; i < BitNode<keySize, childCount>::bridgesSize; ++i)
		{
			size_t shifted = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
			if (current->children[shifted] == nullptr) {
				return nullptr;
			}
			current = (BitNode<keySize, childCount>*)current->children[shifted];
		}
		size_t shiftedLast = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
		return current->children[shiftedLast];
	}

	template <size_t keySize, size_t childCount>
	void* findHash(BitNode<keySize, childCount>* tree, size_t key, KeyValuePair* kvp) {
		BitNode<keySize, childCount>* current = tree;
		//printf("FIND offsetsize is %zu\n", BitNode<keySize, childCount>::offsetsSize);
		size_t i = 0;
		for (; i < BitNode<keySize, childCount>::bridgesSize; ++i)
		{
			size_t shifted = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
			if (current->children[shifted] == nullptr) {
				return nullptr;
			}
			current = (BitNode<keySize, childCount>*)current->children[shifted];
		}
		size_t shiftedLast = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
		KeyValueSpot* gotkv = (KeyValueSpot*)current->children[shiftedLast];
		while (gotkv != nullptr) {
			if (std::memcmp(gotkv->kvp->key, kvp->key, sizeof(kvp->key)) == 0) {
				return gotkv->kvp->value;
			}
			gotkv = gotkv->next;
		}
		return nullptr;
	}

	template <size_t keySize, size_t childCount>
	void* findHashPart(BitNode<keySize, childCount>* tree, size_t key, KeyValuePair* kvp, size_t levels) {
		BitNode<keySize, childCount>* current = tree;
		//printf("FIND offsetsize is %zu\n", BitNode<keySize, childCount>::offsetsSize);
		size_t i = 0;
		for (; i < levels; ++i)
		{
			current = (BitNode<keySize, childCount>*)current->children[(key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift];
		}
		// now the lock part
		ScopedPartLock scoped((ParitionLock*)current->lock);
		for (; i < BitNode<keySize, childCount>::bridgesSize; ++i)
		{
			size_t shifted = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
			if (current->children[shifted] == nullptr) {
				return nullptr;
			}
			current = (BitNode<keySize, childCount>*)current->children[shifted];
		}
		size_t shiftedLast = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
		KeyValueSpot* gotkv = (KeyValueSpot*)current->children[shiftedLast];
		while (gotkv != nullptr) {
			if (std::memcmp(gotkv->kvp->key, kvp->key, sizeof(kvp->key)) == 0) {
				return gotkv->kvp->value;
			}
			gotkv = gotkv->next;
		}
		return nullptr;
	}

	template <size_t keySize, size_t childCount>
	void* removeInto(BitNode<keySize, childCount>* tree, size_t key) {
		BitNode<keySize, childCount>* current = tree;
		size_t i = 0;
		for (; i < BitNode<keySize, childCount>::bridgesSize; ++i)
		{
			size_t shifted = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
			if (current->children[shifted] == nullptr) {
				return nullptr;
			} 
			current = (BitNode<keySize, childCount>*)current->children[shifted];
		}
		size_t shiftedLast = (key >> BitNode<keySize, childCount>::offsets.offsets[i]) & BitNode<keySize, childCount>::bitShift;
		void* got = current->children[shiftedLast];
		current->children[shiftedLast] = nullptr;
		return got;
	}

	struct MapObj {
		static constexpr size_t mapKeySize = 25;
		static constexpr size_t mapChildCount = 32;
		BitNode<mapKeySize, mapChildCount> _bnode;

		MapObj() {
			makeParitions<mapKeySize, mapChildCount>(&_bnode, 3);
		}

		void insert(KeyValuePair* kvp) {
			size_t hash_key = hashData(kvp->key, sizeof(kvp->key));
			//printf("got the hash %zu, bit cover %zu\n", hash_key & BIT_COVERS.shifts[mapKeySize], BIT_COVERS.shifts[mapKeySize]);
			//for (int i = 0; i < 16; ++i)
			//{
			//	printf("%u ", kvp->key[i]);
			//}
			//printf("\n");
			insertHashPart<mapKeySize, mapChildCount>(&_bnode, hash_key, kvp, 3);
		}

		void* find(KeyValuePair* kvp) {
			size_t hash_key = hashData(kvp->key, sizeof(kvp->key));
			return findHashPart<mapKeySize, mapChildCount>(&_bnode, hash_key, kvp, 3);
		}
	};
}

#endif // FORK_NUMBER_TREE_HEAD