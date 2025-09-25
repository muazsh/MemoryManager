[![CMake on multiple platforms](https://github.com/muazsh/MemoryManager/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/muazsh/MemoryManager/actions/workflows/cmake-multi-platform.yml)

# Memory Manager

This is a simple tool that enables detecting and cleaning memory leaks and detecting dangling pointers, the idea is that memory leaks take place when there are some allocations in the heap and there are no references in the stack directly or indirectly point to those allocatations, and the dangling pointer is a pointer which points to some already freed allocation.

This tool overloads `new` and `delete` operators to keep track of what is allocated and what is freed. 

Once memory leaks detection is triggered the stack gets scanned from bottom to top for each **allocated** pointer looking for a reference to it whether in the stack itself or in a heap reachable by other pointer in the stack directly or indirectly. Analog when dangling pointers detection is triggered the stack gets scanned from bottom to top for each **deleted** pointer looking for a reference to it whether in the stack itself or in a heap reachable by other pointer in the stack directly or indirectly.  

The tool can distinguish between leaks and indirect pointers (those pointers which have no references in the stack because they are pointers inside some other pointers in the heap).

# Usage:

- Somewhere in the program where you think memory leak took place call:
```c++
CollectGarbage();
```
`DetectMemoryLeak` function detects and prints out memory leak places in the code without calling `delete` on those leaks, so it can be used for profiling for example.

- Somewhere in the program where you think dangling pointer took place call:
```c++
DetectDanglingPointers();
```

# Limitations:
- The tool assumes a continuous stack memory space, which is not of C++ standard, but for most if not all compilers the stack is a whole and not fragmented.
- Due to C++ runtime implementation where the last stack frame which should have been removed stands still in the stack, the tool might miss some leaks because it still can find references to those leaks in the stack, same for dangling pointer where some reported dangling pointers might still be in some non-reachable stack frame, see the examples in main.cpp.
