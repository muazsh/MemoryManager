// MemoryLeakManagement.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include "MemoryLeakManager.hpp"

struct MyStruct { int x; double y; };

void dummy()
{
	int* pz =  new int(9);
	//s_allocatedPointers[&pz] = pz;

	MyStruct* st = new MyStruct();
	//s_allocatedPointers[&st] = st;
}


int main()
{
	double* px = new double(1);
	//s_allocatedPointers[&px] = px;
	int* py = new int(10);
	CollectGarbage();
	//s_allocatedPointers[&py] = py;
	//dummy();
	/*void* stackBottom,
		* stackTop;
	void** teb = (void**)NtCurrentTeb();
	stackBottom = teb[2];
	stackTop = teb[1];

	while (stackBottom < stackTop)
	{
		if (s_allocatedPointers.find(stackBottom) != s_allocatedPointers.end())
		{
			if(*static_cast<long*>(stackBottom) == (long)s_allocatedPointers[stackBottom])
				std::cout << "found" << std::endl;
		}
		stackBottom = static_cast<char*>(stackBottom) + 1;
	}*/
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
