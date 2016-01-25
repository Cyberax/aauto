cmake_minimum_required(VERSION 3.4)
project(aauto)

# The path for our own CMake modules
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -funsigned-char")

find_package(LibUSB REQUIRED)
find_package(FLTK REQUIRED)

set(SOURCE_FILES src/main.cpp src/transport.cpp src/transport.h src/utils.h src/utils.cpp src/scope_guard.h)
add_executable(aauto ${SOURCE_FILES})

include_directories(${PROJECT_SOURCE_DIR}/src ${LibUSB_INCLUDE_DIRS} ${FLTK_INCLUDE_DIR})
target_link_libraries(aauto ${LibUSB_LIBRARIES} ${FLTK_LIBRARIES})
