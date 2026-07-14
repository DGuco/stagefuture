cmake_minimum_required(VERSION 3.6)
project(stagefuture)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/run)

if(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
    add_compile_options(-g -std=c++11 -O2 -fPIC -Wall -pipe -rdynamic -fno-omit-frame-pointer -ldl)
endif()

# 仅对 Visual Studio 编译器生效
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

set(SOURCE_FILES
    "test/main.cpp" )

file(GLOB_RECURSE FRAMEWORK_HEADER_FILES 
    "core/*.h"
    "core/*.cpp"
    )

file(GLOB_RECURSE BASE_HEADER_FILES 
    "BASE/*.h"
    "BASE/*.cpp"
    )

list(APPEND SOURCE_FILES ${BASE_HEADER_FILES})
list(APPEND SOURCE_FILES ${FRAMEWORK_HEADER_FILES})

include_directories(
        ${CMAKE_SOURCE_DIR}/core
        ${CMAKE_SOURCE_DIR}/base
)

link_directories(
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
        /usr/lib
        /usr/local/lib
        ${CMAKE_SOURCE_DIR}/lib
        ${CMAKE_SOURCE_DIR}/lib/linux
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
        C:/Windows/System32
        ${CMAKE_SOURCE_DIR}/lib
        ${CMAKE_SOURCE_DIR}/lib/win64
        ${CMAKE_SOURCE_DIR}/lib/Debug
endif()
)

add_executable(stagefuture_test ${SOURCE_FILES})
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    target_link_libraries(stagefuture_test psapi)
endif()