[![CMake on multiple platforms](https://github.com/muazsh/MemoryManager/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/muazsh/MemoryManager/actions/workflows/cmake-multi-platform.yml)

# Memory Manager

This tool enables detecting and cleaning memory leaks and detecting dangling pointers, where memory leaks take place when there are some allocations in the heap and there are no references in the stack or in the Data or BSS segments directly or indirectly point to those allocatations, and the dangling pointer is a reachable pointer which points to some already freed allocation.

## Reachability

The tool considers thread stacks and Data segement and BSS segment a root for reachability, so pointers which are referenced in that root are considered reachable, also those pointers stored in the heap which are reachable via a reachable pointer are also reachable.


```text
                    

STACK                                                Data Segment                      Allocation List                        
┌──────────────────────┐                        ┌──────────────────────┐           ┌──────────────────────┐    
│         ...          │                        │         ...          │           │ ptr1 0x10001024      │ Reachable from Data segment
├──────────────────────┤                        ├──────────────────────┤           ├──────────────────────┤
│ ptr0 = 0x10001000    │        ┌──────────────── ptr1 = 0x10001024    │           │      0x10000012      │ Not Reachable (Memory Leak)
├──────────────────────┤        │               ├──────────────────────┤           ├──────────────────────┤
│ ptr2 = 0x10002040    │        │               │         ...          │           │ ptr2 0x10002040      │ Reachable from the stack
├────────│─────────────┤        │               └──────────────────────┘           ├──────────────────────┤
│        │ ...         │        │                                                  │ ptr3 0x100A4080      │ Reachable from the heap
└────────│─────────────┘        │                                                  ├──────────────────────┤
         │                      │                                                  │         ...          │
         │                      │                                                  └──────────────────────┘
         │                      │                         HEAP                         Deallocation List
         │                      ▼                ┌──────────────────────┐          ┌──────────────────────┐
         │              0x10001024 ───────────►  │ value = 10           │          │ ptr0 0x10001000      │ Reachable from the stack (Dangling Pointer)
         │                                       ├──────────────────────┤          ├──────────────────────┤
         │                                       │         ...          │          │         ...          │
         │                                       ├──────────────────────┤          └──────────────────────┘
         └───────────►  0x10002040 ───────────►  │ value = 1337         │
                                                 ├──────────────────────┤
                                ┌───────────────── ptr3 = 0x100A4080    │
                                │                ├──────────────────────┤
                                │                │         ...          │
                                ▼                ├──────────────────────┤
                        0x100A4080 ───────────►  │ value = 500          │
                                                 └──────────────────────┘
                         
```

## Methodology

This tool overloads `new` and `delete` operators to keep track of the allocated pointers in the **allocation list** and the freed pointers in the **deallocation list**.

### Memory Leak

A memory leak takes place if a pointer in the **allocation list** but is **not reachable**.

### Dangling Pointer

A pointer is a dangling pointer if it is in the **deallocation list** but is **reachable**.

### Garbage Collection

The tool poriveds a function to free the deteced memory leaks.

## How It Works

Once memory leaks detection is triggered the stack of each thread gets scanned from top to bottom for each **allocated** pointer looking for a reference to it whether in the stack itself or in a heap reachable by other pointer in the stack directly or indirectly, likewise Data and BSS segments get scanned. 

Analog when dangling pointers detection is triggered the stacks get scanned for each **deleted** pointer looking for a reference to it whether in the stacks or in a heap reachable via other pointers reachable from the stacks directly or indirectly.  

## Usage:

- Somewhere in the program where you think memory leak took place, call:
```c++
DetectMemoryLeak();
```
`DetectMemoryLeak` function detects and prints out memory leak allocation places in the code without calling `delete` on those leaks, so it can be used for profiling for example. On the other hand calling `CollectGarbage` will detect leaks and call `delete` on the detected leaks.

- Somewhere in the program where you think dangling pointer took place, call:
```c++
DetectDanglingPointers();
```
The tool will report those deleted pointers but still reachable via the reachability process.

## Note
- While detecting memory leaks or dangling pointers, the tool holds a global lock to prevent allocating/deallocating heap memory, so the threads which need to do so will get suspended until the process is done. However; the threads are free to use there stacks which in the end wont affect the accurcy of the results.

## Limitations:
- The tool assumes a continuous stack memory space, which is not of C++ standard, but for most if not all compilers the stack is a whole and not fragmented.
- Due to C++ runtime implementation where the last stack frame which should have been removed stands still in the stack, the tool might miss some leaks because it still can find references to those leaks in the stack (False Negative), same for dangling pointer where some reported dangling pointers might still be in the recent stack frame (False Positive). However, both cases are limited to the very recently refernced pointers in the very recent stack frame which should be unwinded and removed already, see the examples in main.cpp.
