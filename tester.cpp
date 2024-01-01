#include "fork-number-tree.h"

#include <chrono>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <functional>
#include <vector>
#include <map>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <thread>
#include <atomic>

using namespace std::chrono;

static std::vector<FNTree::KeyValuePair*> NUM_BANK;
static std::vector<std::string> STR_BANK;

#define BANK_TEST_SIZE 1000000

static void populateBank() {
	for (size_t i = 0; i < BANK_TEST_SIZE; ++i)
	{
		FNTree::KeyValuePair* newkvs = new FNTree::KeyValuePair();
		for (size_t i = 0; i < sizeof(newkvs->key); ++i)
		{
			newkvs->key[i] = (unsigned char)rand();
			newkvs->value = new int(rand());
		}
		NUM_BANK.push_back(newkvs);
	}
}

static void populateStrBank() {
	for (size_t i = 0; i < BANK_TEST_SIZE; ++i)
	{
		std::string foo;
		for (size_t i = 0; i < 16; ++i)
		{
			foo.push_back((char)rand());
		}
		STR_BANK.push_back(foo);
	}

}

static FNTree::MapObj aNode;
static std::unordered_map<std::string, void*> aMap;

void tester_map_func(void) {
	for (std::vector<std::string>::iterator i = STR_BANK.begin(); i != STR_BANK.end(); ++i)
	{
		aMap[*i] = (void*)i->c_str();
	}
}

void lookup_map_func(void) {
	size_t adder = 0;
	for (std::vector<std::string>::iterator i = STR_BANK.begin(); i != STR_BANK.end(); ++i)
	{
		void* found = aMap.find(*i)->second;
		adder += (size_t)found;

	}
	printf("adder %zu\n", adder);
}


void tester_func(void) {
	for (std::vector<FNTree::KeyValuePair*>::iterator i = NUM_BANK.begin(); i != NUM_BANK.end(); ++i)
	{
		//printf("INSERTING KEY ");
		//(*i)->printKey();
		//printf("TO VALUE ");
		//(*i)->printValue();
		//FNTree::insertInto(&aNode, *i, (void*)*i);
		aNode.insert(*i);
	}
}

void tester_func_spec(const std::vector<FNTree::KeyValuePair*>& numbers) {
	for (auto i = numbers.begin(); i != numbers.end(); ++i)
	{
		//printf("INSERTING KEY ");
		//(*i)->printKey();
		//printf("TO VALUE ");
		//(*i)->printValue();
		//FNTree::insertInto(&aNode, *i, (void*)*i);
		aNode.insert(*i);
	}
}

void deleter_func(void) {
	for (std::vector<FNTree::KeyValuePair*>::iterator i = NUM_BANK.begin(); i != NUM_BANK.end(); ++i)
	{
		//size_t result = removeFull(&aNode, *i);
		//printf("removed %zu nodes\n", result);
	}
}

void lookup_func(void) {
	size_t adder = 0;
	for (std::vector<FNTree::KeyValuePair*>::iterator i = NUM_BANK.begin(); i != NUM_BANK.end(); ++i)
	{
		void* found = aNode.find(*i);
		//printf("LOOK UP KEY ");
		//(*i)->printKey();
		//printf("TO VALUE %d\n", *(int*)(found));
		adder += (size_t)found;
	}
	printf("adder %zu\n", adder);
}

void lookup_func_spec(const std::vector<FNTree::KeyValuePair*>& numbers) {
	size_t adder = 0;
	for (auto i = numbers.begin(); i != numbers.end(); ++i)
	{
		void* found = aNode.find(*i);
		//printf("LOOK UP KEY ");
		//(*i)->printKey();
		//printf("TO VALUE %d\n", *(int*)(found));
		adder += (size_t)found;
	}
	printf("adder %zu\n", adder);
}

void time_function(const char* name, std::function<void(void)> func, unsigned times) {
	auto start = high_resolution_clock::now();
	for (unsigned i = 0; i < times; ++i)
	{
		func();
	}
	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(stop - start);
	std::printf("%s -> US %llu\n", name, duration.count());
}

void mt_tester(void) {
	static constexpr size_t threadCount = 8;
	std::thread tpool[threadCount];
	static std::atomic<bool> waitForStart = true;

	for (size_t i = 0; i < threadCount; ++i)
	{
		tpool[i] = std::thread([&]{
			std::vector<FNTree::KeyValuePair*> myCopy = NUM_BANK;
			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle(myCopy.begin(), myCopy.end(), g);
			while (waitForStart.load()) {

			}
			auto start = high_resolution_clock::now();
			lookup_func_spec(myCopy);
			auto stop = high_resolution_clock::now();
			auto duration = duration_cast<microseconds>(stop - start);
			std::printf("%s -> US %llu\n", "threaded lookup", duration.count());

		});
	}
	waitForStart.store(false);

	for (size_t j = 0; j < threadCount; ++j)
	{
		tpool[j].join();
	}
}

/*
PERF BOOST -O3
18 bit 64 children
insert bit test -> US 2941
mem used 2130432
adder 1073064323333195
ilookup bit test -> US 1791
insert map test -> US 148122
adder 1073806376451147
ilookup map test -> US 19345
*/


static void perfTesting() {
	populateBank();
	populateStrBank();
	time_function("FNT insert test", tester_func, 1);
	printf("mem used %zu\n", FNTree::totalMemUsed);
	printf("coll used %zu\n", FNTree::collCount);
	time_function("FNT lookup test", lookup_func, 1);
	//deleter_func();

	time_function("std::unordered_map insert map test", tester_map_func, 1);
	time_function("std::unordered_map lookup map test", lookup_map_func, 1);

	time_function("FNT Multi-Threaded lookup test", mt_tester, 1);
}

int main(int argc, char const *argv[])
{
	perfTesting();
	
	return 0;
}