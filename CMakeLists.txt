# Copyright (c) 2015 Amanieu d'Antras
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

cmake_minimum_required(VERSION 3.1)
project(stagefuture C CXX)

option(BUILD_SHARED_LIBS "Build stagefuture as a shared library" ON)
option(USE_CXX_EXCEPTIONS "Enable C++ exception support" ON)
if (APPLE)
	option(BUILD_FRAMEWORK "Build a Mac OS X framework instead of a library" OFF)
	if (BUILD_FRAMEWORK AND NOT BUILD_SHARED_LIBS)
		message(FATAL_ERROR "Can't build a framework with static libraries")
	endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Add all source and header files so IDEs can see them
set(STAGE_FUTURE_INCLUDE
	${PROJECT_SOURCE_DIR}/include/stagefuture/aligned_alloc.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/cancel.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/continuation_vector.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/parallel_for.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/parallel_invoke.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/parallel_reduce.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/partitioner.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/range.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/ref_count.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/scheduler.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/scheduler_fwd.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/task.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/task_base.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/traits.h
	${PROJECT_SOURCE_DIR}/include/stagefuture/when_all_any.h
)
set(STAGE_FUTURE_SRC
	${PROJECT_SOURCE_DIR}/src/internal.h
	${PROJECT_SOURCE_DIR}/src/fifo_queue.h
	${PROJECT_SOURCE_DIR}/src/scheduler.cpp
	${PROJECT_SOURCE_DIR}/src/singleton.h
	${PROJECT_SOURCE_DIR}/src/task_wait_event.h
	${PROJECT_SOURCE_DIR}/src/threadpool_scheduler.cpp
	${PROJECT_SOURCE_DIR}/src/work_steal_queue.h
)
source_group(include FILES ${PROJECT_SOURCE_DIR}/include/stagefuture.h ${STAGE_FUTURE_INCLUDE})
source_group(src FILES ${STAGE_FUTURE_SRC})
add_library(stagefuture ${PROJECT_SOURCE_DIR}/include/stagefuture.h ${STAGE_FUTURE_INCLUDE} ${STAGE_FUTURE_SRC})

# stagefuture only depends on the C++11 standard libraries, but some implementations
# require the -pthread compiler flag to enable threading functionality.
if (NOT MSVC)
	target_compile_options(stagefuture PRIVATE -std=c++11)
endif()
if (APPLE)
	# Use libc++ on Mac because the shipped libstdc++ version is ancient
	target_compile_options(stagefuture PRIVATE -stdlib=libc++)
	set_target_properties(stagefuture PROPERTIES LINK_FLAGS -stdlib=libc++)
endif()
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(stagefuture PUBLIC Threads::Threads)

# Set up preprocessor definitions
target_include_directories(stagefuture PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(stagefuture PROPERTIES DEFINE_SYMBOL LIBASYNC_BUILD)
if (BUILD_SHARED_LIBS)
	# Minimize the set of symbols exported by libraries
	set_target_properties(stagefuture PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
else()
	target_compile_definitions(stagefuture PUBLIC LIBASYNC_STATIC)
endif()

# Enable warnings for strict C++ standard conformance
if (NOT MSVC)
	target_compile_options(stagefuture PRIVATE -Wall -Wextra -pedantic)
endif()

# stagefuture doesn't make use of RTTI information, so don't generate it
if (MSVC)
	target_compile_options(stagefuture PRIVATE /GR-)
else()
	target_compile_options(stagefuture PRIVATE -fno-rtti)
endif()

# Allow disabling exceptions, but warn the user about the consequences
if (NOT USE_CXX_EXCEPTIONS)
	message(WARNING "Exceptions have been disabled. Any operation that would "
	        "throw an exception will result in a call to std::abort() instead.")
	target_compile_definitions(stagefuture PUBLIC LIBASYNC_NO_EXCEPTIONS)
	if (MSVC)
		target_compile_options(stagefuture PUBLIC /EHs-c-)
	else()
		target_compile_options(stagefuture PUBLIC -fno-exceptions)
	endif()
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/stagefuture.in"
	"${PROJECT_BINARY_DIR}/stagefutureConfig.cmake"
	INSTALL_DESTINATION cmake
)

install(FILES "${PROJECT_BINARY_DIR}/stagefutureConfig.cmake"
	DESTINATION cmake
)

# Install the library and produce a CMake export script
install(TARGETS stagefuture
	EXPORT stagefuture
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	FRAMEWORK DESTINATION Frameworks
)
export(EXPORT stagefuture)
install(EXPORT stagefuture DESTINATION cmake)
if (APPLE AND BUILD_FRAMEWORK)
	set_target_properties(stagefuture PROPERTIES OUTPUT_NAME stagefuture FRAMEWORK ON)
	set_source_files_properties(${STAGE_FUTURE_INCLUDE} PROPERTIES MACOSX_PACKAGE_LOCATION Headers/stagefuture)
	set_source_files_properties(${PROJECT_SOURCE_DIR}/include/stagefuture.h PROPERTIES MACOSX_PACKAGE_LOCATION Headers)
else()
	set_target_properties(stagefuture PROPERTIES OUTPUT_NAME stagefuture)
	target_include_directories(stagefuture INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
	install(FILES ${PROJECT_SOURCE_DIR}/include/stagefuture.h DESTINATION include)
	install(FILES ${STAGE_FUTURE_INCLUDE} DESTINATION include/stagefuture)
endif()

SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "none") #required

INCLUDE(CPack)
