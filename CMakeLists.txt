cmake_minimum_required(VERSION 3.4)
project(aauto)

#set(CMAKE_C_COMPILER "gcc-mp-5")
#set(CMAKE_CXX_COMPILER "g++-mp-5")

# The path for our own CMake modules
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -funsigned-char")

find_package(LibUSB REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(AVCodec REQUIRED)

set(SOURCE_FILES src/main.cpp src/transport.cpp src/transport.h src/utils.h src/utils.cpp src/scope_guard.h src/crypto.h src/crypto.cpp src/proto.cpp src/proto.h src/aa_helpers.cpp src/aa_helpers.h src/decoder.cpp src/decoder.h)
add_executable(aauto ${SOURCE_FILES})

include_directories(${PROJECT_SOURCE_DIR}/src ${LibUSB_INCLUDE_DIRS} ${SDL2_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR} ${LIBACODEC_INCLUDE_DIRS})
target_link_libraries(aauto ${LibUSB_LIBRARIES} ${SDL2_LIBRARY}
        ${OPENSSL_LIBRARIES} ${LIBAVCODEC_LIBRARIES})
