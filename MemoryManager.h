#pragma once
#ifndef MEMORY_MANAGER
#define MEMORY_MANAGER

#include <new>

struct Element;
extern Element* g_allocatedPointersHead;
extern Element* g_allocatedPointersTail;
extern Element* g_deletedPointersHead;
extern Element* g_deletedPointersTail;

extern const char* g_newOperatorCallingFile;
extern int g_newOperatorCallingLine;

void* operator new(std::size_t size);
void* operator new[](std::size_t size);

void operator delete(void* p);
void operator delete[](void* p);

void ResetAllocatedPointers();
void DetectDanglingPointers();
void DetectMemoryLeak();
unsigned CollectGarbage();
void ResetAllocationList();

#endif