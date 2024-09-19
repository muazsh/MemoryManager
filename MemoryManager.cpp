#include "MemoryManager.h"
#include <cstdlib>
#include <cstdio>
#include <cstdint>

struct Element
{
	void* m_ptr;
	bool m_isGarbage;
	char const* m_file;
	int m_line;
	std::size_t m_size;
	Element* m_next;
	Element() : m_ptr(nullptr),
		m_next(nullptr),
		m_isGarbage(true),
		m_file(nullptr),
		m_line(0),
		m_size(0) {}
};

Element* g_allocatedPointersHead = nullptr;
Element* g_allocatedPointersTail = nullptr;
Element* g_deletedPointersHead = nullptr;
Element* g_deletedPointersTail = nullptr;

void* g_stackTop = nullptr;

const char* g_newOperatorCallingFile = nullptr;
int g_newOperatorCallingLine = 0;

unsigned int GetAllocatedPointersCount()
{
	int counter = 0;
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		counter++;
		ite = ite->m_next;
	}
	return counter;
}

void* MyNew(std::size_t size)
{
	void* ptr = std::malloc(size);
	if (ptr)
	{
		Element* ptrElement = (Element*)std::malloc(sizeof(Element));
		if (ptrElement)
		{
			ptrElement->m_isGarbage = false;
			ptrElement->m_ptr = ptr;
			ptrElement->m_file = g_newOperatorCallingFile;
			ptrElement->m_line = g_newOperatorCallingLine;
			ptrElement->m_size = size;
			ptrElement->m_next = nullptr;
			if (g_allocatedPointersHead == nullptr)
			{
				g_allocatedPointersHead = ptrElement;
				g_allocatedPointersTail = ptrElement;
			}
			else
			{
				g_allocatedPointersTail->m_next = ptrElement;
				g_allocatedPointersTail = ptrElement;
			}
			return ptr;
		}
	}
	throw std::bad_alloc{};
}


#undef new

void* operator new(std::size_t size)
{
	return MyNew(size);
}

void* operator new[](std::size_t size)
{
	return MyNew(size);
}

// This macro got from https://stackoverflow.com/questions/619467/macro-to-replace-c-operator-new
// TODO: this macro has issues in a multi-threaded environment 
#define new (g_newOperatorCallingFile=__FILE__,g_newOperatorCallingLine=__LINE__) && false ? nullptr : new

void operator delete(void* p)
{
	auto ite1 = g_allocatedPointersHead;
	auto ite2 = g_allocatedPointersHead;
	if (ite1 != nullptr && ite1->m_ptr == p)
	{
		if (g_allocatedPointersHead == g_allocatedPointersTail)
		{
			g_allocatedPointersTail = g_allocatedPointersTail->m_next;
		}
		g_allocatedPointersHead = g_allocatedPointersHead->m_next;
		ite2 = nullptr;
	}
	else
	{
		while (ite1 != nullptr)
		{
			ite1 = ite1->m_next;
			if (ite1 != nullptr && ite1->m_ptr == p)
			{
				ite2->m_next = ite1->m_next;
				if (g_allocatedPointersTail == ite1)
					g_allocatedPointersTail = ite2;
				break;
			}
			ite2 = ite2->m_next;
		}
	}

	if (ite1 != nullptr) { // move deleted pointer element from allocated to deleted list.
		ite1->m_next = nullptr;
		if (g_deletedPointersHead == nullptr) {
			g_deletedPointersHead = g_deletedPointersTail = ite1;
		}
		else if (g_deletedPointersHead->m_next == nullptr) {
			g_deletedPointersHead->m_next = g_deletedPointersTail = ite1;
		}
		else {
			g_deletedPointersTail->m_next = ite1;
			g_deletedPointersTail = ite1;
		}
	}

	free(p);
}

void operator delete[](void* p)
{
	delete(p);
}

void ResetAllocatedPointers()
{
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		ite->m_isGarbage = true;
		ite = ite->m_next;
	}
}

void DetectDanglingPointers() {
	int dummy = 0;
	dummy++;
	void* stackBottom = &dummy;
	auto stackScanner = stackBottom;
	while (stackScanner < g_stackTop)
	{
		auto ite = g_deletedPointersHead;
		while (ite != nullptr)
		{
			if (*static_cast<int*>(stackScanner) == (int)reinterpret_cast<std::intptr_t>(ite->m_ptr))
			{
				printf("Potential dangling pointer of the pointer allocated in file %s at line %d\n", ite->m_file, ite->m_line);
				break;
			}
			ite = ite->m_next;
		}

		stackScanner = static_cast<char*>(stackScanner) + 1;
	}

	auto ite = g_deletedPointersHead;
	while (ite != nullptr)
	{
		auto ite2 = g_allocatedPointersHead;
		while (ite2 != nullptr)
		{
			int i = 0;
			int* allocatedHeapScanner = (int*)ite2->m_ptr;
			while (i < ite2->m_size)
			{
				if ((int)*(allocatedHeapScanner + i) == (int)reinterpret_cast<std::intptr_t>(ite->m_ptr))
				{
					printf("Potential dangling pointer of the pointer allocated in file %s at line %d\n", ite->m_file, ite->m_line);
					break;
				}
				i++;
			}
			ite2 = ite2->m_next;
		}
		ite = ite->m_next;
	}
}

void DetectMemoryLeak()
{
	ResetAllocatedPointers();
	int dummy = 0;
	void* stackBottom = &dummy;
	auto stackScanner = stackBottom;
	while (stackScanner < g_stackTop)
	{
		auto ite = g_allocatedPointersHead;
		while (ite != nullptr)
		{
			if (*static_cast<int*>(stackScanner) == (int)reinterpret_cast<std::intptr_t>(ite->m_ptr))
			{
				ite->m_isGarbage = false; // pointer is reachable.
				break;
			}
			ite = ite->m_next;
		}

		stackScanner = static_cast<char*>(stackScanner) + 1;
	}

	auto ite = g_allocatedPointersHead;
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
					int* allocatedHeapScanner = (int*)ite2->m_ptr;
					while (i < ite2->m_size)
					{
						if ((int)*(allocatedHeapScanner + i) == (int)reinterpret_cast<std::intptr_t>(ite->m_ptr))
						{
							ite->m_isGarbage = false; // pointer is reachable. 
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
				ite2 = ite2->m_next;
			}
		}
		if (iteFixed)
		{
			ite = g_allocatedPointersHead;
			continue;
		}
		ite = ite->m_next;
	}

	ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		if (ite->m_isGarbage)
		{
			printf("Memory leak detected of pointer allocated in file %s at line %d\n", ite->m_file, ite->m_line);
		}
		ite = ite->m_next;
	}
}

unsigned int CollectGarbage()
{
	DetectMemoryLeak();
	unsigned int count = 0;
	auto ite = g_allocatedPointersHead;
	while (ite != nullptr)
	{
		if (ite->m_isGarbage)
		{
			count++;
			auto next = ite->m_next;
			delete ite->m_ptr;
			ite = next;
		}
		else
		{
			ite = ite->m_next;
		}
	}
	return count;
}

void ResetAllocationList()
{
	while (g_allocatedPointersHead != nullptr)
	{
		delete g_allocatedPointersHead->m_ptr;
	}
}
