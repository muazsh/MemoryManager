// MemoryLeakManagement.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <assert.h>
#include "MemoryLeakManager.hpp"

struct MyStruct { double x; double y; };

void Leak1000()
{
	for (int i = 0; i < 1000; i++)
		auto st = new MyStruct();
}

struct MyStructWithPtr
{
	double x; double* y;
	MyStructWithPtr()
	{
		x = 10;
		y = new double(5);
	}
};


void LeakMyStructWithPtr()
{
	auto ptr = new MyStructWithPtr();
}

int main()
{
	int dummy = 0;
	g_stackTop = &dummy;

	/*
	* For all compilers:
	* When garbage took place in a called function, all garbage are collected by CollectGarbage.
	*/
	Leak1000();
	assert(1000 == GetAllocatedPointersCount());
	CollectGarbage();
	assert(0 == GetAllocatedPointersCount());

///////////////////////////////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < 100; i++)
	{
		auto st1 = new MyStruct();
		auto st2 = new MyStruct();
	}
	assert(200 == GetAllocatedPointersCount());
	CollectGarbage();

#ifdef _WIN64
	assert(0 == GetAllocatedPointersCount());
#elif _WIN32
	/*
	* For MSVC x86 compiler:
	* When garbage took place in the current function in loops, all garbage are collected by CollectGarbage
	* except of the one of the last iteration
	*/
	assert(2 == GetAllocatedPointersCount());
	ResetAllocationList();
#endif // _WIN64

#ifndef _WIN32
	/*
	* For GCC and Clang CollectGarbage detects this kind of leaks.
	*/
	assert(0 == GetAllocatedPointersCount());
#endif // !_WIN32

///////////////////////////////////////////////////////////////////////////////////////////////////
	{
		auto st1 = new MyStruct();
		st1 = new MyStruct();
		auto st2 = new MyStruct();
		st2 = new MyStruct();
	}
	assert(4 == GetAllocatedPointersCount());
	CollectGarbage();
#ifdef _WIN64
	assert(0 == GetAllocatedPointersCount());
#elif _WIN32
	/*
	* For MSVC x86 compiler:
	* When garbage took place in the current function in a block, CollectGarbage detects no leak.
	*/
	assert(4 == GetAllocatedPointersCount());
	CollectGarbage();
	ResetAllocationList();
#endif // _WIN64

#ifndef _WIN32
	/*
	* For GCC and Clang CollectGarbage detects this kind of leaks.
	*/
	assert(0 == GetAllocatedPointersCount());
#endif // !_WIN32

/////////////////////////////////////////////////////////////////////////////////////////////////////
	// Indirect pointers.
	LeakMyStructWithPtr(); // 2 leaks in here;
	MyStructWithPtr myStructWithPtr; // no leak despite allocation in default ctor.
	auto ptrMyStructWithPtr = new MyStructWithPtr(); // no leaks despite inner pointer is not in the stack rather in the heap.
	assert(5 == GetAllocatedPointersCount());
#ifdef _WIN64
#elif _WIN32
	CollectGarbage();
	assert(3 == GetAllocatedPointersCount());
#endif // !_WIN32
}
