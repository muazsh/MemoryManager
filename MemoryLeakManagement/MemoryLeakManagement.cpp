// MemoryLeakManagement.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include "MemoryLeakManager.hpp"

struct MyStruct { int x; double y; };

void Leak()
{
	MyStruct* st1 = new MyStruct();
	st1 = new MyStruct();
	st1 = new MyStruct();
	st1 = new MyStruct();
	delete st1;
}


int main()
{
	int dummy = 0;
	stackTop = &dummy;
	{
		MyStruct* st = new MyStruct();
		st = new MyStruct();
		st = new MyStruct();
		delete st;
	}
	MyStruct* st1 = new MyStruct();
	MyStruct* st2 = new MyStruct();
	MyStruct* st3 = new MyStruct();
	MyStruct* st4 = new MyStruct();
	Leak();
	CollectGarbage();
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
