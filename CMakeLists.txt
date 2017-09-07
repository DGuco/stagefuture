cmake_minimum_required(VERSION 3.6)
project(mythread)
add_definitions(-D_DEBUG_)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -pipe -std=c++11")
set(SOURCE_FILES
        main.cpp
        concurrent/inc/runnable.h
        concurrent/inc/executor.h
        concurrent/inc/thread.h
        concurrent/inc/executor_service.h
        concurrent/src/thread.cpp
        concurrent/src/executor_service.cpp

        )


link_directories(
        /usr/local/lib/
)

add_executable(mythread ${SOURCE_FILES})
target_link_libraries(mythread)
