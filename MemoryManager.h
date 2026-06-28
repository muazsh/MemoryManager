#pragma once
#ifndef MEMORY_MANAGER
#define MEMORY_MANAGER

#include <new>
#include <mutex>

struct Element;
extern Element* g_allocatedPointersHead;
extern Element* g_allocatedPointersTail;
extern Element* g_deletedPointersHead;
extern Element* g_deletedPointersTail;

extern std::mutex g_alloc_dealloc_mtx;

void* operator new(std::size_t size);
void* operator new[](std::size_t size);

void operator delete(void* p);
void operator delete[](void* p);

void ResetAllocatedPointers();
void DetectDanglingPointers();
void DetectMemoryLeak();
unsigned CollectGarbage();
void ResetAllocationList();
bool IsAssignedToGlobalOrStatic(const void* p);

#endif