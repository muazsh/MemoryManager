[![CMake on multiple platforms](https://github.com/muazsh/MemoryManager/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/muazsh/MemoryManager/actions/workflows/cmake-multi-platform.yml)

# Memory Manager

This is a simple tool that enables detecting and cleaning memory leaks and detecting dangling pointers, the idea is that memory leaks take place when there are some allocations in the heap and there are no references in the stack directly or indirectly point to those allocatations, and the dangling pointer is a pointer which points to some already freed allocation.

This tool overloads `new` and `delete` operators to keep track of what is allocated and what is freed. 

Once memory leaks detection is triggered the stack gets scanned from bottom to top for each **allocated** pointer looking for a reference to it whether in the stack itself or in a heap reachable by other pointer in the stack directly or indirectly. Analog when dangling pointers detection is triggered the stack gets scanned from bottom to top for each **deleted** pointer looking for a reference to it whether in the stack itself or in a heap reachable by other pointer in the stack directly or indirectly.  

The tool can distinguish between leaks and indirect pointers (those pointers which have no references in the stack because they are pointers inside some other pointers).

# Usage:
- First of all when the program begins or when you still think no leak has been occured the stack top address should be obtained like:
```c++
int dummy = 0;
dummy++; // preventing optimizing away.
g_stackTop = &dummy; // g_stackTop is a global variable declared in MemeoryLeakManager.hpp
```
- Somewhere in the program where you think memory leak took place, in main thread after making sure worker threads are idle call:
```c++
CollectGarbage();
```
`DetectMemoryLeak` function detects and prints out memory leak places in the code without calling `delete` on those leaks, so it can be used for profiling for example.
- Somewhere in the program where you think dangling pointer took place, in main thread after making sure worker threads are idle call call:
```c++
DetectDanglingPointers();
```

# Limitations:
- The tool assumes a continuous stack memory space, which is not of C++ standard, but for most if not all compilers the stack is a whole and not fragmented.
- Due to C++ runtime implementation where the last stack frame which should have been removed stands still in the stack, the tool might miss some leaks because it still can find references to those leaks in the stack, see the examples in main.cpp.
