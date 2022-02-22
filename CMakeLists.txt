#
# set 
# - AXIDMA_INC_DIR for include directory
#

cmake_minimum_required(VERSION 3.13)

#project(axidma VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Wno-unused-result")

set(AXIDMA_INC_DIR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_library(axidma STATIC ${SRC_FILES})
target_include_directories(axidma PUBLIC ${AXIDMA_INC_DIR})
