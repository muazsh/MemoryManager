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

extern const char* g_newOperatorCallingFile;
extern int g_newOperatorCallingLine;

void* operator new(std::size_t size);
void* operator new[](std::size_t size);

void* MyNew(std::size_t size);

#ifdef WIN32
void* operator new(std::size_t size, bool flag, const char* file, int line);
// TODO: this macro has issues in a multi-threaded environment 
#define new new(false, g_newOperatorCallingFile=__FILE__,g_newOperatorCallingLine=__LINE__) int() != nullptr? nullptr : new
#endif

void operator delete(void* p);
void operator delete[](void* p);

unsigned int GetAllocatedPointersCount();
void ResetAllocatedPointers();
void DetectDanglingPointers();
void DetectMemoryLeak();
unsigned int CollectGarbage();
void ResetAllocationList();

#endif