cmake_minimum_required(VERSION 3.6)
project(mythread)
add_definitions(-D_DEBUG_ -DUSE_LOG4CPP -DUSE_MY_ASSERT)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -pipe -std=c++11")
set(SOURCE_FILES
        main.cpp
        thread/inc/thread.h
        thread/src/thread.cpp
        concurrent/inc/executor_service.h
        concurrent/src/executor_service.cpp
        )


link_directories(
        /usr/local/lib/
)

add_executable(mythread ${SOURCE_FILES})
target_link_libraries(mythread)
set(EXECUTABgit LE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/)
