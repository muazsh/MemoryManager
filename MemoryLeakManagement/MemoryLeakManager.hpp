#pragma once
#include <windows.h>

struct Element
{
	void* m_ptr; 
	bool m_isGarbage;
	Element* next;
	Element() { m_ptr = nullptr; next = nullptr; m_isGarbage = true; }
};

Element* s_allocatedPointersHead = nullptr;
Element* s_allocatedPointersTail = nullptr;

int counter = 0;

void* operator new(size_t size)
{
	counter++;
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
	counter--;
	auto ite1 = s_allocatedPointersHead;
	auto ite2 = s_allocatedPointersHead;
	if (ite1 != nullptr && ite1->m_ptr == p)
	{
		s_allocatedPointersHead = s_allocatedPointersHead->next;
		free(ite1);
		ite1 = nullptr;
		ite2 = nullptr;
		return;
	}
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
	free(p);
}

void ResetAllocatedPointerMap()
{
	auto ite = s_allocatedPointersHead;
	while (ite != nullptr)
	{
		ite->m_isGarbage = true;
		ite = ite->next;
	}
}

Element* DetectMemoryLeak()
{
	ResetAllocatedPointerMap();
	Element* resultsHead = nullptr;
	Element* resultsTail = nullptr;
	void* stackBottom;
	void* stackTop;
	void** teb = (void**)NtCurrentTeb();
	stackTop = teb[1];

	auto ite = s_allocatedPointersHead;
	while (ite != nullptr)
	{
		stackBottom = teb[2];
		while (stackBottom < stackTop)
		{
			if (*static_cast<int*>(stackBottom) == (int)ite->m_ptr)
			{
				ite->m_isGarbage = false;
				break;
			}

			stackBottom = static_cast<char*>(stackBottom) + sizeof(stackBottom);
		}
		ite = ite->next;
	}

	ite = s_allocatedPointersHead;
	while (ite != nullptr)
	{
		if (ite->m_isGarbage)
		{
			if (resultsHead == nullptr)
			{
				resultsHead = ite;
				resultsTail = ite;
			}
			else
			{
				resultsTail->next = ite;
				resultsTail = ite;
			}

		}
		ite = ite->next;
	}
	return resultsHead;
}

void CollectGarbage()
{
	auto garbageList = DetectMemoryLeak();
	while (garbageList != nullptr)
	{
		auto next = garbageList->next;		
		delete[] garbageList->m_ptr;
		garbageList = next;
	}
}