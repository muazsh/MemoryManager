cmake_minimum_required(VERSION 3.13) 
project(MemoryManager)                    

set(CMAKE_CXX_STANDARD 20)           
set(CMAKE_CXX_STANDARD_REQUIRED ON)  

add_executable(MemoryManager main.cpp MemoryManager.cpp MemoryManager.h)

# Add platform-specific dynamic symbol export flags
if(UNIX)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        if(APPLE)
            target_link_options(MemoryManager PRIVATE -Wl,-export_dynamic)
        else()
            target_link_options(MemoryManager PRIVATE -rdynamic)
        endif()
    endif()
endif()
