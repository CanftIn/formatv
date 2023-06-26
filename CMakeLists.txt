cmake_minimum_required(VERSION 3.20)
project(
  formatv
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE "Debug")

# temp define: https://discourse.llvm.org/t/python-api-problem/945
add_compile_options(-fno-rtti)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # for debug of stl structure while using clang compile
  add_compile_options(-fstandalone-debug)
endif()

## apt install llvm-15-dev libclang-15-dev
#set(LLVM_DIR /usr/lib/llvm-15/lib/cmake/llvm)
#set(Clang_DIR /usr/lib/llvm-15/lib/cmake/clang)
## manually install
## set(LLVM_DIR /usr/local/lib/cmake/llvm)
#
#find_package(LLVM REQUIRED CONFIG)
#find_package(Clang REQUIRED CONFIG)
#
#include_directories(${LLVM_INCLUDE_DIRS})
#add_definitions(${LLVM_DEFINITIONS})

include_directories(src)

add_executable(format_test main.cpp)
#target_link_libraries(format_test
#  LLVMSupport
#  LLVMOption
#)
