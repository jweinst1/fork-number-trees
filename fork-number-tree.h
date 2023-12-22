#ifndef FORK_NUMBER_TREE_HEAD
#define FORK_NUMBER_TREE_HEAD

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <climits>

namespace FNtree {
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
		}
		return (size_t)-1;
	}

	static size_t totalMemUsed;

	template <size_t keySize, size_t childCount>
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

		BitNode() { totalMemUsed += sizeof(BitNode);}

		static_assert(getBitCount(childCount) != (size_t)-1);
		static constexpr size_t bitCount = getBitCount(childCount);
		static_assert(keySize % bitCount == 0);
		static constexpr size_t bitIndex = bitCount - 1;
		static constexpr size_t bitShift = BIT_COVERS.shifts[bitIndex];
		static constexpr OffSetArr<keySize / bitCount, bitCount> offsets;
		static constexpr size_t offsetsSize = sizeof(offsets.offsets) / sizeof(offsets.offsets[0]);
		static constexpr size_t bridgesSize = offsetsSize - 1;
		//void* data = nullptr;
		void* children[childCount] = {nullptr};
	};

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
}

#endif // FORK_NUMBER_TREE_HEAD