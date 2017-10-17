cmake_minimum_required(VERSION 3.6)
project(threadpool)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -pipe -std=c++14")
set(SOURCE_FILES
        main.cpp
        threadpool.h
        )


link_directories(
        /usr/local/lib/
)

add_executable(threadpool ${SOURCE_FILES})
