#pragma once
#ifndef MEMORY_LEAK_MANAGER
#define MEMORY_LEAK_MANAGER

#include <new>
#include <cstdlib>

struct Element
{
	void* m_ptr; 
	bool m_isGarbage;
	Element* next;
	Element() { m_ptr = nullptr; next = nullptr; m_isGarbage = true; }
};

Element* g_allocatedPointersHead = nullptr;
Element* g_allocatedPointersTail = nullptr;
void* g_stackTop;

int GetAllocatedPointersCount()
{
	int counter = 0;
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		counter++;
		ite = ite->next;
	}
	return counter;
}
void* operator new(size_t size)
{
	if (size == 0)
        ++size;
	void* ptr = std::malloc(size);
    if (ptr)
	{
		Element* ptrElement = (Element*)std::malloc(sizeof( Element));
		if (ptrElement)
		{
			ptrElement->m_isGarbage = false;
			ptrElement->m_ptr = ptr;
			ptrElement->next = nullptr;
			if (g_allocatedPointersHead == nullptr)
			{
				g_allocatedPointersHead = ptrElement;
				g_allocatedPointersTail = ptrElement;
			}
			else
			{
				g_allocatedPointersTail->next = ptrElement;
				g_allocatedPointersTail = ptrElement;
			}
			return ptr;
		}
	}
    throw std::bad_alloc{}; 
}

void operator delete(void* p)
{
	auto ite1 = g_allocatedPointersHead;
	auto ite2 = g_allocatedPointersHead;
	if (ite1 != nullptr && ite1->m_ptr == p)
	{
		if (g_allocatedPointersHead == g_allocatedPointersTail)
		{
			g_allocatedPointersTail = g_allocatedPointersTail->next;
		}
		g_allocatedPointersHead = g_allocatedPointersHead->next;
		free(ite1);
		ite1 = nullptr;
		ite2 = nullptr;
	}
	else
	{
		while (ite1 != nullptr)
		{
			ite1 = ite1->next;
			if (ite1 != nullptr && ite1->m_ptr == p)
			{
				ite2->next = ite1->next;
				if (g_allocatedPointersTail == ite1)
					g_allocatedPointersTail = ite2;
				free(ite1);
				ite1 = nullptr;
				break;
			}
			ite2 = ite2->next;

		}
	}
	free(p);
	p = nullptr;
}

void ResetAllocatedPointers()
{
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		ite->m_isGarbage = true;
		ite = ite->next;
	}
}

void DetectMemoryLeak()
{
	ResetAllocatedPointers();
	int dummy = 0;
	void* stackBottom = &dummy;

	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		auto stackScanner = stackBottom;
		while (stackScanner < g_stackTop)
		{
			if (*static_cast<long*>(stackScanner) == (long)ite->m_ptr)
			{
				ite->m_isGarbage = false;
				break;
			}

			stackScanner = static_cast<char*>(stackScanner) + sizeof(stackScanner);
		}
		ite = ite->next;
	}
}

void CollectGarbage()
{
	DetectMemoryLeak();
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		if (ite->m_isGarbage)
		{
			auto next = ite->next;
			delete[] ite->m_ptr;
			ite = next;
		}
		else
		{
			ite = ite->next;
		}
	}
}

#endif