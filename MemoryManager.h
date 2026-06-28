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


template <typename T>
class LinkedList {
private:
    struct Node {
        T data;
        Node* next;

        Node(const T& value) : data(value), next(nullptr) {}
    };

public:
    Node* head = nullptr;

    LinkedList() = default;
    LinkedList(const LinkedList&) = default;
    LinkedList& operator=(const LinkedList&) = default;
    LinkedList(LinkedList&&) = default;
    LinkedList& operator=(LinkedList&&) = default;

    void push_front(const T& value) {
        Node* node = (Node*)std::malloc(sizeof(Node));
        node->data = value;
        node->next = head;
        head = node;
    }

    ~LinkedList() {
        Node* current = head;

        while (current) {
            Node* next = current->next;
            free(current);
            current = next;
        }

        head = nullptr;
    }
};

#endif