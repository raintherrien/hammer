cmake_minimum_required(VERSION 3.13)
project(hammer VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

# Try to compile release
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(deadlock REQUIRED)

# Add warnings
# Note: add_compile_options propegates these options to e.g. examples.
set(HAMMER_MSVC_WARNFLAGS /W4 /WX)
set(HAMMER_OTHR_WARNFLAGS -Wall -Werror -Wextra -Wno-date-time -Wno-c++98-compat -Wno-implicit-fallthrough)
add_compile_options("$<IF:$<C_COMPILER_ID:MSVC>,${HAMMER_MSVC_WARNFLAGS},${HAMMER_OTHR_WARNFLAGS}>")

set(HAMMER_SOURCES ${PROJECT_SOURCE_DIR}/src/cli.c
                   ${PROJECT_SOURCE_DIR}/src/error.c
                   ${PROJECT_SOURCE_DIR}/src/salloc.c
                   ${PROJECT_SOURCE_DIR}/src/main.c
                   ${PROJECT_SOURCE_DIR}/src/mem.c)
add_executable(hammer ${HAMMER_SOURCES})
target_include_directories(hammer PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hammer PRIVATE deadlock)

# Disable warnings about fopen in VS2019
if(WIN32)
	target_compile_definitions(hammer PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

option(HAMMER_BUILD_SAN "Build with sanitizers" OFF)
if(HAMMER_BUILD_SAN)
	target_compile_options(hammer PUBLIC -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
	target_link_options   (hammer PUBLIC -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
endif()

option(HAMMER_BUILD_TUNE "Build for this architecture" OFF)
if(HAMMER_BUILD_TUNE)
	target_compile_options(hammer PUBLIC -march=native)
endif()
