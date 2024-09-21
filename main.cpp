// MemoryLeakManagement.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "MemoryManager.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>

struct MyStruct { double x; double y; };

void MakeNoLeaks() {
	auto p1 = std::make_shared<int>();
	auto p2 = std::make_shared<int>();
	auto p3 = std::make_shared<int>();
	p1 = std::make_shared<int>();
	p2 = std::make_shared<int>();
	p3 = std::make_shared<int>();

	auto p4 = new MyStruct();
	delete p4;
	p4 = new MyStruct();
	delete p4;
	p4 = new MyStruct();
	delete p4;
}

void DetectNoLeaks() {
	ResetAllocatedPointers();
	MakeNoLeaks();
	auto count = CollectGarbage();
	std::cout << "\033[37;41m" << "In function " << std::string(__FUNCTION__) << " " << std::to_string(count) << " leaks are collected" << "\033[0m" << std::endl << std::endl;
}
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

int* GetDenaglingPtr() {
	auto ptr = new int(1);
	auto ptr1 = ptr;
	delete ptr;
	return ptr1;
}

MyStructWithPtr* GetDeepDanglingPtr() {
	auto ptr = new MyStructWithPtr();
	delete ptr->y;
	auto doublePtr = new double(10);
	ptr->y = doublePtr;
	delete doublePtr;
	return ptr;
}
void DanglingPointersDetection() {
	ResetAllocatedPointers();
	auto danglingPtr = GetDenaglingPtr();
	auto deepDanglingPtr = GetDeepDanglingPtr();
	DetectDanglingPointers();
}

void Detect1000LeaksInCalledFunction() {
	ResetAllocationList();
	Leak1000();
	auto count = CollectGarbage();
	std::cout << "\033[37;41m" << "In function " << std::string(__FUNCTION__) << " " << std::to_string(count) << " leaks are collected" << "\033[0m" << std::endl << std::endl;
}

void DetectLeaksInLoop() {
	ResetAllocationList();
	for (int i = 0; i < 100; i++)
	{
		auto st1 = new MyStruct();
		auto st2 = new MyStruct();
	}
	auto count = CollectGarbage();
	std::cout << "\033[37;41m"  << "In function " << std::string(__FUNCTION__) << " " << std::to_string(count) << " leaks are collected" << "\033[0m" << std::endl << std::endl;
}

void DetectLeaksInBlock() {
	ResetAllocationList();
	{
		auto st1 = new MyStruct();
		st1 = new MyStruct();
		auto st2 = new MyStruct();
		st2 = new MyStruct();
	}

	auto count = CollectGarbage();
	std::cout << "\033[37;41m" << "In function " + std::string(__FUNCTION__) + " " + std::to_string(count) + " leaks are collected" << "\033[0m" << std::endl << std::endl;
}

void DetectIndirectLeaks() {
	ResetAllocationList();
	LeakMyStructWithPtr(); // 2 leaks in here;
	MyStructWithPtr myStructWithPtr; // no leak despite allocation in default ctor.
	auto ptrMyStructWithPtr = new MyStructWithPtr(); // no leaks despite inner pointer is not in the stack rather in the heap.
	auto count = CollectGarbage();
	std::cout << "\033[37;41m" << "In function " << std::string(__FUNCTION__) << " " << std::to_string(count) << " leaks are collected" << "\033[0m" << std::endl << std::endl;
}

void DetectLeaksInThread() {
	ResetAllocationList();
	MyStruct* myStruct = nullptr;
	{
		std::thread([&myStruct] { myStruct = new MyStruct();
		new MyStruct(); /*leak*/ }).join();
	}

	auto count = CollectGarbage();
	std::cout << "\033[37; 41m" << "In function " << std::string(__FUNCTION__) << " " << std::to_string(count) << " leaks are collected" << "\033[0m" << std::endl << std::endl;

}

int main() {
	int dummy = 0;
	dummy++;
	g_stackTop = &dummy;

	DetectNoLeaks();
	DetectLeaksInLoop();
	DetectLeaksInBlock();
	Detect1000LeaksInCalledFunction();
	DetectIndirectLeaks();
	DetectLeaksInThread();

	DanglingPointersDetection();
}
