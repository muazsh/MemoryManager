cmake_minimum_required(VERSION 3.10) 
project(MemoryManager)                    

set(CMAKE_CXX_STANDARD 20)           
set(CMAKE_CXX_STANDARD_REQUIRED ON)  

add_executable(MemoryManager main.cpp MemoryManager.cpp MemoryManager.h)
