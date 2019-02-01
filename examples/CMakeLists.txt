cmake_minimum_required(VERSION 3.6)
project(example)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
add_compile_options(-Wall -g -pipe -std=c++14)
add_definitions(-D_BOOST_COMP_GNUC)

set(SOURCES
        main.cpp)

include_directories(
        /usr/include/
        /usr/local/include/
        ${CMAKE_SOURCE_DIR}/include/
)


link_directories(
        /usr/lib/
        /usr/local/lib/
        ${CMAKE_SOURCE_DIR}/lib/
)

add_executable(example ${SOURCES})
target_link_libraries(example stagefuture pthread)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/)
