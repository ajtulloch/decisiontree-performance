cmake_minimum_required (VERSION 2.8) 
project (DECISION_TREE_PERF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules")

# Packages
find_package(Boost 1.54 COMPONENTS filesystem chrono log thread system REQUIRED)
find_package(GFlags)

include_directories(${Boost_INCLUDE_DIR} ${GFLAGS_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "-Wall -std=c++11")

# perf executable

set(PERF_SRCS util.cpp main.cpp)
add_executable(perf ${PERF_SRCS})
target_link_libraries(perf ${Boost_LIBRARIES} ${GFLAGS_LIBRARIES})

set(CMAKE_BUILD_TYPE Release)
