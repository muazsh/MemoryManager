#pragma once
#ifndef MEMORY_MANAGER
#define MEMORY_MANAGER

#include <new>

struct Element;
extern Element* g_allocatedPointersHead;
extern Element* g_allocatedPointersTail;
extern Element* g_deletedPointersHead;
extern Element* g_deletedPointersTail;
									   
extern void* g_stackTop; 

void* operator new(size_t size, char const* file, int line);
void* operator new[](size_t size, char const* file, int line);
#define new new(__FILE__, __LINE__)

void operator delete(void* p);
void operator delete[](void* p);

unsigned int GetAllocatedPointersCount();
void ResetAllocatedPointers();
void DetectDanglingPointers();
void DetectMemoryLeak();
unsigned int CollectGarbage();
void ResetAllocationList();

#endif