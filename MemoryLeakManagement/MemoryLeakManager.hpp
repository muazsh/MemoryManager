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

Element* s_allocatedPointersHead = nullptr;
Element* s_allocatedPointersTail = nullptr;
void* stackTop;

int GetAllocatedPointersCount()
{
	int counter = 0;
	auto ite = s_allocatedPointersHead;
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
			if (s_allocatedPointersHead == nullptr)
			{
				s_allocatedPointersHead = ptrElement;
				s_allocatedPointersTail = ptrElement;
			}
			else
			{
				s_allocatedPointersTail->next = ptrElement;
				s_allocatedPointersTail = ptrElement;
			}
			return ptr;
		}
	}
    throw std::bad_alloc{}; 
}

void operator delete(void* p)
{
	auto ite1 = s_allocatedPointersHead;
	auto ite2 = s_allocatedPointersHead;
	if (ite1 != nullptr && ite1->m_ptr == p)
	{
		if (s_allocatedPointersHead == s_allocatedPointersTail)
		{
			s_allocatedPointersTail = s_allocatedPointersTail->next;
		}
		s_allocatedPointersHead = s_allocatedPointersHead->next;
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
				if (s_allocatedPointersTail == ite1)
					s_allocatedPointersTail = ite2;
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
	auto ite = s_allocatedPointersHead;
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

	auto ite = s_allocatedPointersHead;
	while (ite != nullptr)
	{
		auto stackScanner = stackBottom;
		while (stackScanner < stackTop)
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
	auto ite = s_allocatedPointersHead;
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