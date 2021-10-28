#pragma once
#ifndef MEMORY_LEAK_MANAGER
#define MEMORY_LEAK_MANAGER

#include <new>
#include <cstdlib>

struct Element
{
	void* m_ptr; 
	bool m_isGarbage;
	char const* m_file; 
	int m_line;
	size_t m_size;
	Element* next;
	Element() { m_ptr = nullptr; next = nullptr; m_isGarbage = true; m_file = nullptr; m_line = 0; m_size = 0; }
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
void* operator new(size_t size, char const* file, int line)
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
			ptrElement->m_file = file;
			ptrElement->m_line = line;
			ptrElement->m_size = size;
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

#define new new(__FILE__, __LINE__)

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
			if (*static_cast<int*>(stackScanner) == (int)ite->m_ptr)
			{
				ite->m_isGarbage = false;
				break;
			}

			stackScanner = static_cast<char*>(stackScanner) + sizeof(stackScanner);
		}
		ite = ite->next;
	}

	ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		bool iteFixed = false;
		if (ite->m_isGarbage)
		{
			auto ite2 = g_allocatedPointersHead;
			while (ite2 != nullptr)
			{
				if (!ite2->m_isGarbage)
				{
					int i = 0;
					int* varScanner = (int*)ite2->m_ptr;
					while (i < ite2->m_size)
					{
						if ((int)*(varScanner + i) == (int)ite->m_ptr)
						{
							ite->m_isGarbage = false;
							break;
						}
						i++;
					}
					if (!ite->m_isGarbage)
					{
						iteFixed = true;
						break;
					}
				}
				ite2 = ite2->next;
			}
		}
		if (iteFixed)
		{
			ite = g_allocatedPointersHead;
			continue;
		}
		ite = ite->next;
	}

	ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		if (ite->m_isGarbage)
		{
			printf("Memory leak detected in file %s at line %d\n", ite->m_file, ite->m_line);
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

void ResetAllocationList()
{
	while (g_allocatedPointersHead != nullptr)
	{
		delete g_allocatedPointersHead->m_ptr;
	}
}

#endif