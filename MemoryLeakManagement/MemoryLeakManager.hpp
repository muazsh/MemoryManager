#pragma once
#include <map>
#include <list>
#include <windows.h>

enum class PointerType { Pointer, Array };

struct Element
{
	void** m_ref;
	void* m_ptr; 
	PointerType m_ptrType; 
	bool m_isGarbage;
	Element* next;
	Element() { m_ptr = nullptr; m_ref = nullptr;  m_ptrType = PointerType::Pointer; m_isGarbage = true; }
};

//std::map<void*, Element> s_allocatedPointers = {};

Element* s_allocatedPointersHead = nullptr;
Element* s_allocatedPointersTail = nullptr;

void* operator new(size_t size)
{
	char x = 0;
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
			ptrElement->m_ref = &ptr;
			ptrElement->m_ptrType = size <= 8 ? PointerType::Pointer : PointerType::Array;
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
			void** teb = (void**)NtCurrentTeb();
			return ptr;
		}
	}
    throw std::bad_alloc{}; 
}

void operator delete(void* p)
{
	/*void* key = nullptr;
	for (auto& elem : s_allocatedPointers)
		if (elem.second.m_ptr == p)
			key = elem.first;
	if (key != nullptr)
		s_allocatedPointers.erase(key);
		*/
	auto ite1 = s_allocatedPointersHead;
	auto ite2 = s_allocatedPointersHead;
	while (ite1 != nullptr)
	{
		ite1 = ite1->next;
		if (ite1 != nullptr && ite1->m_ptr == p)
		{
			ite2->next = ite1->next;
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
	//for (auto& elem : s_allocatedPointers)
		//elem.second.m_isGarbage = true;
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
	stackBottom = teb[2];
	stackTop = teb[1];

	while (stackBottom < stackTop)
	{
		auto ite = s_allocatedPointersHead;
		while (ite != nullptr)
		{
			if (ite->m_ref == stackBottom)
				break;
			ite = ite->next;
		}
		//if (s_allocatedPointers.find(stackBottom) != s_allocatedPointers.end())
		if(ite != nullptr)
		{
			//if (*static_cast<long*>(stackBottom) == (long)s_allocatedPointers[stackBottom].m_ptr)
			    //s_allocatedPointers[stackBottom].m_isGarbage = false;
			if (*static_cast<long*>(stackBottom) == (long)ite->m_ptr)
				ite->m_isGarbage = false;
		}
		stackBottom = static_cast<char*>(stackBottom) + 1;
	}

	/*for (auto& elem : s_allocatedPointers)
		if (elem.second.m_isGarbage)
			results.push_back(elem.second);
			*/
	auto ite = s_allocatedPointersHead;
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

Element* CollectGarbage()
{
	auto garbageList = DetectMemoryLeak();
	/*for (auto& garbage : garbageList)
	{
		if (garbage->m_ptrType == PointerType::Pointer)
			delete garbage->m_ptr;
		else
			delete[] garbage->m_ptr;
	}*/
	while (garbageList != nullptr)
	{
		if (garbageList->m_ptrType == PointerType::Pointer)
			delete garbageList->m_ptr;
		else
			delete[] garbageList->m_ptr;
		garbageList = garbageList->next;
	}
	return garbageList;
}